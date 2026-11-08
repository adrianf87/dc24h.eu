cmake_minimum_required(VERSION 3.22)
project(dc24h VERSION 0.1.1 LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "dc24h currently targets Linux (Debian / Ubuntu).")
endif()

set(DC24H_VERSION "alpha-${PROJECT_VERSION}")
configure_file(cmake/version.hpp.in generated/dc24h/version.hpp @ONLY)
find_package(Threads REQUIRED)

add_library(dc24h_core
    src/core/server.cpp
    src/core/termination_signals.cpp
    src/logging/logger.cpp
)
target_compile_features(dc24h_core PUBLIC cxx_std_20)
set_target_properties(dc24h_core PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_core PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(dc24h_core PUBLIC Threads::Threads)
target_compile_definitions(dc24h_core PRIVATE _POSIX_C_SOURCE=200809L)

add_executable(dc24h src/main.cpp)
target_link_libraries(dc24h PRIVATE dc24h_core)
set_target_properties(dc24h PROPERTIES CXX_EXTENSIONS OFF)

foreach(target dc24h_core dc24h)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endforeach()

include(GNUInstallDirs)
install(TARGETS dc24h RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE dc24h_core)
    set_target_properties(core_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(core_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME core.lifecycle COMMAND core_tests)
    add_test(NAME core.process COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/process_tests.py" "$<TARGET_FILE:dc24h>")
    set_tests_properties(core.lifecycle core.process PROPERTIES TIMEOUT 30)
endif()
