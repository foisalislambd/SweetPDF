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
)

target_compile_definitions(sweetpdf PRIVATE SWEETPDF_VERSION="${PROJECT_VERSION}")

target_link_libraries(sweetpdf PRIVATE sweetpdf_ui sweetpdf_warnings)

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS sweetpdf RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/org.sweetpdf.SweetPDF.desktop"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
endif()
