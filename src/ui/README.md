# `src/ui` — the interface

`sweetpdf_ui` is Qt Widgets on top of `sweetpdf_core`. It holds no document
state of its own: everything it shows comes from a `core::DocumentSession`, and
everything the user does goes back through that session.

```
ui/
  theme/    colour, typography, iconography, the stylesheet
  ribbon/   the command surface
  viewer/   the page canvas and its floating controls
  dialogs/  one modal per command
  panels/   docked navigators and inspectors
  window/   the shell that wires it all together
```

## What lives where

| Folder | Files | Responsibility |
| --- | --- | --- |
| `theme/` | `Theme`, `IconLibrary` | The single source of appearance: two palettes, a named type scale, ~90 inline SVG icons tinted per theme and state, the generated stylesheet, and drop shadows for floating surfaces. |
| `ribbon/` | `RibbonBar` | Tab strip plus grouped action buttons. `RibbonPage` → `RibbonGroup` → `QAction` or a custom widget. Also hosts the application menu button and the always-visible toggles. |
| `viewer/` | `PageView`, `PageItem`, `PageOverlay`, `FindBar`, `ViewerToolbar` | The scrolling canvas. A `QGraphicsScene` in PDF points, one `PageItem` per page, transient drawing (selection, search hits, the stroke or box being drawn) carried in a `PageOverlay`, and floating controls over the content. |
| `dialogs/` | `SecurityDialog`, `ExportImagesDialog`, `WatermarkDialog`, `OcrDialog`, `SignatureDialog`, `PreferencesDialog`, `PageRangeInput` | One modal per command, each collecting exactly the settings that command needs and nothing else. `PageRangeInput` is the shared "1-4, 8" parser. |
| `panels/` | `Sidebar`, `ThumbnailPanel`, `OutlinePanel`, `CommentsPanel`, `FormsPanel`, `PropertiesPanel` | The icon rail and its panels on the left, the inspector on the right. Panels read the document and rebuild from it rather than tracking edits; rebuilds are coalesced through a timer. |
| `window/` | `MainWindow`, `WelcomeView` | Actions and shortcuts, the application menu, document tabs, the status bar, recent files, drag and drop, and the empty state. |

Panels never open a file dialog: a panel that needs one (extract these pages,
insert a file here) emits a signal that `MainWindow` answers, which keeps the
question of "where does this file come from" in one place.

## Rules that keep the look consistent

1. **No literal colours, sizes or font sizes in widget code.** Read
   `themePalette()`, `themeFont(TextRole::…)` and `icon("name")`. Adding a colour
   means adding a field to `Palette` and filling it in for both themes.
2. **Style through the stylesheet where possible.** Give a widget an object name
   and style `#ThatName` in `Theme::styleSheet`. Reach for `QPainter` only for
   things a stylesheet cannot express: page shadows, thumbnail pages, drop
   shadows (`Theme::setElevation`).
3. **Never set `font-size` on a broad selector.** A size on `QWidget` would beat
   every widget-level `setFont`, which is how the type scale is applied.
4. **Re-tint on `Theme::changed`.** Icons already drawn into a widget do not
   update themselves, so anything holding a `QIcon` or a cached pixmap must
   connect to that signal.
5. **Widgets do not own document state.** Ask the session and react to its
   signals. This is what lets two views of one document stay in sync.
6. **Inside a `QWidget` subclass, use `themePalette()`, not `palette()`** — the
   latter resolves to `QWidget::palette()`.

## How a command travels

```
QAction (created in MainWindow::createActions, one id per command)
  -> appears in the ribbon, the application menu, the status bar, a shortcut
  -> triggered
       -> DocumentSession setter, or a DocumentCommand pushed on the undo stack
            -> session signal
                 -> PageView, panels and status bar update themselves
```

Because every surface shares the same `QAction`, enabling, checking or renaming a
command happens once and shows up everywhere.

## Adding an icon

Add one entry to the table in `IconLibrary.cpp` on the 24x24 grid: live area
3..21, outline only, `@C` where the stroke colour goes, and `fill='@F'` on shapes
that should get the tinted fill. `Theme::icon` handles the rest, including the
disabled and accented variants.
