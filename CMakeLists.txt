cmake_minimum_required(VERSION 3.22)
project(dc24h VERSION 0.1.6 LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "dc24h currently targets Linux (Debian / Ubuntu).")
endif()

set(DC24H_VERSION "alpha-${PROJECT_VERSION}")
configure_file(cmake/version.hpp.in generated/dc24h/version.hpp @ONLY)
find_package(Threads REQUIRED)
find_package(OpenSSL 1.1.1 REQUIRED COMPONENTS SSL Crypto)

add_library(dc24h_i18n src/i18n/messages.cpp)
target_compile_features(dc24h_i18n PUBLIC cxx_std_20)
set_target_properties(dc24h_i18n PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_i18n PUBLIC include PRIVATE languages)

add_library(dc24h_logging src/logging/logger.cpp)
target_compile_features(dc24h_logging PUBLIC cxx_std_20)
set_target_properties(dc24h_logging PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_logging PUBLIC include)
target_link_libraries(dc24h_logging PUBLIC Threads::Threads dc24h_i18n)

find_path(UTF8PROC_INCLUDE_DIR NAMES utf8proc.h REQUIRED)
find_library(UTF8PROC_LIBRARY NAMES utf8proc REQUIRED)
add_library(UTF8Proc::UTF8Proc UNKNOWN IMPORTED)
set_target_properties(UTF8Proc::UTF8Proc PROPERTIES
    IMPORTED_LOCATION "${UTF8PROC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${UTF8PROC_INCLUDE_DIR}")
target_link_libraries(dc24h_logging PRIVATE UTF8Proc::UTF8Proc)
add_library(dc24h_adc src/adc/parser.cpp)
target_compile_features(dc24h_adc PUBLIC cxx_std_20)
set_target_properties(dc24h_adc PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_adc PUBLIC include)
target_link_libraries(dc24h_adc PRIVATE UTF8Proc::UTF8Proc)

add_library(dc24h_adcs src/adcs/tls.cpp)
target_compile_features(dc24h_adcs PUBLIC cxx_std_20)
set_target_properties(dc24h_adcs PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_adcs PUBLIC include)
target_link_libraries(dc24h_adcs PUBLIC OpenSSL::SSL OpenSSL::Crypto)

add_library(dc24h_network src/network/tcp_server.cpp)
target_compile_features(dc24h_network PUBLIC cxx_std_20)
set_target_properties(dc24h_network PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_network PUBLIC include)
target_link_libraries(dc24h_network PUBLIC dc24h_logging dc24h_adc dc24h_adcs)

add_library(dc24h_configuration src/configuration/configuration.cpp)
target_compile_features(dc24h_configuration PUBLIC cxx_std_20)
set_target_properties(dc24h_configuration PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_configuration PUBLIC include)
target_link_libraries(dc24h_configuration PUBLIC dc24h_network dc24h_logging PRIVATE UTF8Proc::UTF8Proc)

add_library(dc24h_core
    src/core/server.cpp
    src/core/termination_signals.cpp
)
target_compile_features(dc24h_core PUBLIC cxx_std_20)
set_target_properties(dc24h_core PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_core PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(dc24h_core PUBLIC Threads::Threads dc24h_network dc24h_logging)
target_compile_definitions(dc24h_core PRIVATE _POSIX_C_SOURCE=200809L)

add_executable(dc24h src/main.cpp)
target_link_libraries(dc24h PRIVATE dc24h_core dc24h_configuration)
set_target_properties(dc24h PROPERTIES CXX_EXTENSIONS OFF)

foreach(target dc24h_i18n dc24h_core dc24h_network dc24h_logging dc24h_adc dc24h_adcs dc24h_configuration dc24h)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endforeach()

include(GNUInstallDirs)
install(TARGETS dc24h RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/dc24h.ini.example DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dc24h)

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
    add_executable(network_tests tests/network_tests.cpp)
    target_link_libraries(network_tests PRIVATE dc24h_core)
    set_target_properties(network_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(network_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME network.unit COMMAND network_tests)
    add_test(NAME network.process COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/network_process_tests.py" "$<TARGET_FILE:dc24h>")
    add_executable(adc_tests tests/adc_tests.cpp)
    target_link_libraries(adc_tests PRIVATE dc24h_adc)
    set_target_properties(adc_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(adc_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME adc.parser COMMAND adc_tests)
    add_test(NAME adc.process COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/adc_process_tests.py" "$<TARGET_FILE:dc24h>")
    add_executable(adcs_unit_tests tests/adcs_unit_tests.cpp)
    target_link_libraries(adcs_unit_tests PRIVATE dc24h_adcs)
    set_target_properties(adcs_unit_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(adcs_unit_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME adcs.crypto COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/adcs_unit_runner.py" "$<TARGET_FILE:adcs_unit_tests>")
    add_test(NAME adcs.process COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/adcs_process_tests.py" "$<TARGET_FILE:dc24h>")
    set_tests_properties(adcs.crypto adcs.process PROPERTIES TIMEOUT 45)
    add_executable(configuration_tests tests/configuration_tests.cpp)
    target_link_libraries(configuration_tests PRIVATE dc24h_configuration)
    set_target_properties(configuration_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(configuration_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME configuration.unit COMMAND configuration_tests)
    add_test(NAME configuration.process COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/configuration_process_tests.py" "$<TARGET_FILE:dc24h>")
    set_tests_properties(configuration.unit configuration.process PROPERTIES TIMEOUT 45)
    add_executable(logging_tests tests/logging_tests.cpp)
    target_link_libraries(logging_tests PRIVATE dc24h_logging UTF8Proc::UTF8Proc)
    set_target_properties(logging_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(logging_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME logging.unit COMMAND logging_tests)
    add_test(NAME logging.process COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/logging_process_tests.py" "$<TARGET_FILE:dc24h>")
    set_tests_properties(logging.unit logging.process PROPERTIES TIMEOUT 45)
    find_program(NCDC_EXECUTABLE ncdc)
    if(NCDC_EXECUTABLE)
        add_test(NAME adc.ncdc COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/ncdc_smoke_test.py"
            "$<TARGET_FILE:dc24h>" "${NCDC_EXECUTABLE}")
        add_test(NAME adcs.ncdc COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/ncdc_adcs_test.py"
            "$<TARGET_FILE:dc24h>" "${NCDC_EXECUTABLE}")
        set_tests_properties(adc.ncdc adcs.ncdc PROPERTIES TIMEOUT 30)
    else()
        message(STATUS "ncdc not found: adc.ncdc and adcs.ncdc smoke tests are unavailable")
    endif()
    set_tests_properties(adc.parser adc.process PROPERTIES TIMEOUT 30)
    set_tests_properties(core.lifecycle core.process network.unit PROPERTIES TIMEOUT 30)
    set_tests_properties(network.process PROPERTIES TIMEOUT 45)
endif()
