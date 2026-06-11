#pragma once

#include <filesystem>
#include <optional>
#include <array>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/element/Image.hpp>
#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/system/Icons.hpp>
#include <hyprtoolkit/types/SizeType.hpp>
#include <hyprtoolkit/types/FontTypes.hpp>

#include <hyprutils/memory/SharedPtr.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprtoolkit;

#define SP CSharedPointer
#define WP CWeakPointer
#define UP CUniquePointer

class FileItem
{
public:
    enum class FileItemType
    {
        Directory,
        Image,
        Video,
        Text,
        Binary,
    };

    FileItem(std::filesystem::path path) : path_(std::move(path)) {}

    FileItemType getPreviewType() const;

    const std::filesystem::path &getPath() const
    {
        return path_;
    }

    std::filesystem::perms getPermissions() const
    {
        if (!cachedPerms_.has_value())
        {
            std::error_code ec;
            auto st = std::filesystem::symlink_status(path_, ec);
            cachedPerms_ = ec ? std::filesystem::perms::none : st.permissions();
        }
        return cachedPerms_.value();
    }

    bool isSymlink() const
    {
        if (!cachedIsSymlink_.has_value())
        {
            std::error_code ec;
            auto st = std::filesystem::symlink_status(path_, ec);
            cachedIsSymlink_ = !ec && std::filesystem::is_symlink(st);
        }
        return cachedIsSymlink_.value();
    }

    bool fullscreenSupport() const
    {
        auto type = getPreviewType();
        return type == FileItemType::Image || type == FileItemType::Video;
    }

private:
    std::filesystem::path path_;
    mutable std::optional<FileItemType> previewType_;
    mutable std::optional<std::filesystem::perms> cachedPerms_;
    mutable std::optional<bool> cachedIsSymlink_;

    FileItemType classifyType() const;
};