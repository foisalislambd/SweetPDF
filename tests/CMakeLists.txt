qt_add_executable(sweetpdf_smoke SmokeTest.cpp)

target_link_libraries(sweetpdf_smoke PRIVATE sweetpdf_ui sweetpdf_warnings)

# The smoke test needs a document. Ghostscript turns the checked-in PostScript
# into a small multi-page PDF with bookmarks and metadata, so no binary fixture
# has to live in the repository.
find_program(GHOSTSCRIPT_EXECUTABLE NAMES gs gswin64c)

set(sample_pdf "${CMAKE_CURRENT_BINARY_DIR}/sample.pdf")

if(GHOSTSCRIPT_EXECUTABLE)
    add_custom_command(
        OUTPUT "${sample_pdf}"
        COMMAND "${GHOSTSCRIPT_EXECUTABLE}" -q -dNOPAUSE -dBATCH -sDEVICE=pdfwrite
                "-sOutputFile=${sample_pdf}" "${CMAKE_CURRENT_SOURCE_DIR}/sample.ps"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/sample.ps"
        COMMENT "Generating the smoke test document"
        VERBATIM
    )
    add_custom_target(sweetpdf_sample_pdf ALL DEPENDS "${sample_pdf}")
    add_dependencies(sweetpdf_smoke sweetpdf_sample_pdf)

    add_test(NAME smoke COMMAND sweetpdf_smoke "${sample_pdf}")
else()
    message(STATUS "Ghostscript not found - the smoke test has no document and is skipped")
endif()
