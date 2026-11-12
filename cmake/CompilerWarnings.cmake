option(MOLIGRAM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

function(moligram_enable_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wnull-dereference
        -Wimplicit-fallthrough
        $<$<BOOL:${MOLIGRAM_WARNINGS_AS_ERRORS}>:-Werror>)
endfunction()
