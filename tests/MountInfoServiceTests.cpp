#include <gtest/gtest.h>

#include "../src/core/MountInfoService.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

namespace
{
class ScopedTestDirectory
{
public:
    explicit ScopedTestDirectory(const std::string& name)
        : path_(std::filesystem::temp_directory_path() / (name + "_" + std::to_string(::getpid())))
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_);
    }

    ~ScopedTestDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void createFile(const std::filesystem::path& path)
{
    std::ofstream file(path);
    ASSERT_TRUE(file.good());
}

void createSymlink(const std::filesystem::path& target, const std::filesystem::path& link)
{
    std::filesystem::create_symlink(target, link);
    ASSERT_TRUE(std::filesystem::exists(link));
}
}

TEST(MountInfoServiceTests, FormatStorageBytesUsesReadableUnits)
{
    EXPECT_EQ(formatStorageBytes(500), "500 B");
    EXPECT_EQ(formatStorageBytes(1536), "1.5 KB");
    EXPECT_EQ(formatStorageBytes(1024ULL * 1024ULL), "1.0 MB");
    EXPECT_EQ(formatStorageBytes(5ULL * 1024ULL * 1024ULL * 1024ULL), "5.0 GB");
}

TEST(MountInfoServiceTests, StatMountPointReadsRootWithoutSudo)
{
    const auto stats = MountInfoService::statMountPoint("/");

    ASSERT_TRUE(stats.has_value());
    EXPECT_GT(stats->totalBytes, 0U);
    EXPECT_LE(stats->usedBytes, stats->totalBytes);
}

TEST(MountInfoServiceTests, ParseMountInfoFiltersPseudoFilesystemsAndGroupsSameSource)
{
    std::istringstream input(
        "26 23 0:22 / /proc rw,nosuid,nodev,noexec,relatime - proc proc rw\n"
        "27 23 259:2 / / rw,relatime - btrfs /dev/nvme0n1p2 rw,compress=zstd,subvol=/\n"
        "28 23 259:2 /@home /home rw,relatime - btrfs /dev/nvme0n1p2 rw,compress=zstd,subvol=/@home\n"
        "29 23 0:45 / /mnt/share rw,relatime - nfs4 server:/export rw\n"
        "30 23 0:46 / /run/user/1000 rw,nosuid,nodev,relatime - tmpfs tmpfs rw\n");

    auto labels = [](const std::string& source) -> std::optional<std::string> {
        if (source == "/dev/nvme0n1p2")
            return "Root Disk";
        return std::nullopt;
    };

    auto stats = [](const std::filesystem::path& path) -> std::optional<MountStorageStats> {
        if (path == "/")
            return MountStorageStats{1000, 250};
        if (path == "/mnt/share")
            return MountStorageStats{2000, 500};
        return std::nullopt;
    };

    const auto mounts = MountInfoService::parseMountInfo(input, labels, stats);

    ASSERT_EQ(mounts.size(), 2);
    EXPECT_EQ(mounts[0].displayName, "Root Disk");
    ASSERT_TRUE(mounts[0].totalBytes.has_value());
    ASSERT_TRUE(mounts[0].usedBytes.has_value());
    EXPECT_EQ(*mounts[0].totalBytes, 1000);
    EXPECT_EQ(*mounts[0].usedBytes, 250);
    ASSERT_EQ(mounts[0].mountPoints.size(), 2);
    EXPECT_EQ(mounts[0].mountPoints[0], std::filesystem::path("/"));
    EXPECT_EQ(mounts[0].mountPoints[1], std::filesystem::path("/home"));

    EXPECT_EQ(mounts[1].displayName, "server:/export");
    ASSERT_EQ(mounts[1].mountPoints.size(), 1);
    EXPECT_EQ(mounts[1].mountPoints[0], std::filesystem::path("/mnt/share"));
}

TEST(MountInfoServiceTests, ParseMountInfoSkipsStorageStatsForNetworkMounts)
{
    std::istringstream input(
        "31 23 8:1 / /mnt/local rw,relatime - ext4 /dev/sda1 rw\n"
        "32 23 0:50 / /mnt/cifs rw,relatime - cifs //server/share rw\n"
        "33 23 0:51 / /mnt/nfs rw,relatime - nfs4 server:/export rw\n");

    std::vector<std::filesystem::path> statRequests;
    auto stats = [&statRequests](const std::filesystem::path& path) -> std::optional<MountStorageStats> {
        statRequests.push_back(path);
        return MountStorageStats{1000, 250};
    };

    const auto mounts = MountInfoService::parseMountInfo(input, {}, stats);

    ASSERT_EQ(mounts.size(), 3);
    ASSERT_EQ(statRequests.size(), 1);
    EXPECT_EQ(statRequests[0], std::filesystem::path("/mnt/local"));

    auto findMount = [&mounts](const std::string& displayName) -> const MountInfo* {
        const auto it = std::find_if(mounts.begin(), mounts.end(), [&displayName](const MountInfo& mount) {
            return mount.displayName == displayName;
        });
        return it == mounts.end() ? nullptr : &*it;
    };

    const auto* local = findMount("sda1");
    ASSERT_NE(local, nullptr);
    EXPECT_EQ(local->totalBytes, 1000);
    EXPECT_EQ(local->usedBytes, 250);

    const auto* cifs = findMount("//server/share");
    ASSERT_NE(cifs, nullptr);
    EXPECT_FALSE(cifs->totalBytes.has_value());
    EXPECT_FALSE(cifs->usedBytes.has_value());

    const auto* nfs = findMount("server:/export");
    ASSERT_NE(nfs, nullptr);
    EXPECT_FALSE(nfs->totalBytes.has_value());
    EXPECT_FALSE(nfs->usedBytes.has_value());
}

