#include "ui/IconLibrary.h"

#include <QHash>

namespace sweet::ui::icons {
namespace {

/// Icon bodies on a 24x24 grid. Strokes use the `@C` placeholder, which Theme
/// replaces with the colour the icon is being drawn in.
const QHash<QString, QString> &bodies()
{
    static const QHash<QString, QString> table = {
        // File
        { QStringLiteral("open"),
          QStringLiteral("<path d='M4 8.5V6.5A1.5 1.5 0 0 1 5.5 5h3.3l1.8 2h5.9A1.5 1.5 0 0 1 18 8.5V10'/>"
                         "<path d='M3.2 10.2h17.6l-1.9 8.1A1.5 1.5 0 0 1 17.4 19.5H6.6a1.5 1.5 0 0 1-1.5-1.2Z'/>") },
        { QStringLiteral("save"),
          QStringLiteral("<path d='M5 4.8h9.5L19 9.3V19a.8.8 0 0 1-.8.8H5.8A.8.8 0 0 1 5 19Z'/>"
                         "<path d='M8.5 4.8V9.5h6'/><path d='M8.5 19.8v-5.6h7v5.6'/>") },
        { QStringLiteral("save-as"),
          QStringLiteral("<path d='M5 4.8h9.5L19 9.3v3'/>"
                         "<path d='M5 4.8V19a.8.8 0 0 0 .8.8h6'/>"
                         "<path d='M8.5 4.8V9.5h6'/><path d='M15 20l6-6-2-2-6 6-.6 2.6Z'/>") },
        { QStringLiteral("print"),
          QStringLiteral("<path d='M7 8.5V4.2h10v4.3'/>"
                         "<path d='M6 8.5h12a2 2 0 0 1 2 2v5h-3'/>"
                         "<path d='M7 15.5H4v-5a2 2 0 0 1 2-2'/>"
                         "<path d='M7 13h10v6.8H7Z'/>") },
        { QStringLiteral("export"),
          QStringLiteral("<path d='M12 3.5v10'/><path d='M8.5 7 12 3.5 15.5 7'/>"
                         "<path d='M4.5 14v4.5a2 2 0 0 0 2 2h11a2 2 0 0 0 2-2V14'/>") },

        // History
        { QStringLiteral("undo"),
          QStringLiteral("<path d='M4.5 9.5h10.5a4.75 4.75 0 1 1 0 9.5H8.5'/>"
                         "<path d='M8.5 5.5 4.5 9.5l4 4'/>") },
        { QStringLiteral("redo"),
          QStringLiteral("<path d='M19.5 9.5H9a4.75 4.75 0 1 0 0 9.5h6.5'/>"
                         "<path d='M15.5 5.5 19.5 9.5l-4 4'/>") },

        // Pointer tools
        { QStringLiteral("cursor"),
          QStringLiteral("<path d='M5.5 3.2 18 10.8l-5.4 1.1 2.5 6-2.3 1-2.5-6-4.8 3.7Z'/>") },
        { QStringLiteral("hand"),
          QStringLiteral("<path d='M8.5 12.5V6.6a1.55 1.55 0 0 1 3.1 0v4.4'/>"
                         "<path d='M11.6 11V5.6a1.55 1.55 0 0 1 3.1 0V11'/>"
                         "<path d='M14.7 11.2V7.8a1.55 1.55 0 0 1 3.1 0V15a5.2 5.2 0 0 1-5.2 5.2h-1.2A5.2 5.2 0 0 1 6.2 15v-2.2l-1.5 1.1a1.6 1.6 0 0 1-1.9-2.5l2.7-2.1'/>") },

        // Content editing
        { QStringLiteral("edit-text"),
          QStringLiteral("<path d='M6.5 6.5V4.5h11v2'/><path d='M12 4.5v15'/>"
                         "<path d='M9.5 19.5h5'/>") },
        { QStringLiteral("edit-image"),
          QStringLiteral("<path d='M4 5.2h12v9.2H4Z'/>"
                         "<path d='M4 12.2 7.2 9l3.1 3.1 2.4-2.4 3.3 3.3'/>"
                         "<circle cx='7.6' cy='8' r='1.15'/>"
                         "<path d='M14.6 19.6 21 13.2l-2-2-6.4 6.4-.5 2.5Z'/>") },
        { QStringLiteral("add-text"),
          QStringLiteral("<path d='M4 6.5h10'/><path d='M4 11h10'/><path d='M4 15.5h5.5'/>"
                         "<path d='M17 10.5v8'/><path d='M13 14.5h8'/>") },
        { QStringLiteral("add-image"),
          QStringLiteral("<path d='M4 5h12.5v9.5H4Z'/>"
                         "<path d='M4 12 7.2 8.8l3.2 3.2 2.3-2.3 3.8 3.8'/>"
                         "<circle cx='7.4' cy='7.8' r='1.1'/>"
                         "<path d='M17.5 16v6'/><path d='M14.5 19h6'/>") },
        { QStringLiteral("shape"),
          QStringLiteral("<path d='M4 13.5h7v7H4Z'/><path d='M13.5 3.5 20.5 10.5l-3.5 3.5-7-7Z'/>") },
        { QStringLiteral("link"),
          QStringLiteral("<path d='M10 13.5a3.5 3.5 0 0 0 5 0l3-3a3.5 3.5 0 0 0-5-5l-1.5 1.5'/>"
                         "<path d='M14 10.5a3.5 3.5 0 0 0-5 0l-3 3a3.5 3.5 0 0 0 5 5l1.5-1.5'/>") },

        // Markup
        { QStringLiteral("highlight"),
          QStringLiteral("<path d='M9 14.2 7 19.2l5-2 8.2-8.2-3-3Z'/>"
                         "<path d='M13.2 6 18.2 11'/><path d='M4 21.2h16'/>") },
        { QStringLiteral("underline"),
          QStringLiteral("<path d='M7 4.2v6.8a5 5 0 0 0 10 0V4.2'/><path d='M5 20.2h14'/>") },
        { QStringLiteral("strikeout"),
          QStringLiteral("<path d='M4.5 12h15'/>"
                         "<path d='M8 7.6c0-1.9 1.8-3.4 4-3.4s4 1.2 4 2.8'/>"
                         "<path d='M16 16.4c0 1.9-1.8 3.4-4 3.4s-4-1.2-4-2.8'/>") },
        { QStringLiteral("draw"),
          QStringLiteral("<path d='M4 20l1.2-4.4L16.4 4.4l3.2 3.2L8.4 18.8Z'/>"
                         "<path d='M14.2 6.6 17.4 9.8'/>") },
        { QStringLiteral("note"),
          QStringLiteral("<path d='M5 4.5h14v10.2l-4.3 4.8H5Z'/><path d='M14.7 19.5v-4.8H19'/>"
                         "<path d='M8.5 9h7'/><path d='M8.5 12.5h4.5'/>") },
        { QStringLiteral("redact"),
          QStringLiteral("<path d='M4 8.5h16v7H4Z'/><path d='M4 4.5h9'/><path d='M4 19.5h9'/>") },

        // Tools
        { QStringLiteral("ocr"),
          QStringLiteral("<path d='M4 8.5V4.5h4'/><path d='M20 8.5V4.5h-4'/>"
                         "<path d='M4 15.5v4h4'/><path d='M20 15.5v4h-4'/>"
                         "<path d='M9 15.5 12 8.5l3 7'/><path d='M10.1 13.2h3.8'/>") },
        { QStringLiteral("merge"),
          QStringLiteral("<path d='M3.5 6.5h5.6l4.2 5.5H20'/><path d='M3.5 17.5h5.6l4.2-5.5'/>"
                         "<path d='M17 9 20 12l-3 3'/>") },
        { QStringLiteral("split"),
          QStringLiteral("<path d='M20.5 6.5h-5.6l-4.2 5.5H4'/><path d='M20.5 17.5h-5.6l-4.2-5.5'/>"
                         "<path d='M7 9 4 12l3 3'/>") },
        { QStringLiteral("compress"),
          QStringLiteral("<path d='M12 3.5v5.5'/><path d='M9 6.5 12 9.5 15 6.5'/>"
                         "<path d='M12 20.5V15'/><path d='M9 17.5 12 14.5 15 17.5'/>"
                         "<path d='M4 12h16'/>") },
        { QStringLiteral("convert"),
          QStringLiteral("<path d='M4.5 8.5h11.5l-3.2-3.2'/><path d='M19.5 15.5H8l3.2 3.2'/>") },
        { QStringLiteral("lock"),
          QStringLiteral("<path d='M6 11h12v9H6Z'/><path d='M9 11V8a3 3 0 0 1 6 0v3'/>"
                         "<circle cx='12' cy='15.4' r='1.15'/>") },
        { QStringLiteral("shield"),
          QStringLiteral("<path d='M12 3.2 20 6.2v5.9c0 4.9-4 7.9-8 8.7-4-.8-8-3.8-8-8.7V6.2Z'/>"
                         "<path d='M9 12.2l2.2 2.2 4-4'/>") },
        { QStringLiteral("sign"),
          QStringLiteral("<path d='M4 16.5c3.2 0 5-10 8-10s1.8 7.6 4 7.6 2-2.6 4-2.6'/>"
                         "<path d='M4 20.5h16'/>") },
        { QStringLiteral("form"),
          QStringLiteral("<path d='M5 4.5h14v15H5Z'/><path d='M8.5 9h7'/><path d='M8.5 12.5h7'/>"
                         "<path d='M8.5 16h3.5'/>") },
        { QStringLiteral("settings"),
          QStringLiteral("<circle cx='12' cy='12' r='3.1'/>"
                         "<path d='M12 3v2.4'/><path d='M12 18.6V21'/><path d='M3 12h2.4'/>"
                         "<path d='M18.6 12H21'/><path d='M5.6 5.6 7.3 7.3'/>"
                         "<path d='M16.7 16.7 18.4 18.4'/><path d='M18.4 5.6 16.7 7.3'/>"
                         "<path d='M7.3 16.7 5.6 18.4'/>") },

        // Sidebar
        { QStringLiteral("pages"),
          QStringLiteral("<path d='M8 3.5h7l4 4v13H8Z'/><path d='M15 3.5v4h4'/>"
                         "<path d='M5 7v13.5h2.5'/>") },
        { QStringLiteral("bookmark"),
          QStringLiteral("<path d='M6.5 4h11v16.5l-5.5-3.8-5.5 3.8Z'/>") },
        { QStringLiteral("comment"),
          QStringLiteral("<path d='M4 5h16v10.5h-8.6L6.5 19.5v-4H4Z'/>"
                         "<path d='M8 8.8h8'/><path d='M8 11.8h5'/>") },
        { QStringLiteral("attachment"),
          QStringLiteral("<path d='M16.3 7.2 8.8 14.7a3.1 3.1 0 0 0 4.4 4.4l7.3-7.3a5.2 5.2 0 0 0-7.3-7.3L5.5 12.2'/>") },
        { QStringLiteral("layers"),
          QStringLiteral("<path d='M12 3.2 3.5 8 12 12.8 20.5 8Z'/><path d='M3.5 13 12 17.8 20.5 13'/>") },
        { QStringLiteral("info"),
          QStringLiteral("<circle cx='12' cy='12' r='8.2'/><path d='M12 11v5.5'/>"
                         "<circle cx='12' cy='8.2' r='1'/>") },

        // View
        { QStringLiteral("zoom-in"),
          QStringLiteral("<circle cx='10.8' cy='10.8' r='6.6'/><path d='M15.6 15.6 20.8 20.8'/>"
                         "<path d='M8 10.8h5.6'/><path d='M10.8 8v5.6'/>") },
        { QStringLiteral("zoom-out"),
          QStringLiteral("<circle cx='10.8' cy='10.8' r='6.6'/><path d='M15.6 15.6 20.8 20.8'/>"
                         "<path d='M8 10.8h5.6'/>") },
        { QStringLiteral("search"),
          QStringLiteral("<circle cx='10.8' cy='10.8' r='6.6'/><path d='M15.6 15.6 20.8 20.8'/>") },
        { QStringLiteral("fit-page"),
          QStringLiteral("<path d='M4 9.5V4.5h5'/><path d='M20 9.5V4.5h-5'/>"
                         "<path d='M4 14.5v5h5'/><path d='M20 14.5v5h-5'/>") },
        { QStringLiteral("fit-width"),
          QStringLiteral("<path d='M3.5 7.5v9'/><path d='M20.5 7.5v9'/><path d='M6 12h12'/>"
                         "<path d='M8.6 9.4 6 12l2.6 2.6'/><path d='M15.4 9.4 18 12l-2.6 2.6'/>") },
        { QStringLiteral("actual-size"),
          QStringLiteral("<path d='M4.5 4.5h15v15h-15Z'/><path d='M9 15V9l-2 1.4'/>"
                         "<path d='M13 9h3.5l-2 2.6a2 2 0 1 1-1.4 3.4'/>") },
        { QStringLiteral("rotate-left"),
          QStringLiteral("<path d='M4.2 9V4'/><path d='M4.2 9h5'/>"
                         "<path d='M5.4 9A8 8 0 1 1 12 20.2'/>") },
        { QStringLiteral("rotate-right"),
          QStringLiteral("<path d='M19.8 9V4'/><path d='M19.8 9h-5'/>"
                         "<path d='M18.6 9A8 8 0 1 0 12 20.2'/>") },
        { QStringLiteral("single-page"),
          QStringLiteral("<path d='M7.5 4h9v16h-9Z'/>") },
        { QStringLiteral("continuous"),
          QStringLiteral("<path d='M7.5 2.5h9v8h-9Z'/><path d='M7.5 13.5h9v8h-9Z'/>") },
        { QStringLiteral("two-page"),
          QStringLiteral("<path d='M3.5 5h8v14h-8Z'/><path d='M12.5 5h8v14h-8Z'/>") },
        { QStringLiteral("grid"),
          QStringLiteral("<path d='M4 4h7v7H4Z'/><path d='M13 4h7v7h-7Z'/>"
                         "<path d='M4 13h7v7H4Z'/><path d='M13 13h7v7h-7Z'/>") },
        { QStringLiteral("panel-left"),
          QStringLiteral("<path d='M4 5h16v14H4Z'/><path d='M9.2 5v14'/>") },
        { QStringLiteral("panel-right"),
          QStringLiteral("<path d='M4 5h16v14H4Z'/><path d='M14.8 5v14'/>") },
        { QStringLiteral("panel-both"),
          QStringLiteral("<path d='M4 5h16v14H4Z'/><path d='M8.6 5v14'/><path d='M15.4 5v14'/>") },
        { QStringLiteral("sun"),
          QStringLiteral("<circle cx='12' cy='12' r='4'/><path d='M12 3.2v2'/><path d='M12 18.8v2'/>"
                         "<path d='M3.2 12h2'/><path d='M18.8 12h2'/><path d='M5.9 5.9 7.4 7.4'/>"
                         "<path d='M16.6 16.6 18.1 18.1'/><path d='M18.1 5.9 16.6 7.4'/>"
                         "<path d='M7.4 16.6 5.9 18.1'/>") },
        { QStringLiteral("moon"),
          QStringLiteral("<path d='M20.2 14.6A8.6 8.6 0 1 1 9.4 3.8 6.6 6.6 0 0 0 20.2 14.6Z'/>") },

        // Text properties
        { QStringLiteral("bold"),
          QStringLiteral("<path d='M7 4.5h5.4a3.75 3.75 0 0 1 0 7.5H7Z'/>"
                         "<path d='M7 12h6.4a3.75 3.75 0 0 1 0 7.5H7Z'/>") },
        { QStringLiteral("italic"),
          QStringLiteral("<path d='M10 4.5h7.5'/><path d='M6.5 19.5H14'/><path d='M14 4.5 10 19.5'/>") },
        { QStringLiteral("align-left"),
          QStringLiteral("<path d='M4 6.5h16'/><path d='M4 12h10'/><path d='M4 17.5h13'/>") },
        { QStringLiteral("align-center"),
          QStringLiteral("<path d='M4 6.5h16'/><path d='M7 12h10'/><path d='M5.5 17.5h13'/>") },
        { QStringLiteral("align-right"),
          QStringLiteral("<path d='M4 6.5h16'/><path d='M10 12h10'/><path d='M7 17.5h13'/>") },
        { QStringLiteral("align-justify"),
          QStringLiteral("<path d='M4 6.5h16'/><path d='M4 12h16'/><path d='M4 17.5h16'/>") },

        // Generic
        { QStringLiteral("plus"), QStringLiteral("<path d='M12 5v14'/><path d='M5 12h14'/>") },
        { QStringLiteral("minus"), QStringLiteral("<path d='M5 12h14'/>") },
        { QStringLiteral("close"), QStringLiteral("<path d='M6.5 6.5 17.5 17.5'/><path d='M17.5 6.5 6.5 17.5'/>") },
        { QStringLiteral("check"), QStringLiteral("<path d='M5 12.8 9.5 17.3 19 7.8'/>") },
        { QStringLiteral("trash"),
          QStringLiteral("<path d='M5 7h14'/><path d='M9.5 7V4.6h5V7'/>"
                         "<path d='M6.8 7 7.9 20h8.2L17.2 7'/><path d='M10.6 10.6v6'/>"
                         "<path d='M13.4 10.6v6'/>") },
        { QStringLiteral("duplicate"),
          QStringLiteral("<path d='M8.5 8.5h11v11h-11Z'/><path d='M15.5 5.5h-11v11'/>") },
        { QStringLiteral("chevron-left"), QStringLiteral("<path d='M14.5 6 8.5 12l6 6'/>") },
        { QStringLiteral("chevron-right"), QStringLiteral("<path d='M9.5 6 15.5 12l-6 6'/>") },
        { QStringLiteral("chevron-down"), QStringLiteral("<path d='M6 9.5 12 15.5l6-6'/>") },
        { QStringLiteral("chevron-up"), QStringLiteral("<path d='M6 14.5 12 8.5l6 6'/>") },
        { QStringLiteral("first-page"),
          QStringLiteral("<path d='M16 6l-6 6 6 6'/><path d='M7.5 6v12'/>") },
        { QStringLiteral("last-page"),
          QStringLiteral("<path d='M8 6l6 6-6 6'/><path d='M16.5 6v12'/>") },
        { QStringLiteral("more"),
          QStringLiteral("<circle cx='5.5' cy='12' r='1.3'/><circle cx='12' cy='12' r='1.3'/>"
                         "<circle cx='18.5' cy='12' r='1.3'/>") },
        { QStringLiteral("logo"),
          QStringLiteral("<path d='M5 3.5h9l5 5V20a.5.5 0 0 1-.5.5h-13A.5.5 0 0 1 5 20Z'/>"
                         "<path d='M14 3.5v5h5'/><path d='M8.5 17.5v-6h2.2a1.9 1.9 0 0 1 0 3.8H8.5'/>") },
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
