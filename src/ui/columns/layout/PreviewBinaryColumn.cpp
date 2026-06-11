#include "PreviewBinaryColumn.hpp"

PreviewBinaryColumn::PreviewBinaryColumn(CSharedPointer<IBackend> backend, std::filesystem::path path, float widthPercent)
    : BaseLayoutColumn(backend, std::move(path), widthPercent)
{
}

void PreviewBinaryColumn::draw()
{
    const auto backend = backend_;

    auto text = CTextBuilder::begin()
                    ->text("This file is a binary executable, or it could not be previewed")
                    ->align(Hyprtoolkit::HT_FONT_ALIGN_CENTER)
                    ->color([backend]() -> Hyprtoolkit::CHyprColor
                            { return backend->getPalette()->m_colors.text; })
                    ->commence();

    layout_->addChild(text);
}
