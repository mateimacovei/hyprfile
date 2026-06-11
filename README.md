# hyprfile

Minimalist, opinionated (read: uncustomizable) Hyprland file manager built with Hyprtoolkit.

## Idea

Tired of tweaking yet another theme manager or config file just for your file manager? I was, so I build this.

It only contains what I actually use in a file explorer, nothing more, nothing less. If you need comprehensive options, this might not be for you.

## Features

- **Three-column layout**: Parent directory (left), current directory (center), preview (right), inspired by yazi.
- **Keyboard-only navigation**: arrow keys or vim navigation (`h`/`j`/`k`/`l`), with count prefixes for repeated movement/search actions.
- **Smart previews**: 
  - Text files: line-based preview
  - Images: full preview with proper scaling, fullscreen mode for immersive viewing
  - Videos: thumbnail preview
  - Directories: nested directory listing
- **Search**: Live search in the current directory with next/previous result navigation.
- **File operations**: Copy, cut, paste, and trash support for selected files/directories. Also works between different windows.
- **System integration**: Open files with system handlers, open text files in `nvim`, open terminals, and spawn another hyprfile window in the current folder.
- **Dependency diagnostics**: `--debug` prints the app version plus a dependency report and exits without launching the UI.

## Dependencies

Build-time requirements:

- CMake 3.10+
- C++23-capable compiler
- pkg-config
- [hyprtoolkit](https://github.com/hyprwm/hyprtoolkit) - UI framework
- [hyprutils](https://github.com/hyprwm/hyprutils) - memory, logging, CLI, and math utilities
- pixman-1 - required by the Hyprtoolkit backend stack
- aquamarine - Hyprland display backend dependency
- libdrm - direct rendering dependency
- xkbcommon - keyboard input/key symbol handling
- FFmpeg libraries: libavformat, libavcodec, libswscale, libavutil - video thumbnail extraction
- gdk-pixbuf-2.0 - image format detection, image dimensions, and PNG encoding for generated thumbnails

Test-only requirement:

- GTest

Runtime integrations used when available:

- `gio trash` - move files to trash
- `xdg-open` - open images, videos, and other non-text files with the desktop default app
- `nvim` - open text files in a terminal
- A supported terminal: `$TERMINAL`, alacritty, foot, footclient, kitty, wezterm, gnome-terminal, konsole, xfce4-terminal, or xterm

`--debug` prints the app version, checks the pkg-config modules and runtime integrations above, prints the report to stdout, and exits with status `1` if any required dependency is missing.

## Building

```bash
# Debug build and run
./run.sh

# Run Debug-profile tests
./test.sh

# Release build, test, and install into ./release
./release.sh
```

Instead of running `./release.sh`, you can download a compiled artifact from [GitHub Releases](https://github.com/mateimacovei/hyprfile/releases).

## Running

```bash
# Debug build
./build/hyprfile [directory]

# Release artifact
./release/hyprfile [directory]

# If ./release is on PATH
hyprfile [directory]

# With verbose UI logging
./build/hyprfile -v -d /path/to/dir

# Print version and dependency diagnostics without launching the UI
./build/hyprfile --debug
```

## Keyboard Shortcuts

Most actions are keyboard-only. Vim-style keys and arrow keys are both supported where listed.

### Navigation

| Keys | Action |
| --- | --- |
| `j`, `Down` | Move down |
| `k`, `Up` | Move up |
| `h`, `Left`, `Backspace` | Go to parent directory |
| `l`, `Right` | Go to selected child directory, or enter supported preview actions |
| `Enter` | Open the selected directory or open the selected file with the system default app |
| `g` | Go to top |
| `G` | Go to bottom |
| `PgDn`, `Ctrl+j` | Page down |
| `PgUp`, `Ctrl+k` | Page up |
| `r` | Refresh current directory |
| `.` | Toggle hidden files |

### Count Prefixes

You can type a number before count-aware actions to repeat them. For example, `5j` moves down five entries, `10k` moves up ten entries, and `3n` jumps to the third next search match.

Count prefixes currently apply to:

| Keys | Counted action |
| --- | --- |
| `j`, `Down` | Move down N entries |
| `k`, `Up` | Move up N entries |
| `n` | Jump to the Nth next search result |
| `N` | Jump to the Nth previous search result |

### Search

| Keys | Action |
| --- | --- |
| `/` | Start search input |
| `Enter` | Commit the active search |
| `Esc` | Cancel the active search |
| `n` | Jump to next search result |
| `N` | Jump to previous search result |

### File Operations

| Keys | Action |
| --- | --- |
| `y` | Copy the selected file or directory |
| `Y` | Cut the selected file or directory |
| `p` | Paste into the current directory |
| `Del`, `D` | Move the selected item to trash |

### Windows And Terminal

| Keys | Action |
| --- | --- |
| `t` | Open a terminal in the current folder |
| `T` | Open a new hyprfile window in the current folder |

### Preview And Fullscreen

| Keys | Action |
| --- | --- |
| `l`, `Right` | For images/videos, enter fullscreen preview or advance to the next fullscreen-supported item |
| `h`, `Left` | In fullscreen preview, go to the previous fullscreen-supported item |

### App Controls

| Keys | Action |
| --- | --- |
| `H` | Show help |
| `q`, `Esc` | Quit, close help, or exit fullscreen preview |
| `v` | Multi-selection mode |

### Note
I am a Java programmer by profession, so sorry in advance if I'm breaking any of the C++ holy laws here:)
