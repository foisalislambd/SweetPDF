#include "ui/theme/IconLibrary.h"

#include <QHash>

namespace sweet::ui::icons {
namespace {

/// Icon bodies on a 24x24 grid.
///
/// House rules, so the set stays coherent as it grows:
///   * live area is 3..21, which leaves a 3px optical margin on every side;
///   * outline style only, 1.6 stroke, round caps and joins;
///   * `@C` marks the stroke colour, which Theme substitutes per state;
///   * `@F` marks an optional fill, replaced with the same colour at low alpha
///     to give an icon weight without turning it into a solid glyph.
const QHash<QString, QString> &bodies()
{
    static const QHash<QString, QString> table = {
        // -- Application ---------------------------------------------------
        { QStringLiteral("menu"),
          QStringLiteral("<path d='M4 7h16'/><path d='M4 12h16'/><path d='M4 17h10'/>") },
        { QStringLiteral("logo"),
          QStringLiteral("<path d='M5.5 3.5h8.2L19 8.8V19a1.5 1.5 0 0 1-1.5 1.5H7A1.5 1.5 0 0 1 5.5 19Z' fill='@F'/>"
                         "<path d='M13.7 3.5v5.3H19'/>"
                         "<path d='M9 17.5v-6h2.4a1.9 1.9 0 0 1 0 3.9H9'/>") },
        { QStringLiteral("settings"),
          QStringLiteral("<circle cx='12' cy='12' r='2.9'/>"
                         "<path d='M12 3.4v2.2'/><path d='M12 18.4v2.2'/>"
                         "<path d='M3.4 12h2.2'/><path d='M18.4 12h2.2'/>"
                         "<path d='M5.9 5.9 7.5 7.5'/><path d='M16.5 16.5 18.1 18.1'/>"
                         "<path d='M18.1 5.9 16.5 7.5'/><path d='M7.5 16.5 5.9 18.1'/>") },
        { QStringLiteral("info"),
          QStringLiteral("<circle cx='12' cy='12' r='8.4'/><path d='M12 11.2v5.4'/>"
                         "<circle cx='12' cy='8' r='1'/>") },
        { QStringLiteral("keyboard"),
          QStringLiteral("<rect x='3' y='6.5' width='18' height='11' rx='2'/>"
                         "<path d='M7 10h.01'/><path d='M11 10h.01'/><path d='M15 10h.01'/>"
                         "<path d='M8 14h8'/>") },

        // -- File ----------------------------------------------------------
        { QStringLiteral("open"),
          QStringLiteral("<path d='M4 9V6.6A1.6 1.6 0 0 1 5.6 5h3.1l1.9 2.1h5.8A1.6 1.6 0 0 1 18 8.7v.6'/>"
                         "<path d='M3.3 10.3h17.4l-1.9 8A1.6 1.6 0 0 1 17.2 19.6H6.8a1.6 1.6 0 0 1-1.6-1.3Z' fill='@F'/>") },
        { QStringLiteral("file-pdf"),
          QStringLiteral("<path d='M6 3.6h7.6L18.4 8.4V20a.4.4 0 0 1-.4.4H6.4A.4.4 0 0 1 6 20Z' fill='@F'/>"
                         "<path d='M13.6 3.6v4.8h4.8'/>"
                         "<path d='M9 17.6v-4.4h1.8a1.4 1.4 0 0 1 0 2.8H9'/>") },
        { QStringLiteral("save"),
          QStringLiteral("<path d='M5.2 5.2h9.3l4.3 4.3v9.3a.8.8 0 0 1-.8.8H6a.8.8 0 0 1-.8-.8Z'/>"
                         "<path d='M8.6 5.2v4.3h6'/><path d='M8.6 19.6v-5.2h6.8v5.2'/>") },
        { QStringLiteral("save-as"),
          QStringLiteral("<path d='M5.2 5.2h9.3l4.3 4.3v2.6'/>"
                         "<path d='M5.2 5.2V19a.8.8 0 0 0 .8.8h5.6'/>"
                         "<path d='M8.6 5.2v4.3h6'/>"
                         "<path d='M14.6 19.8 20.4 14l-2.2-2.2-5.8 5.8-.5 2.6Z'/>") },
        { QStringLiteral("print"),
          QStringLiteral("<path d='M7.2 8.6V4.4h9.6v4.2'/>"
                         "<path d='M7.2 16.4H4.6a1.6 1.6 0 0 1-1.6-1.6v-4.6a1.6 1.6 0 0 1 1.6-1.6h14.8a1.6 1.6 0 0 1 1.6 1.6v4.6a1.6 1.6 0 0 1-1.6 1.6h-2.6'/>"
                         "<path d='M7.2 13.4h9.6v6.2H7.2Z' fill='@F'/>") },
        { QStringLiteral("export"),
          QStringLiteral("<path d='M12 3.6v9.8'/><path d='M8.4 7.2 12 3.6l3.6 3.6'/>"
                         "<path d='M4.6 14v4.4a1.6 1.6 0 0 0 1.6 1.6h11.6a1.6 1.6 0 0 0 1.6-1.6V14'/>") },
        { QStringLiteral("recent"),
          QStringLiteral("<circle cx='12' cy='12' r='8.4'/><path d='M12 7.4V12l3.4 2'/>") },
        { QStringLiteral("folder"),
          QStringLiteral("<path d='M3.4 7.2A1.6 1.6 0 0 1 5 5.6h3.6l2 2.2h7.4A1.6 1.6 0 0 1 19.6 9.4v7.4a1.6 1.6 0 0 1-1.6 1.6H5a1.6 1.6 0 0 1-1.6-1.6Z' fill='@F'/>") },
        { QStringLiteral("star"),
          QStringLiteral("<path d='M12 3.8l2.6 5.3 5.8.8-4.2 4.1 1 5.8L12 17.1l-5.2 2.7 1-5.8L3.6 9.9l5.8-.8Z' fill='@F'/>") },

        // -- History -------------------------------------------------------
        { QStringLiteral("undo"),
          QStringLiteral("<path d='M4.6 9.6h10.6a4.8 4.8 0 1 1 0 9.6H8.6'/>"
                         "<path d='M8.6 5.6 4.6 9.6l4 4'/>") },
        { QStringLiteral("redo"),
          QStringLiteral("<path d='M19.4 9.6H8.8a4.8 4.8 0 1 0 0 9.6h6.6'/>"
                         "<path d='M15.4 5.6 19.4 9.6l-4 4'/>") },
        { QStringLiteral("copy"),
          QStringLiteral("<rect x='8.6' y='8.6' width='11.4' height='11.4' rx='2'/>"
                         "<path d='M15.6 5.6H6a1.6 1.6 0 0 0-1.6 1.6v9.6'/>") },
        { QStringLiteral("duplicate"),
          QStringLiteral("<rect x='8.6' y='8.6' width='11.4' height='11.4' rx='2'/>"
                         "<path d='M15.6 5.6H6a1.6 1.6 0 0 0-1.6 1.6v9.6'/>"
                         "<path d='M14.3 11.6v5.4'/><path d='M11.6 14.3h5.4'/>") },

        // -- Pointer tools -------------------------------------------------
        { QStringLiteral("cursor"),
          QStringLiteral("<path d='M5.6 3.4 17.8 10.9l-5.3 1 2.4 5.9-2.2 1-2.4-5.9-4.7 3.6Z' fill='@F'/>") },
        { QStringLiteral("hand"),
          QStringLiteral("<path d='M8.6 12.4V6.7a1.5 1.5 0 0 1 3 0v4.2'/>"
                         "<path d='M11.6 11V5.7a1.5 1.5 0 0 1 3 0V11'/>"
                         "<path d='M14.6 11.2V8a1.5 1.5 0 0 1 3 0v6.9a5.1 5.1 0 0 1-5.1 5.1h-1.1a5.1 5.1 0 0 1-5.1-5.1v-2.1l-1.4 1a1.55 1.55 0 0 1-1.9-2.4l2.6-2'/>") },
        { QStringLiteral("text-select"),
          QStringLiteral("<path d='M9.4 4.4h5.2'/><path d='M12 4.4v15.2'/><path d='M9.4 19.6h5.2'/>"
                         "<path d='M4 8.4V4.4h3'/><path d='M20 8.4V4.4h-3'/>"
                         "<path d='M4 15.6v4h3'/><path d='M20 15.6v4h-3'/>") },
        { QStringLiteral("zoom-area"),
          QStringLiteral("<circle cx='11' cy='11' r='6.4'/><path d='M15.6 15.6 20.4 20.4'/>"
                         "<path d='M8.4 11h5.2'/><path d='M11 8.4v5.2'/>") },

        // -- Content editing ----------------------------------------------
        { QStringLiteral("edit-text"),
          QStringLiteral("<path d='M6.4 7V4.6h11.2V7'/><path d='M12 4.6v14.8'/>"
                         "<path d='M9.4 19.4h5.2'/>") },
        { QStringLiteral("edit-image"),
          QStringLiteral("<rect x='3.6' y='5' width='12.4' height='9.4' rx='1.6' fill='@F'/>"
                         "<path d='M3.6 12.2 7 8.8l3.2 3.2 2.4-2.4 3.4 3.4'/>"
                         "<circle cx='7.4' cy='8' r='1.1'/>"
                         "<path d='M14.4 19.8 20.4 13.8l-2.2-2.2-6 6-.5 2.7Z'/>") },
        { QStringLiteral("add-text"),
          QStringLiteral("<path d='M4 6.6h10'/><path d='M4 11h10'/><path d='M4 15.4h5.6'/>"
                         "<path d='M17 10.4v8.2'/><path d='M12.9 14.5h8.2'/>") },
        { QStringLiteral("add-image"),
          QStringLiteral("<rect x='3.6' y='4.6' width='12.8' height='9.6' rx='1.6' fill='@F'/>"
                         "<path d='M3.6 11.8 7 8.4l3.2 3.2 2.3-2.3 3.9 3.9'/>"
                         "<circle cx='7.3' cy='7.6' r='1.1'/>"
                         "<path d='M17.4 15.6v5.4'/><path d='M14.7 18.3h5.4'/>") },
        { QStringLiteral("shape"),
          QStringLiteral("<rect x='3.6' y='13.2' width='7.2' height='7.2' rx='1.2' fill='@F'/>"
                         "<path d='M13.8 3.6 20.4 10.2 17 13.6 10.4 7Z'/>") },
        { QStringLiteral("ellipse"),
          QStringLiteral("<ellipse cx='12' cy='12' rx='8.2' ry='5.8' fill='@F'/>") },
        { QStringLiteral("line"),
          QStringLiteral("<path d='M4.4 19.2 19.6 4.8'/>") },
        { QStringLiteral("arrow"),
          QStringLiteral("<path d='M4.4 19.2 16.8 6.8'/>"
                         "<path d='M11.2 6.2h6.2v6.2'/><path d='M17.4 6.2 11.6 12'/>") },
        { QStringLiteral("link"),
          QStringLiteral("<path d='M10.2 13.4a3.4 3.4 0 0 0 4.8 0l2.9-2.9a3.4 3.4 0 0 0-4.8-4.8l-1.5 1.5'/>"
                         "<path d='M13.8 10.6a3.4 3.4 0 0 0-4.8 0L6.1 13.5a3.4 3.4 0 0 0 4.8 4.8l1.5-1.5'/>") },
        { QStringLiteral("crop"),
          QStringLiteral("<path d='M6.4 3.6v11.8a1.6 1.6 0 0 0 1.6 1.6h12'/>"
                         "<path d='M20.4 17.6V6a1.6 1.6 0 0 0-1.6-1.6H7'/>"
                         "<path d='M17.6 20.4v-2.8'/><path d='M3.6 6.4h2.8'/>") },
        { QStringLiteral("watermark"),
          QStringLiteral("<rect x='4' y='4' width='16' height='16' rx='2'/>"
                         "<path d='M8 15.4 15.4 8'/><path d='M11.4 17.4 17.4 11.4'/>"
                         "<path d='M6.6 12 12 6.6'/>") },

        // -- Markup --------------------------------------------------------
        { QStringLiteral("highlight"),
          QStringLiteral("<path d='M8.8 14.4 6.8 19.2l4.8-2 8.2-8.2-2.8-2.8Z' fill='@F'/>"
                         "<path d='M13.2 6 18 10.8'/><path d='M4 20.8h16'/>") },
        { QStringLiteral("underline"),
          QStringLiteral("<path d='M7 4.4v6.6a5 5 0 0 0 10 0V4.4'/><path d='M5.4 20h13.2'/>") },
        { QStringLiteral("strikeout"),
          QStringLiteral("<path d='M4.6 12h14.8'/>"
                         "<path d='M8 7.8c0-2 1.8-3.4 4-3.4s4 1.2 4 2.8'/>"
                         "<path d='M16 16.2c0 2-1.8 3.4-4 3.4s-4-1.2-4-2.8'/>") },
        { QStringLiteral("squiggly"),
          QStringLiteral("<path d='M6.4 7.2v6.2a4.4 4.4 0 0 0 8.8 0V7.2'/>"
                         "<path d='M4.6 19.2q1.4-1.8 2.8 0t2.8 0 2.8 0 2.8 0 2.8 0'/>") },
        { QStringLiteral("draw"),
          QStringLiteral("<path d='M4 20l1.2-4.4L16.2 4.6l3.2 3.2L8.4 18.8Z' fill='@F'/>"
                         "<path d='M14 6.8 17.2 10'/>") },
        { QStringLiteral("note"),
          QStringLiteral("<path d='M5 4.8h14v9.8l-4.4 4.6H5Z' fill='@F'/>"
                         "<path d='M14.6 19.2v-4.6H19'/>"
                         "<path d='M8.4 9h7.2'/><path d='M8.4 12.4h4.6'/>") },
        { QStringLiteral("stamp"),
          QStringLiteral("<rect x='3.6' y='7.2' width='16.8' height='9.6' rx='1.4' fill='@F'/>"
                         "<path d='M7.2 12h9.6'/><path d='M8.4 4.8h7.2'/><path d='M8.4 19.2h7.2'/>") },
        { QStringLiteral("comment"),
          QStringLiteral("<path d='M4 5.6h16v9.8h-8.4L6.6 19v-3.6H4Z' fill='@F'/>"
                         "<path d='M8 9.2h8'/><path d='M8 12.2h5'/>") },
        { QStringLiteral("comment-plus"),
          QStringLiteral("<path d='M4 5.6h16v9.8h-8.4L6.6 19v-3.6H4Z' fill='@F'/>"
                         "<path d='M12 8.2v4.6'/><path d='M9.7 10.5h4.6'/>") },
        { QStringLiteral("redact"),
          QStringLiteral("<path d='M3.6 8.8h16.8v6.4H3.6Z' fill='@F'/>"
                         "<path d='M3.6 4.8h9.2'/><path d='M3.6 19.2h9.2'/>") },

        // -- Pages ---------------------------------------------------------
        { QStringLiteral("pages"),
          QStringLiteral("<path d='M8.4 3.6h6.6l4 4v12.8H8.4Z' fill='@F'/>"
                         "<path d='M15 3.6v4h4'/><path d='M5.4 7v13.4h2.6'/>") },
        { QStringLiteral("page-insert"),
          QStringLiteral("<path d='M6.4 3.6h7.2l4 4v6.2'/><path d='M13.6 3.6v4h4'/>"
                         "<path d='M6.4 3.6v16.8h5.4'/>"
                         "<path d='M17.6 15v5.4'/><path d='M14.9 17.7h5.4'/>") },
        { QStringLiteral("page-delete"),
          QStringLiteral("<path d='M6.4 3.6h7.2l4 4v6.2'/><path d='M13.6 3.6v4h4'/>"
                         "<path d='M6.4 3.6v16.8h5.4'/><path d='M14.9 17.7h5.4'/>") },
        { QStringLiteral("page-extract"),
          QStringLiteral("<path d='M9.6 3.6h5.8l4 4v10.8H9.6Z'/><path d='M15.4 3.6v4h4'/>"
                         "<path d='M6.4 6.8v13.6h2.6'/><path d='M12.4 14.6h4.4'/>"
                         "<path d='M14.6 12.4v4.4'/>") },
        { QStringLiteral("bookmark"),
          QStringLiteral("<path d='M6.6 4.4h10.8v16l-5.4-3.8-5.4 3.8Z' fill='@F'/>") },
        { QStringLiteral("attachment"),
          QStringLiteral("<path d='M16.2 7.4 8.8 14.8a3.1 3.1 0 0 0 4.4 4.4l7.2-7.2a5.2 5.2 0 0 0-7.3-7.3L5.6 12.2'/>") },
        { QStringLiteral("layers"),
          QStringLiteral("<path d='M12 3.6 3.8 8 12 12.4 20.2 8Z' fill='@F'/>"
                         "<path d='M3.8 12.8 12 17.2 20.2 12.8'/>") },

        // -- Tools ---------------------------------------------------------
        { QStringLiteral("ocr"),
          QStringLiteral("<path d='M4 8.6V4.6h4'/><path d='M20 8.6V4.6h-4'/>"
                         "<path d='M4 15.4v4h4'/><path d='M20 15.4v4h-4'/>"
                         "<path d='M9 15.4 12 8.6l3 6.8'/><path d='M10.1 13.1h3.8'/>") },
        { QStringLiteral("merge"),
          QStringLiteral("<path d='M3.6 6.6h5.6l4.2 5.4h5.4'/><path d='M3.6 17.4h5.6l4.2-5.4'/>"
                         "<path d='M16.8 9 19.8 12l-3 3'/>") },
        { QStringLiteral("split"),
          QStringLiteral("<path d='M20.4 6.6h-5.6l-4.2 5.4H5.2'/><path d='M20.4 17.4h-5.6l-4.2-5.4'/>"
                         "<path d='M7.2 9 4.2 12l3 3'/>") },
        { QStringLiteral("compress"),
          QStringLiteral("<path d='M12 3.6v5.2'/><path d='M9.2 6 12 8.8 14.8 6'/>"
                         "<path d='M12 20.4v-5.2'/><path d='M9.2 18 12 15.2 14.8 18'/>"
                         "<path d='M4 12h16'/>") },
        { QStringLiteral("convert"),
          QStringLiteral("<path d='M4.6 8.6h11.6l-3.2-3.2'/><path d='M19.4 15.4H7.8l3.2 3.2'/>") },
        { QStringLiteral("compare"),
          QStringLiteral("<path d='M11 4.6h-5A1.4 1.4 0 0 0 4.6 6v12A1.4 1.4 0 0 0 6 19.4h5'/>"
                         "<path d='M13 4.6h5A1.4 1.4 0 0 1 19.4 6v12A1.4 1.4 0 0 1 18 19.4h-5'/>"
                         "<path d='M12 3.4v17.2'/>") },
        { QStringLiteral("form"),
          QStringLiteral("<rect x='4.6' y='4.4' width='14.8' height='15.2' rx='1.8'/>"
                         "<path d='M8 9h8'/><path d='M8 12.6h8'/><path d='M8 16.2h3.6'/>") },
        { QStringLiteral("sign"),
          QStringLiteral("<path d='M4 16.4c3.2 0 5-9.8 8-9.8s1.8 7.4 4 7.4 2-2.6 4-2.6'/>"
                         "<path d='M4 20.4h16'/>") },
        { QStringLiteral("lock"),
          QStringLiteral("<rect x='5.6' y='10.6' width='12.8' height='9.4' rx='1.8' fill='@F'/>"
                         "<path d='M8.8 10.6V8a3.2 3.2 0 0 1 6.4 0v2.6'/>"
                         "<circle cx='12' cy='15.2' r='1.15'/>") },
        { QStringLiteral("lock-open"),
          QStringLiteral("<rect x='5.6' y='10.6' width='12.8' height='9.4' rx='1.8' fill='@F'/>"
                         "<path d='M8.8 10.6V8a3.2 3.2 0 0 1 6.2-1.1'/>"
                         "<circle cx='12' cy='15.2' r='1.15'/>") },
        { QStringLiteral("shield"),
          QStringLiteral("<path d='M12 3.4 20 6.4v5.8c0 4.8-4 7.8-8 8.6-4-.8-8-3.8-8-8.6V6.4Z' fill='@F'/>"
                         "<path d='M9.2 12.2 11.4 14.4 15.2 10.6'/>") },
        { QStringLiteral("eye"),
          QStringLiteral("<path d='M12 5.6c5 0 8.4 4.2 9.4 6.4-1 2.2-4.4 6.4-9.4 6.4S3.6 14.2 2.6 12C3.6 9.8 7 5.6 12 5.6Z'/>"
                         "<circle cx='12' cy='12' r='2.8'/>") },

        // -- View ----------------------------------------------------------
        { QStringLiteral("zoom-in"),
          QStringLiteral("<circle cx='10.8' cy='10.8' r='6.6'/><path d='M15.6 15.6 20.6 20.6'/>"
                         "<path d='M8 10.8h5.6'/><path d='M10.8 8v5.6'/>") },
        { QStringLiteral("zoom-out"),
          QStringLiteral("<circle cx='10.8' cy='10.8' r='6.6'/><path d='M15.6 15.6 20.6 20.6'/>"
                         "<path d='M8 10.8h5.6'/>") },
        { QStringLiteral("search"),
          QStringLiteral("<circle cx='10.8' cy='10.8' r='6.6'/><path d='M15.6 15.6 20.6 20.6'/>") },
        { QStringLiteral("case-sensitive"),
          QStringLiteral("<path d='M3 17.2 7.2 6.8l4.2 10.4'/><path d='M4.4 13.8h5.6'/>"
                         "<path d='M20.4 11.6a3.2 3.2 0 0 0-5.6 2.1v.6a3.2 3.2 0 0 0 5.6 2.1'/>"
                         "<path d='M20.4 10.4v6.6'/>") },
        { QStringLiteral("whole-word"),
          QStringLiteral("<path d='M3 8.6V6.4h18v2.2'/><path d='M3 15.4v2.2h18v-2.2'/>"
                         "<path d='M8.4 15 11 8.8 13.6 15'/><path d='M9.3 13.2h3.4'/>") },
        { QStringLiteral("fit-page"),
          QStringLiteral("<path d='M4 9.6V4.4h5.2'/><path d='M20 9.6V4.4h-5.2'/>"
                         "<path d='M4 14.4v5.2h5.2'/><path d='M20 14.4v5.2h-5.2'/>") },
        { QStringLiteral("fit-width"),
          QStringLiteral("<path d='M3.6 7.4v9.2'/><path d='M20.4 7.4v9.2'/>"
                         "<path d='M6.4 12h11.2'/><path d='M9 9.4 6.4 12 9 14.6'/>"
                         "<path d='M15 9.4 17.6 12 15 14.6'/>") },
        { QStringLiteral("actual-size"),
          QStringLiteral("<rect x='4.4' y='4.4' width='15.2' height='15.2' rx='2'/>"
                         "<path d='M9.4 15.2V9l-1.8 1.3'/>"
                         "<path d='M12.8 9h3.6l-2 2.7a2 2 0 1 1-1.5 3.4'/>") },
        { QStringLiteral("rotate-left"),
          QStringLiteral("<path d='M4.2 9.2V4.4'/><path d='M4.2 9.2h4.8'/>"
                         "<path d='M5.4 9.2A8 8 0 1 1 12 20.2'/>") },
        { QStringLiteral("rotate-right"),
          QStringLiteral("<path d='M19.8 9.2V4.4'/><path d='M19.8 9.2h-4.8'/>"
                         "<path d='M18.6 9.2A8 8 0 1 0 12 20.2'/>") },
        { QStringLiteral("single-page"),
          QStringLiteral("<rect x='7.4' y='4' width='9.2' height='16' rx='1.2' fill='@F'/>") },
        { QStringLiteral("continuous"),
          QStringLiteral("<rect x='7.4' y='2.6' width='9.2' height='8' rx='1.2' fill='@F'/>"
                         "<rect x='7.4' y='13.4' width='9.2' height='8' rx='1.2' fill='@F'/>") },
        { QStringLiteral("two-page"),
          QStringLiteral("<rect x='3.4' y='5' width='7.8' height='14' rx='1.2' fill='@F'/>"
                         "<rect x='12.8' y='5' width='7.8' height='14' rx='1.2' fill='@F'/>") },
        { QStringLiteral("grid"),
          QStringLiteral("<rect x='4' y='4' width='7' height='7' rx='1.2'/>"
                         "<rect x='13' y='4' width='7' height='7' rx='1.2'/>"
                         "<rect x='4' y='13' width='7' height='7' rx='1.2'/>"
                         "<rect x='13' y='13' width='7' height='7' rx='1.2'/>") },
        { QStringLiteral("panel-left"),
          QStringLiteral("<rect x='3.6' y='5' width='16.8' height='14' rx='2'/>"
                         "<path d='M9 5v14'/><path d='M4.8 8.2h2.6' /><path d='M4.8 11.4h2.6'/>") },
        { QStringLiteral("panel-right"),
          QStringLiteral("<rect x='3.6' y='5' width='16.8' height='14' rx='2'/>"
                         "<path d='M15 5v14'/><path d='M16.6 8.2h2.6'/><path d='M16.6 11.4h2.6'/>") },
        { QStringLiteral("sun"),
          QStringLiteral("<circle cx='12' cy='12' r='3.9'/><path d='M12 3.4v2'/>"
                         "<path d='M12 18.6v2'/><path d='M3.4 12h2'/><path d='M18.6 12h2'/>"
                         "<path d='M6 6 7.5 7.5'/><path d='M16.5 16.5 18 18'/>"
                         "<path d='M18 6 16.5 7.5'/><path d='M7.5 16.5 6 18'/>") },
        { QStringLiteral("moon"),
          QStringLiteral("<path d='M20.2 14.6A8.6 8.6 0 1 1 9.4 3.8 6.6 6.6 0 0 0 20.2 14.6Z' fill='@F'/>") },

        // -- Text properties ----------------------------------------------
        { QStringLiteral("bold"),
          QStringLiteral("<path d='M7 4.6h5.4a3.7 3.7 0 0 1 0 7.4H7Z'/>"
                         "<path d='M7 12h6.4a3.7 3.7 0 0 1 0 7.4H7Z'/>") },
        { QStringLiteral("italic"),
          QStringLiteral("<path d='M10 4.6h7.4'/><path d='M6.6 19.4H14'/>"
                         "<path d='M14 4.6 10 19.4'/>") },
        { QStringLiteral("align-left"),
          QStringLiteral("<path d='M4 6.6h16'/><path d='M4 12h10'/><path d='M4 17.4h13'/>") },
        { QStringLiteral("align-center"),
          QStringLiteral("<path d='M4 6.6h16'/><path d='M7 12h10'/><path d='M5.5 17.4h13'/>") },
        { QStringLiteral("align-right"),
          QStringLiteral("<path d='M4 6.6h16'/><path d='M10 12h10'/><path d='M7 17.4h13'/>") },
        { QStringLiteral("align-justify"),
          QStringLiteral("<path d='M4 6.6h16'/><path d='M4 12h16'/><path d='M4 17.4h16'/>") },

        // -- Generic -------------------------------------------------------
        { QStringLiteral("plus"), QStringLiteral("<path d='M12 5v14'/><path d='M5 12h14'/>") },
        { QStringLiteral("minus"), QStringLiteral("<path d='M5 12h14'/>") },
        { QStringLiteral("close"),
          QStringLiteral("<path d='M6.6 6.6 17.4 17.4'/><path d='M17.4 6.6 6.6 17.4'/>") },
        { QStringLiteral("check"), QStringLiteral("<path d='M5 12.8 9.5 17.3 19 7.8'/>") },
        { QStringLiteral("trash"),
          QStringLiteral("<path d='M5 7.2h14'/><path d='M9.6 7.2V4.8h4.8v2.4'/>"
                         "<path d='M6.8 7.2 7.9 19.8h8.2L17.2 7.2'/>"
                         "<path d='M10.6 10.8v5.6'/><path d='M13.4 10.8v5.6'/>") },
        { QStringLiteral("chevron-left"), QStringLiteral("<path d='M14.4 6 8.8 12l5.6 6'/>") },
        { QStringLiteral("chevron-right"), QStringLiteral("<path d='M9.6 6 15.2 12l-5.6 6'/>") },
        { QStringLiteral("chevron-down"), QStringLiteral("<path d='M6 9.6 12 15.2l6-5.6'/>") },
        { QStringLiteral("chevron-up"), QStringLiteral("<path d='M6 14.4 12 8.8l6 5.6'/>") },
        { QStringLiteral("first-page"),
          QStringLiteral("<path d='M16 6l-6 6 6 6'/><path d='M7.4 6v12'/>") },
        { QStringLiteral("last-page"),
          QStringLiteral("<path d='M8 6l6 6-6 6'/><path d='M16.6 6v12'/>") },
        { QStringLiteral("more"),
          QStringLiteral("<circle cx='5.6' cy='12' r='1.3'/><circle cx='12' cy='12' r='1.3'/>"
                         "<circle cx='18.4' cy='12' r='1.3'/>") },
    };
    return table;
}

} // namespace

QString markup(const QString &name)
{
    const QString body = bodies().value(name);
    if (body.isEmpty())
        return {};

    return QStringLiteral("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' "
                          "width='24' height='24'>"
                          "<g fill='none' stroke='@C' stroke-width='1.6' "
                          "stroke-linecap='round' stroke-linejoin='round'>%1</g></svg>")
        .arg(body);
}

QStringList available()
{
    QStringList names = bodies().keys();
    names.sort();
    return names;
}

} // namespace sweet::ui::icons
