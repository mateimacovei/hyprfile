#pragma once

#include <cctype>
#include <cstdint>
#include <string>

#include <hyprtoolkit/core/Input.hpp>
#include <xkbcommon/xkbcommon.h>

namespace SearchInput
{
    inline bool isCancelKeysym(uint32_t keysym)
    {
        return keysym == XKB_KEY_Escape || keysym == XKB_KEY_q;
    }

    inline bool appendKeysym(std::string &buffer, uint32_t keysym, uint32_t normalizedMask)
    {
        constexpr uint32_t nonTextModifiers = Hyprtoolkit::Input::HT_MODIFIER_CTRL |
                                              Hyprtoolkit::Input::HT_MODIFIER_ALT |
                                              Hyprtoolkit::Input::HT_MODIFIER_META;
        if ((normalizedMask & nonTextModifiers) != 0)
            return false;

        char text[8] = {};
        const int bytesIncludingNull = xkb_keysym_to_utf8(keysym, text, sizeof(text));
        if (bytesIncludingNull <= 0 || bytesIncludingNull > static_cast<int>(sizeof(text)))
            return false;

        const int bytes = bytesIncludingNull - 1;
        const bool printableAscii = bytes == 1 && std::isprint(static_cast<unsigned char>(text[0]));
        const bool printableUtf8 = bytes > 1;
        if (!printableAscii && !printableUtf8)
            return false;

        buffer.append(text, static_cast<std::size_t>(bytes));
        return true;
    }

    inline void popLastUtf8Codepoint(std::string &buffer)
    {
        if (buffer.empty())
            return;

        std::size_t start = buffer.size() - 1;
        while (start > 0 && (static_cast<unsigned char>(buffer[start]) & 0xC0) == 0x80)
            --start;
        buffer.erase(start);
    }
}
