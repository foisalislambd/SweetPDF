qt_add_executable(sweetpdf
    main.cpp
)

# The application icon is the one asset that has to be a real file: window
# managers, packagers and the window itself all want it. UI iconography stays in
# ui/theme/IconLibrary so it can be tinted per theme.
qt_add_resources(sweetpdf "sweetpdf_assets"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}/resources"
    FILES "${CMAKE_SOURCE_DIR}/resources/icons/org.sweetpdf.SweetPDF.svg"
)

set_target_properties(sweetpdf PROPERTIES
    OUTPUT_NAME sweetpdf
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME "SweetPDF"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.sweetpdf.SweetPDF"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

target_compile_definitions(sweetpdf PRIVATE SWEETPDF_VERSION="${PROJECT_VERSION}")

target_link_libraries(sweetpdf PRIVATE sweetpdf_ui sweetpdf_warnings)

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS sweetpdf RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/org.sweetpdf.SweetPDF.desktop"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
    install(FILES "${CMAKE_SOURCE_DIR}/resources/icons/org.sweetpdf.SweetPDF.svg"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps")
endif()
