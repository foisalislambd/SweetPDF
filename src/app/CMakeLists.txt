set(sweetpdf_sources main.cpp)

if(WIN32)
    # The icon and the version block are compiled into the executable, so Explorer,
    # the taskbar and the installer's shortcut all show the real artwork.
    set(SWEETPDF_ICON_PATH "${CMAKE_SOURCE_DIR}/resources/icons/sweetpdf.ico")
    # VERSIONINFO wants four comma-separated numbers, not "1.0.0".
    set(SWEETPDF_VERSION_COMMA "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
    configure_file(sweetpdf.rc.in "${CMAKE_CURRENT_BINARY_DIR}/sweetpdf.rc" @ONLY)
    list(APPEND sweetpdf_sources "${CMAKE_CURRENT_BINARY_DIR}/sweetpdf.rc")
endif()

qt_add_executable(sweetpdf
    ${sweetpdf_sources}
)

set_target_properties(sweetpdf PROPERTIES
    OUTPUT_NAME sweetpdf
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME "SweetPDF"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.sweetpdf.SweetPDF"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

target_compile_definitions(sweetpdf PRIVATE SWEETPDF_VERSION="${PROJECT_VERSION}")

target_link_libraries(sweetpdf PRIVATE sweetpdf_ui sweetpdf_warnings)

# Where the app lands differs per platform: Linux follows the FHS, Windows ships
# a portable folder the deploy tool can fill with Qt next to the executable, and
# macOS installs the bundle as-is.
include(GNUInstallDirs)

if(WIN32)
    install(TARGETS sweetpdf RUNTIME DESTINATION ".")
elseif(APPLE)
    install(TARGETS sweetpdf BUNDLE DESTINATION ".")
else()
    install(TARGETS sweetpdf RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/org.sweetpdf.SweetPDF.desktop"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
    # The AppStream release entry has to name the version being built, and a
    # hand-maintained one would go stale the moment CI tags a release.
    string(TIMESTAMP SWEETPDF_BUILD_DATE "%Y-%m-%d" UTC)
    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/org.sweetpdf.SweetPDF.metainfo.xml.in"
        "${CMAKE_CURRENT_BINARY_DIR}/org.sweetpdf.SweetPDF.metainfo.xml"
        @ONLY
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.sweetpdf.SweetPDF.metainfo.xml"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo")
    # One 512px icon would leave panels and menus to downscale it themselves, which
    # looks soft at 16 and 24 pixels, so every size the theme spec lists is shipped.
    foreach(size 16 22 24 32 48 64 128 256 512)
        install(
            FILES "${CMAKE_SOURCE_DIR}/resources/icons/hicolor/${size}x${size}.png"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${size}x${size}/apps"
            RENAME "org.sweetpdf.SweetPDF.png"
        )
    endforeach()
endif()
