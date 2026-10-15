# The original assignment programs (with bug fixes, but the original
# O(N * V) data structures). Kept buildable for golden tests and as the
# "before" side of the benchmarks.

add_executable(legacy_ex3a_tweets
    ex3a/tweets_generator.c ex3a/markov_chain.c ex3a/linked_list.c)

add_executable(legacy_ex3b_tweets
    ex3b/tweets_generator.c ex3b/markov_chain.c ex3b/linked_list.c)

add_executable(legacy_ex3b_snakes
    ex3b/snakes_and_ladders.c ex3b/markov_chain.c ex3b/linked_list.c)

foreach(target legacy_ex3a_tweets legacy_ex3b_tweets legacy_ex3b_snakes)
    mk_set_warnings(${target} LEGACY)
endforeach()

# The Part B engine as a library, for the before/after benchmarks.
add_library(legacy_engine STATIC ex3b/markov_chain.c ex3b/linked_list.c)
target_include_directories(legacy_engine PUBLIC ex3b)
mk_set_warnings(legacy_engine LEGACY)
