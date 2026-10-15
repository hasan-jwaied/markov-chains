if(NOT MK_BUILD_LEGACY)
    message(STATUS "Benchmarks need MK_BUILD_LEGACY (the 'before' engine); skipping")
    return()
endif()

add_executable(bench bench_main.c)
target_link_libraries(bench PRIVATE markov legacy_engine)
mk_set_warnings(bench STRICT)

set(MK_BENCH_CORPORA
    ${PROJECT_SOURCE_DIR}/data/corpora/justdoit_tweets.txt
    ${PROJECT_SOURCE_DIR}/data/corpora/gutenberg/alice.txt
    ${PROJECT_SOURCE_DIR}/data/corpora/gutenberg/frankenstein.txt
    ${PROJECT_SOURCE_DIR}/data/corpora/gutenberg/pride-and-prejudice.txt
    ${PROJECT_SOURCE_DIR}/data/corpora/gutenberg/moby-dick.txt)

# `cmake --build <dir> --target benchmark` prints the Markdown report.
add_custom_target(benchmark
    COMMAND bench ${MK_BENCH_CORPORA}
    DEPENDS bench
    USES_TERMINAL)
