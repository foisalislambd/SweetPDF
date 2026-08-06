qt_add_executable(sweetpdf_smoke SmokeTest.cpp)
target_link_libraries(sweetpdf_smoke PRIVATE sweetpdf_ui sweetpdf_warnings)

# The editing test needs no widgets: it drives core directly, which is where a
# bug would corrupt a document rather than just look wrong.
qt_add_executable(sweetpdf_edit_test EditTest.cpp)
target_link_libraries(sweetpdf_edit_test PRIVATE sweetpdf_core sweetpdf_warnings)

# Both tests need a document. Ghostscript turns the checked-in PostScript into a
# small multi-page PDF with bookmarks and metadata, so no binary fixture has to
# live in the repository.
find_program(GHOSTSCRIPT_EXECUTABLE NAMES gs gswin64c)

set(sample_pdf "${CMAKE_CURRENT_BINARY_DIR}/sample.pdf")

if(GHOSTSCRIPT_EXECUTABLE)
    add_custom_command(
        OUTPUT "${sample_pdf}"
        COMMAND "${GHOSTSCRIPT_EXECUTABLE}" -q -dNOPAUSE -dBATCH -sDEVICE=pdfwrite
                "-sOutputFile=${sample_pdf}" "${CMAKE_CURRENT_SOURCE_DIR}/sample.ps"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/sample.ps"
        COMMENT "Generating the test document"
        VERBATIM
    )
    add_custom_target(sweetpdf_sample_pdf ALL DEPENDS "${sample_pdf}")
    add_dependencies(sweetpdf_smoke sweetpdf_sample_pdf)
    add_dependencies(sweetpdf_edit_test sweetpdf_sample_pdf)

    add_test(NAME smoke COMMAND sweetpdf_smoke "${sample_pdf}")
    add_test(NAME editing COMMAND sweetpdf_edit_test "${sample_pdf}")
else()
    message(STATUS "Ghostscript not found - the tests have no document and are skipped")
endif()
