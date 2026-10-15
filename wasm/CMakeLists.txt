if(NOT EMSCRIPTEN)
    # Native builds still compile the bridge with the strict warning set, and
    # the unit tests exercise it, so it cannot rot between browser builds.
    add_library(markov_wasm_api STATIC api.c)
    target_link_libraries(markov_wasm_api PUBLIC markov)
    mk_set_warnings(markov_wasm_api STRICT)
    return()
endif()

add_executable(markov_wasm api.c)
target_link_libraries(markov_wasm PRIVATE markov)
mk_set_warnings(markov_wasm STRICT)
set_target_properties(markov_wasm PROPERTIES OUTPUT_NAME markov SUFFIX ".mjs")
target_link_options(markov_wasm PRIVATE
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=createMarkovModule
    -sENVIRONMENT=web,worker,node
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=16MB
    -sMAXIMUM_MEMORY=1GB
    -sSTACK_SIZE=1MB
    -sFILESYSTEM=0
    -sEXPORTED_FUNCTIONS=_malloc,_free
    -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAP32,HEAPU32,HEAPF64,UTF8ToString)

# Hand the module to the web app.
set(MK_WEB_WASM_DIR ${PROJECT_SOURCE_DIR}/web/src/wasm)
add_custom_command(TARGET markov_wasm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${MK_WEB_WASM_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE_DIR:markov_wasm>/markov.mjs
            $<TARGET_FILE_DIR:markov_wasm>/markov.wasm
            ${MK_WEB_WASM_DIR}
    COMMENT "Copying markov.mjs + markov.wasm to web/src/wasm")
