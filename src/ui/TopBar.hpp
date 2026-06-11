#pragma once

#include "StatusBarBase.hpp"
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/types/FontTypes.hpp>

#include <filesystem>

std::filesystem::path topBarDisplayPath(const std::filesystem::path &directoryPath,
                                        const std::filesystem::path &selectedItemPath,
                                        bool fullscreen);

class TopBar : public StatusBarBase
{
public:
    TopBar(SP<IBackend> backend, int gap);

    void updatePath(const std::filesystem::path &path);

private:
    SP<CTextElement> textElement_;
};
