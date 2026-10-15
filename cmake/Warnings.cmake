# Compiler warning profiles.
#
#   mk_set_warnings(<target> STRICT)  - new code: everything we can enable
#   mk_set_warnings(<target> LEGACY)  - the original assignment code
#
# Both include -Wconversion; MK_WERROR (default ON) turns warnings into errors.

set(MK_LEGACY_WARNINGS
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wvla)

set(MK_STRICT_WARNINGS
    ${MK_LEGACY_WARNINGS}
    -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes
    -Wformat=2 -Wimplicit-fallthrough -Wundef -Wpointer-arith
    -Wwrite-strings -Wold-style-definition -Wnull-dereference)

function(mk_set_warnings target profile)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(MK_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        return()
    endif()
    if(profile STREQUAL "STRICT")
        target_compile_options(${target} PRIVATE ${MK_STRICT_WARNINGS})
    else()
        target_compile_options(${target} PRIVATE ${MK_LEGACY_WARNINGS})
    endif()
    if(MK_WERROR)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()
