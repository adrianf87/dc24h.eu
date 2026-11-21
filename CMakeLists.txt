cmake_minimum_required(VERSION 3.22)
project(dc24h VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerWarnings.cmake)

set(DC24H_RELEASE_LABEL "Alpha ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
set(DC24H_VERSION "${PROJECT_VERSION}-alpha.1")
configure_file(cmake/version.hpp.in generated/dc24h/version.hpp @ONLY)

add_library(dc24h_cli STATIC src/cli.cpp)
target_include_directories(dc24h_cli
    PUBLIC include
    PRIVATE "${PROJECT_BINARY_DIR}/generated"
)
dc24h_enable_warnings(dc24h_cli)

add_executable(dc24h-hub src/main.cpp)
target_link_libraries(dc24h-hub PRIVATE dc24h_cli)
dc24h_enable_warnings(dc24h-hub)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
