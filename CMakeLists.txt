cmake_minimum_required(VERSION 3.16)
project(pdp11sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(pdp11
    src/pdp11.cpp
    src/assembler.cpp
    src/disasm.cpp
)

target_include_directories(pdp11 PUBLIC src)

add_executable(pdp11sim src/main.cpp)
target_link_libraries(pdp11sim pdp11)

add_executable(pdp11_tests tests/test_runner.cpp)
target_link_libraries(pdp11_tests pdp11)
