cmake_minimum_required(VERSION 3.22)
project(dc24h VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerWarnings.cmake)

set(DC24H_RELEASE_LABEL "Alpha ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
set(DC24H_VERSION "${PROJECT_VERSION}-alpha.1")
configure_file(cmake/version.hpp.in generated/dc24h/version.hpp @ONLY)

find_path(UTF8PROC_INCLUDE_DIR NAMES utf8proc.h REQUIRED)
find_library(UTF8PROC_LIBRARY NAMES utf8proc REQUIRED)
find_path(GCRYPT_INCLUDE_DIR NAMES gcrypt.h REQUIRED)
find_library(GCRYPT_LIBRARY NAMES gcrypt REQUIRED)

add_library(dc24h_adc STATIC
    src/adc/parser.cpp
    src/adc/protocol.cpp
    src/adc/session.cpp
)
target_include_directories(dc24h_adc
    PUBLIC include
    PRIVATE "${UTF8PROC_INCLUDE_DIR}" "${GCRYPT_INCLUDE_DIR}"
)
target_link_libraries(dc24h_adc PRIVATE "${UTF8PROC_LIBRARY}" "${GCRYPT_LIBRARY}")
dc24h_enable_warnings(dc24h_adc)

add_library(dc24h_presence STATIC src/hub/presence.cpp src/hub/peer_routing.cpp)
target_include_directories(dc24h_presence PUBLIC include PRIVATE "${UTF8PROC_INCLUDE_DIR}")
target_link_libraries(dc24h_presence PUBLIC dc24h_adc PRIVATE "${UTF8PROC_LIBRARY}")
dc24h_enable_warnings(dc24h_presence)

find_package(Boost 1.74 REQUIRED CONFIG)
find_package(OpenSSL 3.0 REQUIRED COMPONENTS SSL Crypto)
find_package(Threads REQUIRED)
find_path(MARIADB_INCLUDE_DIR NAMES mysql.h PATH_SUFFIXES mariadb REQUIRED)
find_library(MARIADB_LIBRARY NAMES mariadb REQUIRED)
find_path(ARGON2_INCLUDE_DIR NAMES argon2.h REQUIRED)
find_library(ARGON2_LIBRARY NAMES argon2 REQUIRED)
find_package(nlohmann_json 3.10.5 REQUIRED)

add_library(dc24h_config STATIC src/net/config.cpp)
target_include_directories(dc24h_config PUBLIC include)
target_link_libraries(dc24h_config PRIVATE dc24h_adc)
dc24h_enable_warnings(dc24h_config)

add_library(dc24h_db STATIC src/db/store.cpp)
target_include_directories(dc24h_db PUBLIC include PRIVATE "${MARIADB_INCLUDE_DIR}")
target_link_libraries(dc24h_db
    PRIVATE dc24h_config dc24h_adc Boost::headers "${MARIADB_LIBRARY}" Threads::Threads
)
dc24h_enable_warnings(dc24h_db)

add_library(dc24h_net STATIC src/net/server.cpp src/net/connection.cpp src/net/ban_worker.cpp)
target_include_directories(dc24h_net PUBLIC include PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(dc24h_net
    PUBLIC Boost::headers
    PRIVATE dc24h_presence dc24h_config dc24h_db OpenSSL::SSL Threads::Threads
)
dc24h_enable_warnings(dc24h_net)

add_library(dc24h_admin STATIC src/admin/crypto.cpp src/admin/server.cpp src/admin/service.cpp)
target_include_directories(dc24h_admin
    PUBLIC include
    PRIVATE "${ARGON2_INCLUDE_DIR}" "${PROJECT_BINARY_DIR}/generated"
)
target_link_libraries(dc24h_admin
    PRIVATE dc24h_adc dc24h_db dc24h_net Threads::Threads OpenSSL::Crypto
        "${ARGON2_LIBRARY}" nlohmann_json::nlohmann_json
)
dc24h_enable_warnings(dc24h_admin)

add_library(dc24h_cli STATIC src/cli.cpp src/server_cli.cpp src/database_cli.cpp)
target_include_directories(dc24h_cli PUBLIC include PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_link_libraries(dc24h_cli PRIVATE dc24h_net dc24h_config dc24h_db dc24h_admin Threads::Threads)
dc24h_enable_warnings(dc24h_cli)

add_executable(dc24h-hub src/main.cpp)
target_link_libraries(dc24h-hub PRIVATE dc24h_cli)
dc24h_enable_warnings(dc24h-hub)

option(DC24H_ENABLE_FUZZING "Build the ADC parser libFuzzer target" OFF)
if(DC24H_ENABLE_FUZZING)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "DC24H_ENABLE_FUZZING requires Clang with libFuzzer")
    endif()
    target_compile_options(dc24h_adc PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=undefined
        -fno-omit-frame-pointer
    )
    target_link_options(dc24h_adc INTERFACE -fsanitize=address,undefined)

    add_executable(dc24h_adc_fuzz tests/adc/parser_fuzz.cpp)
    target_link_libraries(dc24h_adc_fuzz PRIVATE dc24h_adc)
    target_compile_options(dc24h_adc_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined
        -fno-omit-frame-pointer
    )
    target_link_options(dc24h_adc_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    dc24h_enable_warnings(dc24h_adc_fuzz)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
