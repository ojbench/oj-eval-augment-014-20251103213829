cmake_minimum_required(VERSION 3.15.2)
project(python_interpreter)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")

# Build only our src/ without ANTLR dependencies
include_directories(
    ${PROJECT_SOURCE_DIR}/src/
)

file(GLOB_RECURSE main_src src/*.cpp)
add_executable(code ${main_src})