TEST(MountInfoServiceTests, ParseMountInfoUsesDisplayNameFallbacks)
{
    std::istringstream input(
        "31 23 8:1 / /mnt/plain rw,relatime - ext4 /dev/sda1 rw\n"
        "32 23 0:50 / /mnt/remote rw,relatime - cifs //server/share rw\n"
        "33 23 0:51 / /mnt/fuse rw,relatime - fuse.sshfs none rw\n"
        "34 23 0:52 / /mnt/fuseblk rw,relatime - fuseblk none rw\n");

    const auto mounts = MountInfoService::parseMountInfo(input, {}, {});

    std::vector<std::string> displayNames;
    for (const auto& mount : mounts)
        displayNames.push_back(mount.displayName);

    ASSERT_EQ(displayNames.size(), 4);
    EXPECT_TRUE(std::find(displayNames.begin(), displayNames.end(), "sda1") != displayNames.end());
    EXPECT_TRUE(std::find(displayNames.begin(), displayNames.end(), "//server/share") != displayNames.end());
    EXPECT_TRUE(std::find(displayNames.begin(), displayNames.end(), "fuse.sshfs") != displayNames.end());
    EXPECT_TRUE(std::find(displayNames.begin(), displayNames.end(), "fuseblk") != displayNames.end());
}

TEST(MountInfoServiceTests, ParseMountInfoExcludesRuntimeFusePortal)
{
    std::istringstream input(
        "50 23 0:60 / /run/user/1000/doc rw,nosuid,nodev,relatime - fuse.portal portal rw\n");

    const auto mounts = MountInfoService::parseMountInfo(input, {}, {});

    EXPECT_TRUE(mounts.empty());
}

TEST(MountInfoServiceTests, ParseMountInfoUsesResolvedLabelsBeforeDeviceNames)
{
    std::istringstream input(
        "35 23 8:3 / /mnt/photos rw,relatime - ext4 /dev/sdb1 rw\n"
        "36 23 8:4 / /mnt/archive rw,relatime - ext4 /dev/sdc1 rw\n");

    auto labels = [](const std::string& source) -> std::optional<std::string> {
        if (source == "/dev/sdb1")
            return "Photos";
        if (source == "/dev/sdc1")
            return "Archive Partition";
        return std::nullopt;
    };

    const auto mounts = MountInfoService::parseMountInfo(input, labels, {});

    std::vector<std::string> displayNames;
    for (const auto& mount : mounts)
        displayNames.push_back(mount.displayName);

    ASSERT_EQ(displayNames.size(), 2);
    EXPECT_TRUE(std::find(displayNames.begin(), displayNames.end(), "Photos") != displayNames.end());
    EXPECT_TRUE(std::find(displayNames.begin(), displayNames.end(), "Archive Partition") != displayNames.end());
    EXPECT_TRUE(std::find(displayNames.begin(), displayNames.end(), "sdb1") == displayNames.end());
    EXPECT_TRUE(std::find(displayNames.begin(), displayNames.end(), "sdc1") == displayNames.end());
}

TEST(MountInfoServiceTests, ParseMountInfoUnescapesMountPointsAndSources)
{
    std::istringstream input(
        "34 23 8:2 / /mnt/My\\040Disk rw,relatime - ext4 /dev/disk\\040source rw\n");

    const auto mounts = MountInfoService::parseMountInfo(input, {}, {});
    ASSERT_EQ(mounts.size(), 1);
    EXPECT_EQ(mounts[0].displayName, "disk source");
    ASSERT_EQ(mounts[0].mountPoints.size(), 1);
    EXPECT_EQ(mounts[0].mountPoints[0], std::filesystem::path("/mnt/My Disk"));
}

