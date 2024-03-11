cmake_minimum_required(VERSION 3.16.0)

project(kaleidoscope)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM definitions: ${LLVM_DEFINITIONS}")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(llvm_libs core orcjit)
message(STATUS "Link to LLVM libs: ${llvm_libs}")

#add_compile_options(-Wall -Wextra -Wpedantic -Werror)

set(SOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SOURCES_LIST ${SOURCES_DIR}/main.cpp ${SOURCES_DIR}/LLVMContextData.cpp)

add_executable(
    ${PROJECT_NAME}
    ${SOURCES_LIST}
)

include_directories(PRIVATE include)
include_directories(PRIVATE ${LLVM_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} ${llvm_libs})
