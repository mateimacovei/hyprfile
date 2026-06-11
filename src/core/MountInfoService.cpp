#include "MountInfoService.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include <sys/statvfs.h>

namespace
{
std::vector<std::string> splitWhitespace(const std::string& value)
{
    std::istringstream input(value);
    std::vector<std::string> fields;
    std::string field;
    while (input >> field)
        fields.push_back(field);
    return fields;
}

std::string unescapeMountInfoField(const std::string& value)
{
    std::string result;
    result.reserve(value.size());

    for (std::size_t i = 0; i < value.size();)
    {
        if (value[i] == '\\' && i + 3 < value.size() && value[i + 1] >= '0' && value[i + 1] <= '7' &&
            value[i + 2] >= '0' && value[i + 2] <= '7' && value[i + 3] >= '0' && value[i + 3] <= '7')
        {
            const auto decoded = static_cast<char>(((value[i + 1] - '0') << 6) | ((value[i + 2] - '0') << 3) |
                                                   (value[i + 3] - '0'));
            result.push_back(decoded);
            i += 4;
            continue;
        }

        result.push_back(value[i]);
        ++i;
    }

    return result;
}

int hexValue(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

std::string decodeUdevLabelFilename(const std::string& value)
{
    std::string result;
    result.reserve(value.size());

    for (std::size_t i = 0; i < value.size();)
    {
        if (value[i] == '\\' && i + 3 < value.size() && value[i + 1] == 'x')
        {
            const auto high = hexValue(value[i + 2]);
            const auto low = hexValue(value[i + 3]);
            if (high >= 0 && low >= 0)
            {
                result.push_back(static_cast<char>((high << 4) | low));
                i += 4;
                continue;
            }
        }

        result.push_back(value[i]);
        ++i;
    }

    return result;
}

bool isPseudoFilesystem(const std::string& filesystemType)
{
    static const std::unordered_set<std::string> pseudoFilesystems = {"autofs",     "binfmt_misc", "bpf",
                                                                     "cgroup",     "cgroup2",      "configfs",
                                                                     "debugfs",    "devpts",       "devtmpfs",
                                                                      "efivarfs",   "fuse.portal",  "fusectl",
                                                                      "hugetlbfs",
                                                                      "mqueue",     "proc",         "pstore",
                                                                      "ramfs",      "securityfs",   "sysfs",
                                                                     "tmpfs",      "tracefs"};

    return pseudoFilesystems.contains(filesystemType);
}

bool isDiskLikeMount(const std::string& source, const std::string& filesystemType)
{
    return source.starts_with("/dev/") || source.starts_with("//") || source.find(':') != std::string::npos ||
           filesystemType == "fuseblk" || filesystemType.starts_with("fuse.");
}

bool isNetworkMount(const std::string& source, const std::string& filesystemType)
{
    return source.starts_with("//") || source.find(':') != std::string::npos || filesystemType == "cifs" ||
           filesystemType == "smb3" || filesystemType == "nfs" || filesystemType == "nfs4" ||
           filesystemType == "fuse.sshfs";
}

std::string makeDisplayName(const std::string& source,
                            const std::string& filesystemType,
                            const MountLabelResolver& labelResolver)
{
    if (labelResolver)
    {
        const auto label = labelResolver(source);
        if (label.has_value() && !label->empty())
            return *label;
    }

    if (source.starts_with("/dev/"))
    {
        const auto basename = std::filesystem::path(source).filename().string();
        if (!basename.empty())
            return basename;
    }

    if (!source.empty() && source != "none")
        return source;

    return filesystemType;
}

struct MountInfoGroup
{
    MountInfo mount;
};
}

std::string formatStorageBytes(std::uintmax_t bytes)
{
    static constexpr std::array<const char*, 6> units = {"B", "KB", "MB", "GB", "TB", "PB"};

    if (bytes < 1024)
        return std::to_string(bytes) + " B";

    double value = static_cast<double>(bytes);
    std::size_t unitIndex = 0;
    while (value >= 1024.0 && unitIndex + 1 < units.size())
    {
        value /= 1024.0;
        ++unitIndex;
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value << ' ' << units[unitIndex];
    return out.str();
}

std::vector<MountInfo> MountInfoService::listMounts()
{
    std::ifstream mountInfo("/proc/self/mountinfo");
    if (!mountInfo)
        return {};

    return parseMountInfo(mountInfo, MountInfoService::resolveBlockDeviceLabel, MountInfoService::statMountPoint);
}

std::vector<MountInfo> MountInfoService::parseMountInfo(std::istream& input,
                                                        MountLabelResolver labelResolver,
                                                        MountStatProvider statProvider)
{
    std::vector<MountInfoGroup> groups;
    std::unordered_map<std::string, std::size_t> groupIndexes;

    std::string line;
    while (std::getline(input, line))
    {
        const auto separator = line.find(" - ");
        if (separator == std::string::npos)
            continue;

        const auto preFields = splitWhitespace(line.substr(0, separator));
        const auto postFields = splitWhitespace(line.substr(separator + 3));
        if (preFields.size() <= 4 || postFields.empty())
            continue;

        const auto mountPoint = std::filesystem::path(unescapeMountInfoField(preFields[4]));
        const auto filesystemType = unescapeMountInfoField(postFields[0]);
        const auto source = postFields.size() > 1 ? unescapeMountInfoField(postFields[1]) : std::string();

        if (isPseudoFilesystem(filesystemType) || !isDiskLikeMount(source, filesystemType))
            continue;

        const auto groupKey = source + '\n' + filesystemType;
        auto groupIt = groupIndexes.find(groupKey);
        if (groupIt == groupIndexes.end())
        {
            groupIt = groupIndexes.emplace(groupKey, groups.size()).first;
            groups.push_back(MountInfoGroup{MountInfo{makeDisplayName(source, filesystemType, labelResolver), {}, {}, {}}});
        }

        auto& mount = groups[groupIt->second].mount;
        mount.mountPoints.push_back(mountPoint);

        if (statProvider && !isNetworkMount(source, filesystemType) && !mount.totalBytes.has_value() &&
            !mount.usedBytes.has_value())
        {
            const auto stats = statProvider(mountPoint);
            if (stats.has_value())
            {
                mount.totalBytes = stats->totalBytes;
                mount.usedBytes = stats->usedBytes;
            }
        }
    }

    std::vector<MountInfo> mounts;
    mounts.reserve(groups.size());
    for (auto& group : groups)
        mounts.push_back(std::move(group.mount));

    std::sort(mounts.begin(), mounts.end(), [](const MountInfo& lhs, const MountInfo& rhs) {
        if (lhs.displayName != rhs.displayName)
            return lhs.displayName < rhs.displayName;
        if (lhs.mountPoints.empty() || rhs.mountPoints.empty())
            return lhs.mountPoints.size() < rhs.mountPoints.size();
        return lhs.mountPoints.front() < rhs.mountPoints.front();
    });

    return mounts;
}

std::optional<std::string> MountInfoService::resolveBlockDeviceLabel(const std::string& source)
{
    if (!source.starts_with("/dev/"))
        return std::nullopt;

    static const std::vector<std::filesystem::path> labelDirectories = {"/dev/disk/by-label", "/dev/disk/by-partlabel"};

    return resolveBlockDeviceLabelFromDirectories(source, labelDirectories);
}

std::optional<std::string> MountInfoService::resolveBlockDeviceLabelFromDirectories(
    const std::string& source,
    const std::vector<std::filesystem::path>& labelDirectories)
{
    std::error_code ec;
    const auto canonicalSource = std::filesystem::canonical(source, ec);
    if (ec)
        return std::nullopt;

    for (const auto& labelDirectory : labelDirectories)
    {
        ec.clear();
        if (!std::filesystem::exists(labelDirectory, ec) || ec)
            continue;

        std::filesystem::directory_iterator entries(labelDirectory, ec);
        if (ec)
            continue;

        for (const auto& entry : entries)
        {
            std::error_code targetEc;
            const auto target = std::filesystem::canonical(entry.path(), targetEc);
            if (!targetEc && target == canonicalSource)
                return decodeUdevLabelFilename(entry.path().filename().string());
        }
    }

    return std::nullopt;
}

std::optional<MountStorageStats> MountInfoService::statMountPoint(const std::filesystem::path& mountPoint)
{
    struct statvfs stats
    {
    };

    if (::statvfs(mountPoint.c_str(), &stats) != 0)
        return std::nullopt;

    const auto fragmentSize = stats.f_frsize != 0 ? stats.f_frsize : stats.f_bsize;
    if (fragmentSize == 0)
        return std::nullopt;

    const auto totalBytes = static_cast<std::uintmax_t>(stats.f_blocks) * static_cast<std::uintmax_t>(fragmentSize);
    const auto freeBytes = static_cast<std::uintmax_t>(stats.f_bfree) * static_cast<std::uintmax_t>(fragmentSize);
    const auto usedBytes = totalBytes >= freeBytes ? totalBytes - freeBytes : 0;

    return MountStorageStats{totalBytes, usedBytes};
}
