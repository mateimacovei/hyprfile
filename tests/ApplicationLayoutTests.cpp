#include <gtest/gtest.h>

#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/types/SizeType.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

#include "ui/ApplicationLayout.hpp"

using namespace Hyprtoolkit;
using namespace Hyprutils::Math;
using Hyprutils::Memory::CSharedPointer;

namespace
{
    CSharedPointer<CRowLayoutElement> makeBar()
    {
        return CRowLayoutBuilder::begin()
            ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_ABSOLUTE, {1.F, 30.F}})
            ->commence();
    }

    CSharedPointer<CColumnLayoutElement> makeMainChild()
    {
        return CColumnLayoutBuilder::begin()
            ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
            ->commence();
    }
}

TEST(ApplicationLayoutTests, MainAreaKeepsVisibleHeightWhenPreviewIsRebuiltAfterShrink)
{
    auto applicationLayout = CColumnLayoutBuilder::begin()
                                 ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                                 ->commence();
    auto topBar = makeBar();
    auto mainLayout = hyprfile::UI::makeMainLayout();
    mainLayout->setGrow(true);
    auto statusBar = makeBar();

    mainLayout->addChild(makeMainChild());
    auto previewChild = makeMainChild();
    mainLayout->addChild(previewChild);
    applicationLayout->addChild(topBar);
    applicationLayout->addChild(mainLayout);
    applicationLayout->addChild(statusBar);

    CSharedPointer<IElement> root = applicationLayout;
    root->reposition(CBox(0, 0, 800, 1000));
    root->reposition(CBox(0, 0, 800, 500));

    mainLayout->removeChild(previewChild);
    mainLayout->addChild(makeMainChild());
    root->reposition(CBox(0, 0, 800, 500));

    EXPECT_NEAR(mainLayout->posFromParent().y, 30.F, 0.1F);
    EXPECT_NEAR(mainLayout->size().y, 440.F, 0.1F);
    EXPECT_NEAR(statusBar->posFromParent().y, 470.F, 0.1F);
}
