add_library(vimith_options INTERFACE)

if(MSVC)
    target_compile_options(vimith_options INTERFACE
        /W4
        /permissive-
        /utf-8
        /wd4068  # unknown pragma
        $<$<CONFIG:Release>:/O2 /GL /DNDEBUG>
        $<$<CONFIG:Debug>:/Od /Zi /RTC1>
    )
    target_compile_definitions(vimith_options INTERFACE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )
else()
    target_compile_options(vimith_options INTERFACE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3 -march=native -DNDEBUG>
        $<$<CONFIG:Debug>:-g3>
    )
endif()
