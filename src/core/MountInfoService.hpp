#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <istream>
#include <optional>
#include <string>
#include <vector>

struct MountInfo
{
    std::string displayName;
    std::optional<std::uintmax_t> totalBytes;
    std::optional<std::uintmax_t> usedBytes;
    std::vector<std::filesystem::path> mountPoints;
};

struct MountStorageStats
{
    std::uintmax_t totalBytes = 0;
    std::uintmax_t usedBytes = 0;
};

using MountLabelResolver = std::function<std::optional<std::string>(const std::string& source)>;
using MountStatProvider = std::function<std::optional<MountStorageStats>(const std::filesystem::path& mountPoint)>;

std::string formatStorageBytes(std::uintmax_t bytes);

class MountInfoService
{
public:
    static std::vector<MountInfo> listMounts();
    static std::vector<MountInfo> parseMountInfo(std::istream& input,
                                                 MountLabelResolver labelResolver = {},
                                                 MountStatProvider statProvider = {});
    static std::optional<std::string> resolveBlockDeviceLabel(const std::string& source);
    static std::optional<std::string>
        resolveBlockDeviceLabelFromDirectories(const std::string& source,
                                               const std::vector<std::filesystem::path>& labelDirectories);
    static std::optional<MountStorageStats> statMountPoint(const std::filesystem::path& mountPoint);
};