TEST(MountInfoServiceTests, ParseMountInfoUsesLaterMountPointWhenStatsFail)
{
    std::istringstream input(
        "40 23 8:1 / /mnt/a rw,relatime - ext4 /dev/sda1 rw\n"
        "41 23 8:1 /sub /mnt/b rw,relatime - ext4 /dev/sda1 rw\n");

    auto stats = [](const std::filesystem::path& path) -> std::optional<MountStorageStats> {
        if (path == "/mnt/b")
            return MountStorageStats{4096, 1024};
        return std::nullopt;
    };

    const auto mounts = MountInfoService::parseMountInfo(input, {}, stats);
    ASSERT_EQ(mounts.size(), 1);
    ASSERT_TRUE(mounts[0].totalBytes.has_value());
    ASSERT_TRUE(mounts[0].usedBytes.has_value());
    EXPECT_EQ(*mounts[0].totalBytes, 4096);
    EXPECT_EQ(*mounts[0].usedBytes, 1024);
}

TEST(MountInfoServiceTests, ResolveBlockDeviceLabelFromDirectoriesPrefersByLabelOverByPartlabel)
{
    ScopedTestDirectory temp("hyprfile_mount_label_priority");
    const auto byLabel = temp.path() / "by-label";
    const auto byPartlabel = temp.path() / "by-partlabel";
    std::filesystem::create_directories(byLabel);
    std::filesystem::create_directories(byPartlabel);

    const auto source = temp.path() / "source";
    createFile(source);
    createSymlink(source, byLabel / "FilesystemLabel");
    createSymlink(source, byPartlabel / "PartitionLabel");

    const auto label = MountInfoService::resolveBlockDeviceLabelFromDirectories(source.string(), {byLabel, byPartlabel});

    ASSERT_TRUE(label.has_value());
    EXPECT_EQ(*label, "FilesystemLabel");
}

TEST(MountInfoServiceTests, ResolveBlockDeviceLabelFromDirectoriesUsesByPartlabelWhenByLabelDoesNotMatch)
{
    ScopedTestDirectory temp("hyprfile_mount_label_partlabel");
    const auto byLabel = temp.path() / "by-label";
    const auto byPartlabel = temp.path() / "by-partlabel";
    std::filesystem::create_directories(byLabel);
    std::filesystem::create_directories(byPartlabel);

    const auto source = temp.path() / "source";
    const auto otherSource = temp.path() / "other";
    createFile(source);
    createFile(otherSource);
    createSymlink(otherSource, byLabel / "OtherDisk");
    createSymlink(source, byPartlabel / "PartitionLabel");

    const auto label = MountInfoService::resolveBlockDeviceLabelFromDirectories(source.string(), {byLabel, byPartlabel});

    ASSERT_TRUE(label.has_value());
    EXPECT_EQ(*label, "PartitionLabel");
}

TEST(MountInfoServiceTests, ResolveBlockDeviceLabelFromDirectoriesMatchesCanonicalSourceSymlinks)
{
    ScopedTestDirectory temp("hyprfile_mount_label_canonical");
    const auto byLabel = temp.path() / "by-label";
    std::filesystem::create_directories(byLabel);

    const auto realSource = temp.path() / "real-source";
    const auto sourceSymlink = temp.path() / "source-link";
    createFile(realSource);
    createSymlink(realSource, sourceSymlink);
    createSymlink(realSource, byLabel / "CanonicalDisk");

    const auto label = MountInfoService::resolveBlockDeviceLabelFromDirectories(sourceSymlink.string(), {byLabel});

    ASSERT_TRUE(label.has_value());
    EXPECT_EQ(*label, "CanonicalDisk");
}

TEST(MountInfoServiceTests, ResolveBlockDeviceLabelFromDirectoriesDecodesUdevHexEscapes)
{
    ScopedTestDirectory temp("hyprfile_mount_label_escaped");
    const auto byLabel = temp.path() / "by-label";
    std::filesystem::create_directories(byLabel);

    const auto source = temp.path() / "source";
    createFile(source);
    createSymlink(source, byLabel / "My\\x20Disk");

    const auto label = MountInfoService::resolveBlockDeviceLabelFromDirectories(source.string(), {byLabel});

    ASSERT_TRUE(label.has_value());
    EXPECT_EQ(*label, "My Disk");
}

TEST(MountInfoServiceTests, ResolveBlockDeviceLabelFromDirectoriesLeavesOrdinaryNamesUnchanged)
{
    ScopedTestDirectory temp("hyprfile_mount_label_plain");
    const auto byLabel = temp.path() / "by-label";
    std::filesystem::create_directories(byLabel);

    const auto source = temp.path() / "source";
    createFile(source);
    createSymlink(source, byLabel / "PlainDisk");

    const auto label = MountInfoService::resolveBlockDeviceLabelFromDirectories(source.string(), {byLabel});

    ASSERT_TRUE(label.has_value());
    EXPECT_EQ(*label, "PlainDisk");
}
