#include "FileItem.hpp"

FileItem::FileItemType FileItem::getPreviewType() const
{
    if (!previewType_.has_value())
        previewType_ = classifyType();
    return previewType_.value();
}

FileItem::FileItemType FileItem::classifyType() const
{
    std::error_code ec;
    if (std::filesystem::is_directory(path_, ec))
        return FileItemType::Directory;

    std::string ext = path_.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    static constexpr std::array<const char *, 6> imageExtensions = {
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp"};
    for (const char *e : imageExtensions)
        if (ext == e)
            return FileItemType::Image;

    static constexpr std::array<const char *, 7> videoExtensions = {
        ".mp4", ".mov", ".mkv", ".webm", ".avi", ".mp3", ".wav"};
    for (const char *e : videoExtensions)
        if (ext == e)
            return FileItemType::Video;

    // Known text extensions — no content sniffing needed
    static constexpr std::array<const char *, 33> textExtensions = {
        ".txt",  ".md",   ".markdown", ".rst",  ".cfg",  ".conf",  ".config",
        ".ini",  ".toml", ".yaml",     ".yml",  ".json", ".jsonc", ".xml",
        ".html", ".htm",  ".css",      ".scss", ".js",   ".ts",   ".jsx",
        ".tsx",  ".py",   ".cpp",      ".cxx",  ".cc",   ".c",    ".hpp",
        ".hxx",  ".h",    ".java",     ".sh",   ".props"};
    for (const char *e : textExtensions)
        if (ext == e)
            return FileItemType::Text;

    // Fall back to content sniffing — but only for regular files.
    // Opening a non-regular file (FIFO, socket, device node) blocks the thread.
    {
        std::error_code ec;
        auto st = std::filesystem::symlink_status(path_, ec);
        if (ec || !std::filesystem::is_regular_file(st))
            return FileItemType::Binary;
    }

    std::ifstream file(path_, std::ios::binary);
    if (!file)
        return FileItemType::Binary;

    constexpr std::streamsize kSampleSize = 4096;
    std::array<char, kSampleSize> buffer{};
    file.read(buffer.data(), kSampleSize);
    const std::streamsize readBytes = file.gcount();
    if (readBytes <= 0)
        return FileItemType::Text;

    std::size_t suspicious = 0;
    for (std::streamsize i = 0; i < readBytes; ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(buffer[i]);
        if (ch == 0)
            return FileItemType::Binary;
        if (ch < 0x20 && ch != 0x09 && ch != 0x0A && ch != 0x0D)
            ++suspicious;
    }

    return (suspicious * 100 < static_cast<std::size_t>(readBytes) * 15)
               ? FileItemType::Text
               : FileItemType::Binary;
}