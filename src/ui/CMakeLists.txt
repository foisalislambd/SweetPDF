# Everything visual. One folder per concern; see README.md in this directory.

qt_add_library(sweetpdf_ui STATIC
    # theme/ - the single source of colour, typography and iconography.
    theme/IconLibrary.h      theme/IconLibrary.cpp
    theme/Theme.h            theme/Theme.cpp

    # ribbon/ - the command surface.
    ribbon/RibbonBar.h       ribbon/RibbonBar.cpp

    # viewer/ - the page canvas and its floating controls.
    viewer/PageOverlay.h
    viewer/FindBar.h         viewer/FindBar.cpp
    viewer/PageItem.h        viewer/PageItem.cpp
    viewer/PageView.h        viewer/PageView.cpp
    viewer/ViewerToolbar.h   viewer/ViewerToolbar.cpp

    # dialogs/ - modal task surfaces, each one command deep.
    dialogs/PageRangeInput.h      dialogs/PageRangeInput.cpp
    dialogs/SecurityDialog.h      dialogs/SecurityDialog.cpp
    dialogs/ExportImagesDialog.h  dialogs/ExportImagesDialog.cpp
    dialogs/WatermarkDialog.h     dialogs/WatermarkDialog.cpp
    dialogs/OcrDialog.h           dialogs/OcrDialog.cpp
    dialogs/SignatureDialog.h     dialogs/SignatureDialog.cpp
    dialogs/PreferencesDialog.h   dialogs/PreferencesDialog.cpp

    # panels/ - docked navigators and inspectors.
    panels/Sidebar.h         panels/Sidebar.cpp
    panels/ThumbnailPanel.h  panels/ThumbnailPanel.cpp
    panels/OutlinePanel.h    panels/OutlinePanel.cpp
    panels/CommentsPanel.h   panels/CommentsPanel.cpp
    panels/FormsPanel.h      panels/FormsPanel.cpp
    panels/PropertiesPanel.h panels/PropertiesPanel.cpp

    # window/ - the shell that wires everything together.
    window/MainWindow.h      window/MainWindow.cpp
    window/WelcomeView.h     window/WelcomeView.cpp
)

target_include_directories(sweetpdf_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(sweetpdf_ui
    PUBLIC  sweetpdf_core Qt6::Widgets
    PRIVATE Qt6::Svg Qt6::PrintSupport sweetpdf_warnings
)
