#pragma once

#include "StatusBarBase.hpp"
#include "model/FileItemLayout.hpp"
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/types/FontTypes.hpp>
#include <string>
#include <vector>

std::string formatItemCountText(int count);

class StatusBar : public StatusBarBase
{
public:
    StatusBar(CSharedPointer<IBackend> backend);

    void update(WP<FileItem> item);
    void setCount(int count);
    void setDirectoryItemCount(int count);
    void setSearchQuery(const std::string &query);
    void clearSearchQuery();

    void clearPermChars();

private:
    CSharedPointer<CTextElement> countText_;
    CSharedPointer<CTextElement> directoryCountText_;
    CSharedPointer<CRectangleElement> spacer_;
    std::vector<CSharedPointer<CTextElement>> permChars_;
    void setTransientText(std::string text);
};
