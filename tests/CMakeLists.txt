find_program(BASH_PROGRAM bash)
find_program(VALGRIND_PROGRAM valgrind)

# unit tests
add_executable(unit_tests
    unit/test_main.c
    unit/test_alloc.c
    unit/test_rng.c
    unit/test_alias.c
    unit/test_chain.c
    unit/test_text.c
    unit/test_snakes.c
    unit/test_math.c
    unit/test_faults.c)
target_link_libraries(unit_tests PRIVATE markov)
mk_set_warnings(unit_tests STRICT)
add_test(NAME unit COMMAND unit_tests)

# fuzz target (libFuzzer with clang, a replay/random driver otherwise)
add_library(mk_fuzz_target STATIC fuzz/fuzz_engine.c)
target_link_libraries(mk_fuzz_target PUBLIC markov)
mk_set_warnings(mk_fuzz_target STRICT)

add_executable(fuzz_driver fuzz/fuzz_driver.c)
target_link_libraries(fuzz_driver PRIVATE mk_fuzz_target)
mk_set_warnings(fuzz_driver STRICT)
add_test(NAME fuzz_smoke COMMAND fuzz_driver --iterations 3000 --seed 2026)

if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND MK_LIBFUZZER)
    add_executable(fuzz_libfuzzer fuzz/fuzz_engine.c)
    target_link_libraries(fuzz_libfuzzer PRIVATE markov)
    target_compile_options(fuzz_libfuzzer PRIVATE -fsanitize=fuzzer)
    target_link_options(fuzz_libfuzzer PRIVATE -fsanitize=fuzzer)
endif()

# golden tests: byte-exact parity with the original programs
if(BASH_PROGRAM)
    set(GOLDEN ${CMAKE_CURRENT_SOURCE_DIR}/golden/run_golden.sh)
    add_test(NAME golden_cli
        COMMAND ${BASH_PROGRAM} ${GOLDEN}
                --tweets $<TARGET_FILE:tweets_generator>
                --snakes $<TARGET_FILE:snakes_and_ladders>)
    if(MK_BUILD_LEGACY)
        add_test(NAME golden_legacy
            COMMAND ${BASH_PROGRAM} ${GOLDEN}
                    --tweets $<TARGET_FILE:legacy_ex3b_tweets>
                    --tweets-a $<TARGET_FILE:legacy_ex3a_tweets>
                    --snakes $<TARGET_FILE:legacy_ex3b_snakes>)
    endif()
    add_test(NAME cli_behaviour
        COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/cli/run_cli_tests.sh
                $<TARGET_FILE:tweets_generator>
                $<TARGET_FILE:snakes_and_ladders>)
endif()

# Valgrind (only meaningful without ASan)
if(VALGRIND_PROGRAM AND NOT MK_SANITIZE)
    set(VG ${VALGRIND_PROGRAM} --quiet --error-exitcode=99 --leak-check=full
           --show-leak-kinds=all --errors-for-leak-kinds=all)
    add_test(NAME valgrind_unit COMMAND ${VG} $<TARGET_FILE:unit_tests>)
    add_test(NAME valgrind_fuzz
        COMMAND ${VG} $<TARGET_FILE:fuzz_driver> --iterations 300 --seed 7)
    if(BASH_PROGRAM)
        string(REPLACE ";" " " VG_STRING "${VG}")
        add_test(NAME valgrind_golden_cli
            COMMAND ${BASH_PROGRAM} ${GOLDEN}
                    --tweets $<TARGET_FILE:tweets_generator>
                    --snakes $<TARGET_FILE:snakes_and_ladders>)
        set_tests_properties(valgrind_golden_cli PROPERTIES
            ENVIRONMENT "GOLDEN_WRAPPER=${VG_STRING}")
        if(MK_BUILD_LEGACY)
            add_test(NAME valgrind_golden_legacy
                COMMAND ${BASH_PROGRAM} ${GOLDEN}
                        --tweets $<TARGET_FILE:legacy_ex3b_tweets>
                        --tweets-a $<TARGET_FILE:legacy_ex3a_tweets>
                        --snakes $<TARGET_FILE:legacy_ex3b_snakes>)
            # The legacy engine is O(N * V); under Valgrind the large corpora
            # take minutes, so they are covered by golden_legacy instead.
            set_tests_properties(valgrind_golden_legacy PROPERTIES
                ENVIRONMENT "GOLDEN_WRAPPER=${VG_STRING};GOLDEN_SKIP=large|io|justdoit")
        endif()
        set_tests_properties(valgrind_golden_cli valgrind_unit valgrind_fuzz
            PROPERTIES LABELS valgrind TIMEOUT 1800)
        if(MK_BUILD_LEGACY)
            set_tests_properties(valgrind_golden_legacy
                PROPERTIES LABELS valgrind TIMEOUT 1800)
        endif()
    endif()
endif()
