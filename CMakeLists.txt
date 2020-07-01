cmake_minimum_required(VERSION 3.10)
project(rel C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_INSTALL_PREFIX /usr/local/bin)

add_definitions(-DDEBUG)

find_package(BISON REQUIRED)
find_package(PkgConfig REQUIRED)

bison_target(parser src/parse.y ${CMAKE_CURRENT_BINARY_DIR}/parse.c
             DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parse.h)

set(SOURCE_FILES
    src/hashset.c src/buildprog.c src/sub.c
    src/pike.c src/utils.c src/transition.c
    src/compile.c src/main.c
)

set(HEADER_FILES
    src/regex.h src/hashset.h
    src/utils.h src/transition.h
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(rel ${BISON_parser_OUTPUTS} ${SOURCE_FILES} ${HEADER_FILES})

pkg_check_modules(deps REQUIRED IMPORTED_TARGET glib-2.0)
target_link_libraries(rel PkgConfig::deps)

install(TARGETS rel DESTINATION ${CMAKE_INSTALL_PREFIX})
