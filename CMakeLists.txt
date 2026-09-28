cmake_minimum_required(VERSION 3.20)
project(tgt_exchange LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-libgcc-file-name
        OUTPUT_VARIABLE LIBGCC_FILE
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    get_filename_component(GCC_INSTALL_DIR ${LIBGCC_FILE} DIRECTORY)
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE}
        --extra-arg=--gcc-install-dir=${GCC_INSTALL_DIR})
endif()

find_package(Drogon REQUIRED)

add_library(exchange_core core/OrderBook.cpp core/Exchange.cpp)
target_include_directories(exchange_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
add_executable(core_tests tests/core_tests.cc tests/exchange_tests.cc)
target_link_libraries(core_tests PRIVATE exchange_core Drogon::Drogon)
ParseAndAddDrogonTests(core_tests)
