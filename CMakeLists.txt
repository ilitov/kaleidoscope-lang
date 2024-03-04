cmake_minimum_required(VERSION 3.16.0)

project(kaleidoscope)

set(CMAKE_CXX_STANDARD 20)

#add_compile_options(-Wall -Wextra -Wpedantic -Werror)

set(SOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SOURCES_LIST ${SOURCES_DIR}/main.cpp)

add_executable(
    ${PROJECT_NAME}
    ${SOURCES_LIST}
)

include_directories(PRIVATE include)
