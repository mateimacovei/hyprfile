#pragma once

#include <filesystem>
#include <vector>
#include <string>

class FileSystemService
{
public:
    static FileSystemService& get()
    {
        static FileSystemService instance;
        return instance;
    }

    struct SFileEntry
    {
        std::filesystem::path path;
        std::string name;
        bool isDirectory;
    };

    std::vector<SFileEntry> listDirectory(const std::filesystem::path& dirPath) const;
    bool toggleHiddenFiles();
    bool showHiddenFiles() const;
    void setShowHiddenFiles(bool showHiddenFiles);

    static bool trashWithGio(const std::filesystem::path& absPath);

private:
    FileSystemService() = default;
    ~FileSystemService() = default;
    FileSystemService(const FileSystemService&) = delete;
    FileSystemService& operator=(const FileSystemService&) = delete;

    bool showHiddenFiles_ = true;
};
