#include "TopBar.hpp"

std::filesystem::path topBarDisplayPath(const std::filesystem::path &directoryPath,
                                        const std::filesystem::path &selectedItemPath,
                                        bool fullscreen)
{
    return fullscreen ? selectedItemPath : directoryPath;
}

TopBar::TopBar(SP<IBackend> backend, int gap)
    : StatusBarBase(backend, StatusBarBase::makeLayout(gap)),
      textElement_(CTextBuilder::begin()
                       ->text(std::string{})
                       ->align(Hyprtoolkit::HT_FONT_ALIGN_LEFT)
                       ->fontSize({CFontSize::HT_FONT_TEXT})
                       ->color([backend]
                               { return backend->getPalette()->m_colors.text; })
                       ->commence())
{
    layout_->addChild(textElement_);
}

void TopBar::updatePath(const std::filesystem::path &path)
{
    textElement_->rebuild()->text(path.string())->commence();
}
