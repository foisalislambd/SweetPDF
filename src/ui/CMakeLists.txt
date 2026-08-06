# Everything visual. One folder per concern; see README.md in this directory.

qt_add_library(sweetpdf_ui STATIC
    # theme/ - the single source of colour, typography and iconography.
    theme/IconLibrary.h      theme/IconLibrary.cpp
    theme/Theme.h            theme/Theme.cpp

    # ribbon/ - the command surface.
    ribbon/RibbonBar.h       ribbon/RibbonBar.cpp

    # viewer/ - the page canvas and its floating controls.
    viewer/PageItem.h        viewer/PageItem.cpp
    viewer/PageView.h        viewer/PageView.cpp
    viewer/ViewerToolbar.h   viewer/ViewerToolbar.cpp

    # panels/ - docked navigators and inspectors.
    panels/Sidebar.h         panels/Sidebar.cpp
    panels/ThumbnailPanel.h  panels/ThumbnailPanel.cpp
    panels/OutlinePanel.h    panels/OutlinePanel.cpp
    panels/PropertiesPanel.h panels/PropertiesPanel.cpp

    # window/ - the shell that wires everything together.
    window/MainWindow.h      window/MainWindow.cpp
    window/WelcomeView.h     window/WelcomeView.cpp
)

target_include_directories(sweetpdf_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(sweetpdf_ui
    PUBLIC  sweetpdf_core Qt6::Widgets
    PRIVATE Qt6::Svg sweetpdf_warnings
)
