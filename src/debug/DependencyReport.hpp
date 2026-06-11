#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Debug
{
    struct SDependencyReport
    {
        std::string text;
        bool allFound = false;
    };

    struct SCheckContext
    {
        std::function<bool(std::string_view)> pkgConfigExists;
        std::function<std::optional<std::filesystem::path>(std::string_view)> findExecutable;
        std::function<std::optional<std::string>(std::string_view)> getEnv;
    };

    SCheckContext makeSystemCheckContext();
    SDependencyReport buildDependencyReport(const SCheckContext &context);
}
