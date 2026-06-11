#include <gtest/gtest.h>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/element/Element.hpp>
#include <hyprutils/cli/Logger.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/signal/Signal.hpp>
#include <xkbcommon/xkbcommon.h>

#include <string>
#include <utility>
#include <vector>

#include "core/KeyBindings.hpp"
#include "ui/HelpPopup.hpp"

namespace
{
    struct BackendGuard
    {
        BackendGuard()
        {
            logger = Hyprutils::Memory::makeShared<Hyprutils::CLI::CLogger>();
            logger->setEnableStdout(false);
            logger->setLogLevel(Hyprutils::CLI::LOG_WARN);

            loggerConn = Hyprutils::Memory::makeShared<Hyprutils::CLI::CLoggerConnection>(*logger);
            loggerConn->setLogLevel(Hyprutils::CLI::LOG_WARN);

            Hyprtoolkit::IBackend::SBackendCreationData backendData;
            backendData.pLogConnection = loggerConn;

            backend = Hyprtoolkit::IBackend::createWithData(backendData);
        }

        ~BackendGuard()
        {
            if (backend)
                backend->destroy();
        }

        Hyprutils::Memory::CSharedPointer<Hyprutils::CLI::CLogger> logger;
        Hyprutils::Memory::CSharedPointer<Hyprutils::CLI::CLoggerConnection> loggerConn;
        Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> backend;
    };
}

TEST(HelpPopupTests, BodyRowsAndFooterRemainPositionedInsidePopup)
{
    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    std::vector<std::pair<std::string, std::string>> help;
    for (int i = 0; i < 24; ++i)
        help.emplace_back("key" + std::to_string(i), "Description " + std::to_string(i));

    const auto layout = hyprfile::UI::makeHelpPopupContent(backend.backend, help);
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement> root = layout.root;

    root->reposition(Hyprutils::Math::CBox(0, 0, 620, 540));

    ASSERT_EQ(layout.rows.size(), help.size());
    EXPECT_GT(layout.body->size().y, 0.F);
    EXPECT_LT(layout.footer->posFromParent().y, 540.F);

    for (const auto &row : layout.rows)
    {
        EXPECT_GT(row->size().x, 0.F);
        EXPECT_LT(row->posFromParent().y, 540.F);
    }
}

TEST(HelpPopupTests, IntroDescribesKeyboardOnlyInteraction)
{
    EXPECT_EQ(hyprfile::UI::helpPopupIntroText(), "Keyboard-only file manager");
}

TEST(HelpPopupTests, CloseKeyListenerHandlesPopupKeyboardEvents)
{
    CKeyBindings bindings;
    Hyprutils::Signal::CSignalT<Hyprtoolkit::Input::SKeyboardKeyEvent> keyboardKey;
    int closeCount = 0;

    hyprfile::UI::bindHelpPopupCloseKeys(keyboardKey, bindings, [&]
                                          { ++closeCount; });

    Hyprtoolkit::Input::SKeyboardKeyEvent ignored;
    ignored.xkbKeysym = XKB_KEY_j;
    keyboardKey.emit(ignored);
    EXPECT_EQ(closeCount, 0);

    Hyprtoolkit::Input::SKeyboardKeyEvent keyUp;
    keyUp.xkbKeysym = XKB_KEY_Escape;
    keyUp.down = false;
    keyboardKey.emit(keyUp);
    EXPECT_EQ(closeCount, 0);

    Hyprtoolkit::Input::SKeyboardKeyEvent repeat;
    repeat.xkbKeysym = XKB_KEY_Escape;
    repeat.repeat = true;
    keyboardKey.emit(repeat);
    EXPECT_EQ(closeCount, 0);

    Hyprtoolkit::Input::SKeyboardKeyEvent escape;
    escape.xkbKeysym = XKB_KEY_Escape;
    keyboardKey.emit(escape);
    EXPECT_EQ(closeCount, 1);

    Hyprtoolkit::Input::SKeyboardKeyEvent q;
    q.xkbKeysym = XKB_KEY_q;
    keyboardKey.emit(q);
    EXPECT_EQ(closeCount, 2);

    Hyprtoolkit::Input::SKeyboardKeyEvent help;
    help.xkbKeysym = XKB_KEY_H;
    keyboardKey.emit(help);
    EXPECT_EQ(closeCount, 3);
}
