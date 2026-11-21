function(dc24h_enable_warnings target)
    target_compile_options(${target} PRIVATE
        "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion;-Wshadow>"
        "$<$<CXX_COMPILER_ID:MSVC>:/W4;/utf-8>"
    )
endfunction()
