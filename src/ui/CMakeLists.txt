qt_add_library(sweetpdf_ui STATIC
    IconLibrary.h      IconLibrary.cpp
    Theme.h            Theme.cpp
    RibbonBar.h        RibbonBar.cpp
    PageItem.h         PageItem.cpp
    PageView.h         PageView.cpp
    ViewerToolbar.h    ViewerToolbar.cpp
    ThumbnailPanel.h   ThumbnailPanel.cpp
    OutlinePanel.h     OutlinePanel.cpp
    Sidebar.h          Sidebar.cpp
    PropertiesPanel.h  PropertiesPanel.cpp
    MainWindow.h       MainWindow.cpp
)

target_include_directories(sweetpdf_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(sweetpdf_ui
    PUBLIC  sweetpdf_core Qt6::Widgets
    PRIVATE Qt6::Svg sweetpdf_warnings
)
