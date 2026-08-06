qt_add_executable(sweetpdf
    main.cpp
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
    install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/org.sweetpdf.SweetPDF.png"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps")
endif()
