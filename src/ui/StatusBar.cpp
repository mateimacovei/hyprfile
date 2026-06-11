#include "StatusBar.hpp"
#include "StatusBarBase.hpp"

#include <filesystem>
#include <system_error>

// Determine the ls -l type character for a path
static char fileTypeChar(const std::filesystem::path &path)
{
    std::error_code ec;
    auto st = std::filesystem::symlink_status(path, ec);
    if (ec)
        return '?';
    switch (st.type())
    {
    case std::filesystem::file_type::regular:
        return '-';
    case std::filesystem::file_type::directory:
        return 'd';
    case std::filesystem::file_type::symlink:
        return 'l';
    case std::filesystem::file_type::block:
        return 'b';
    case std::filesystem::file_type::character:
        return 'c';
    case std::filesystem::file_type::fifo:
        return 'p';
    case std::filesystem::file_type::socket:
        return 's';
    default:
        return '?';
    }
}

// Build a 10-character permissions string matching ls -l (e.g. "drwxr-xr-x")
static std::string permsToString(const std::filesystem::path &path, std::filesystem::perms p)
{
    using P = std::filesystem::perms;

    // one entry per rwx bit: {bit, char_if_set, char_if_clear}
    static const struct
    {
        P bit;
        char yes, no;
    } table[] = {
        {P::owner_read, 'r', '-'},
        {P::owner_write, 'w', '-'},
        {P::owner_exec, 'x', '-'},
        {P::group_read, 'r', '-'},
        {P::group_write, 'w', '-'},
        {P::group_exec, 'x', '-'},
        {P::others_read, 'r', '-'},
        {P::others_write, 'w', '-'},
        {P::others_exec, 'x', '-'},
    };

    std::string s(10, '-');
    s[0] = fileTypeChar(path);
    for (int i = 0; i < 9; ++i)
        s[i + 1] = (p & table[i].bit) != P::none ? table[i].yes : table[i].no;

    // setuid/setgid/sticky override the exec character at positions 3, 6, 9
    if ((p & P::set_uid) != P::none)
        s[3] = s[3] == 'x' ? 's' : 'S';
    if ((p & P::set_gid) != P::none)
        s[6] = s[6] == 'x' ? 's' : 'S';
    if ((p & P::sticky_bit) != P::none)
        s[9] = s[9] == 'x' ? 't' : 'T';
    return s;
}

// Return a color for each permissions character based on position and value
static CHyprColor colorForPermChar(int pos, char ch)
{
    const CHyprColor DIM = CHyprColor(0.40, 0.40, 0.40, 1.0);
    const CHyprColor BLUE = CHyprColor(0.45, 0.65, 1.00, 1.0);   // type d
    const CHyprColor CYAN = CHyprColor(0.35, 0.90, 0.90, 1.0);   // type l
    const CHyprColor YELLOW = CHyprColor(0.95, 0.80, 0.25, 1.0); // r bits
    const CHyprColor RED = CHyprColor(0.95, 0.40, 0.25, 1.0);    // w bits
    const CHyprColor GREEN = CHyprColor(0.35, 0.88, 0.40, 1.0);  // x/s/t bits

    if (pos == 0) // file type
    {
        if (ch == 'd')
            return BLUE;
        if (ch == 'l')
            return CYAN;
        if (ch == '-')
            return DIM;
        return CHyprColor(0.75, 0.55, 0.95, 1.0); // other types: purple
    }

    // r bits: positions 1, 4, 7
    if (pos == 1 || pos == 4 || pos == 7)
        return (ch == 'r') ? YELLOW : DIM;

    // w bits: positions 2, 5, 8
    if (pos == 2 || pos == 5 || pos == 8)
        return (ch == 'w') ? RED : DIM;

    // x/s/t bits: positions 3, 6, 9
    return (ch != '-') ? GREEN : DIM;
}

std::string formatItemCountText(int count)
{
    return std::to_string(count) + (count == 1 ? " item" : " items");
}

StatusBar::StatusBar(SP<IBackend> backend)
    : StatusBarBase(backend, StatusBarBase::makeLayout()),
      countText_(CTextBuilder::begin()
                     ->text(std::string{})
                     ->fontSize({CFontSize::HT_FONT_TEXT})
                     ->align(Hyprtoolkit::HT_FONT_ALIGN_LEFT)
                     ->color([backend]
                             { return backend->getPalette()->m_colors.text; })
                     ->commence()),
      directoryCountText_(CTextBuilder::begin()
                              ->text(" " + formatItemCountText(0) + " ")
                              ->fontSize({CFontSize::HT_FONT_TEXT})
                              ->align(Hyprtoolkit::HT_FONT_ALIGN_RIGHT)
                              ->color([backend]
                                      { return backend->getPalette()->m_colors.text.darken(0.25); })
                              ->commence()),
      spacer_(CRectangleBuilder::begin()
                  ->color([]
                          { return CHyprColor(0, 0, 0, 0); })
                  ->size({CDynamicSize::HT_SIZE_AUTO, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                  ->commence())
{
    layout_->addChild(countText_);
    countText_->setGrow(true, false);
    spacer_->setGrow(true);
    layout_->addChild(spacer_);
    layout_->addChild(directoryCountText_);
}

void StatusBar::setTransientText(std::string text)
{
    countText_->rebuild()->text(std::move(text))->commence();
    layout_->forceReposition();
}

void StatusBar::clearPermChars()
{
    for (auto &el : permChars_)
        layout_->removeChild(el);
    permChars_.clear();
}

void StatusBar::update(WP<FileItem> item)
{
    clearPermChars();

    SP<FileItem> spItem = item.lock();
    if (!spItem)
        return;

    const auto perms = spItem->getPermissions();
    const std::string permStr = permsToString(spItem->getPath(), perms);

    for (int i = 0; i < 10; ++i)
    {
        const char ch = permStr[i];
        const CHyprColor color = colorForPermChar(i, ch);
        auto el = CTextBuilder::begin()
                      ->text(std::string(1, ch))
                      ->fontSize({CFontSize::HT_FONT_TEXT})
                      ->align(Hyprtoolkit::HT_FONT_ALIGN_CENTER)
                      ->color([color]
                              { return color; })
                      ->commence();
        permChars_.push_back(el);
        layout_->addChild(el);
    }
}

void StatusBar::setCount(int count)
{
    if (count > 0)
        setTransientText(std::to_string(count));
    else
        setTransientText(std::string{});
}

void StatusBar::setDirectoryItemCount(int count)
{
    directoryCountText_->rebuild()->text(" " + formatItemCountText(count) + " ")->commence();
    layout_->forceReposition();
}

void StatusBar::setSearchQuery(const std::string &query)
{
    setTransientText("/" + query);
}

void StatusBar::clearSearchQuery()
{
    setTransientText(std::string{});
}
