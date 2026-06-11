#include "FileItemLayout.hpp"

#include <unordered_map>

static std::string getIconName(FileItem::FileItemType type)
{
    switch (type)
    {
    case FileItem::FileItemType::Directory:
        return "folder";
    case FileItem::FileItemType::Image:
        return "image-x-generic";
    case FileItem::FileItemType::Video:
        return "video-x-generic";
    case FileItem::FileItemType::Text:
        return "text-x-generic";
    case FileItem::FileItemType::Binary:
    default:
        return "application-x-executable";
    }
}

static SP<ISystemIconDescription> getCachedIconDesc(SP<IBackend> backend, FileItem::FileItemType type)
{
    static std::unordered_map<FileItem::FileItemType, SP<ISystemIconDescription>> cache;

    auto it = cache.find(type);
    if (it != cache.end())
        return it->second;

    auto factory = backend->systemIcons();
    SP<ISystemIconDescription> desc = factory ? factory->lookupIcon(getIconName(type)) : nullptr;
    cache[type] = desc;
    return desc;
}

CHyprColor fileItemMultiSelectionIndicatorColor(SP<IBackend> backend, bool selected, bool multiSelected)
{
    if (!multiSelected)
        return CHyprColor(0, 0, 0, 0);
    if (selected)
        return backend->getPalette()->m_colors.text;
    return backend->getPalette()->m_colors.accent;
}

FileItemLayout::FileItemLayout(SP<IBackend> backend, SP<FileItem> item)
    : backend_(backend), item_(item),
      background_(CRectangleBuilder::begin()
                      ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_ABSOLUTE, {1.F, 20.F}})
                      ->color([this]
                              { return backend_->getPalette()->m_colors.background; })
                      ->rounding(10)
                      ->commence()),
      multiSelectionIndicator_(CRectangleBuilder::begin()
                                    ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_PERCENT, {3.F, 1.F}})
                                    ->color([this]
                                            { return fileItemMultiSelectionIndicatorColor(backend_, selected_, multiSelected_); })
                                    ->commence()),
      contentLayout_(CRowLayoutBuilder::begin()
                          ->gap(4)
                          ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                          ->commence()),
      iconElement_([this]() -> SP<CImageElement>
                   {
              auto iconDesc = getCachedIconDesc(backend_, item_->getPreviewType());
              if (!iconDesc || !iconDesc->exists())
                  return nullptr;

              return CImageBuilder::begin()
                  ->icon(iconDesc)
                  ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, {16.F, 16.F}})
                  ->commence(); }()),
      textElement_(buildTextElement())
{
    if (iconElement_)
    {
        iconPadding_ = CColumnLayoutBuilder::begin()
                           ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_PERCENT, {6.F, 1.F}})
                           ->commence();
        contentLayout_->addChild(iconPadding_);
        contentLayout_->addChild(iconElement_);
    }

    contentLayout_->addChild(textElement_);
    multiSelectionIndicator_->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
    background_->addChild(multiSelectionIndicator_);
    background_->addChild(contentLayout_);
}

void FileItemLayout::setSelected(bool selected)
{
    if (selected_ == selected)
        return;

    selected_ = selected;
    applySelectionStyle();
}

void FileItemLayout::setMultiSelected(bool multiSelected)
{
    if (multiSelected_ == multiSelected)
        return;

    multiSelected_ = multiSelected;
    applyMultiSelectionStyle();
}

void FileItemLayout::setSearchMatched(bool matched)
{
    if (searchMatched_ == matched)
        return;

    searchMatched_ = matched;
    applySelectionStyle();
}

void FileItemLayout::rebind(SP<FileItem> newItem, bool selected, bool multiSelected)
{
    selected_ = selected;
    multiSelected_ = multiSelected;

    const auto oldType = item_->getPreviewType();
    item_ = newItem;
    applySelectionStyle();
    applyMultiSelectionStyle();

    // Rebuild icon if file type changed
    const auto newType = item_->getPreviewType();
    if (newType != oldType)
    {
        // Remove old icon elements
        if (iconElement_)
        {
            contentLayout_->removeChild(iconElement_);
            iconElement_ = nullptr;
        }
        if (iconPadding_)
        {
            contentLayout_->removeChild(iconPadding_);
            iconPadding_ = nullptr;
        }

        // Create new icon
        auto iconDesc = getCachedIconDesc(backend_, newType);
        if (iconDesc && iconDesc->exists())
        {
            iconElement_ = CImageBuilder::begin()
                               ->icon(iconDesc)
                               ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, {16.F, 16.F}})
                               ->commence();
            iconPadding_ = CColumnLayoutBuilder::begin()
                               ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_PERCENT, {6.F, 1.F}})
                               ->commence();

            // Re-add text last: remove, add icon+padding, add text
            contentLayout_->removeChild(textElement_);
            contentLayout_->addChild(iconPadding_);
            contentLayout_->addChild(iconElement_);
            contentLayout_->addChild(textElement_);
        }
    }
}

void FileItemLayout::applySelectionStyle()
{
    // Rebuild text element (color depends on selected_ state)
    contentLayout_->removeChild(textElement_);
    textElement_ = buildTextElement();
    contentLayout_->addChild(textElement_);

    // Rebuild background with correct selection color
    const auto backgroundColor = selected_ ? backend_->getPalette()->m_colors.accent
                                           : backend_->getPalette()->m_colors.background;
    background_->rebuild()
        ->color([backgroundColor]
                { return backgroundColor; })
        ->rounding(10)
        ->commence();
    applyMultiSelectionStyle();
}

void FileItemLayout::applyMultiSelectionStyle()
{
    multiSelectionIndicator_->rebuild()
        ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_PERCENT, {3.F, 1.F}})
        ->color([this]
                { return fileItemMultiSelectionIndicatorColor(backend_, selected_, multiSelected_); })
        ->commence();
}

CSharedPointer<CTextElement> FileItemLayout::buildTextElement() const
{
    return CTextBuilder::begin()
        ->text(item_->getPath().filename().string())
        ->fontSize({Hyprtoolkit::CFontSize::HT_FONT_TEXT})
        ->align(Hyprtoolkit::HT_FONT_ALIGN_LEFT)
        ->color([this]() -> CHyprColor
                {
                    if (selected_)
                        return backend_->getPalette()->m_colors.background;
                    if (searchMatched_)
                        return backend_->getPalette()->m_colors.accent;
                    return backend_->getPalette()->m_colors.text;
                })
        ->commence();
}
