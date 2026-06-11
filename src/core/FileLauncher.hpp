// Helper for opening files with the appropriate application.
#pragma once

#include <filesystem>

#include "../ui/model/FileItem.hpp"

class FileLauncher
{
public:
    // Launches the file according to its type. Binary files are executed
    // directly when executable; other files are opened via xdg-open.
    static bool open(const std::filesystem::path &path, FileItem::FileItemType type);
};
