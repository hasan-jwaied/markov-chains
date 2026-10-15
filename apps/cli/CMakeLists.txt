add_library(mk_cli_common STATIC cli_common.c)
mk_set_warnings(mk_cli_common STRICT)

foreach(app tweets_generator snakes_and_ladders)
    add_executable(${app} ${app}.c)
    target_link_libraries(${app} PRIVATE markov mk_cli_common)
    mk_set_warnings(${app} STRICT)
endforeach()
