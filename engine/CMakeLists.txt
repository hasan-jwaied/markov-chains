add_library(markov STATIC
    src/alias.c
    src/alloc.c
    src/chain.c
    src/linalg.c
    src/math.c
    src/rng.c
    src/status.c
    src/text.c
    plugins/cell.c
    plugins/ngram.c
    plugins/snakes.c
    plugins/word.c)

target_include_directories(markov PUBLIC include)
mk_set_warnings(markov STRICT)

if(NOT MSVC)
    target_link_libraries(markov PUBLIC m)
endif()
