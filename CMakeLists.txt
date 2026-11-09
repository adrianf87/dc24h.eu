cmake_minimum_required(VERSION 3.22)
project(dc24h VERSION 0.1.9 LANGUAGES CXX)

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

add_library(dc24h_security src/security/limits.cpp src/security/secret_file.cpp)
target_compile_features(dc24h_security PUBLIC cxx_std_20)
set_target_properties(dc24h_security PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_security PUBLIC include)
target_link_libraries(dc24h_security PRIVATE OpenSSL::Crypto)

add_library(dc24h_network src/network/tcp_server.cpp)
target_compile_features(dc24h_network PUBLIC cxx_std_20)
set_target_properties(dc24h_network PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_network PUBLIC include)
target_link_libraries(dc24h_network PUBLIC dc24h_logging dc24h_adc dc24h_adcs dc24h_security)

find_path(MARIADB_INCLUDE_DIR NAMES mysql.h PATH_SUFFIXES mariadb REQUIRED)
find_library(MARIADB_LIBRARY NAMES mariadb REQUIRED)
add_library(MariaDB::ConnectorC UNKNOWN IMPORTED)
set_target_properties(MariaDB::ConnectorC PROPERTIES
    IMPORTED_LOCATION "${MARIADB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${MARIADB_INCLUDE_DIR}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS database/migrations/0001_schema_migrations.sql)
file(READ database/migrations/0001_schema_migrations.sql DC24H_BASELINE_SQL)
file(SHA256 database/migrations/0001_schema_migrations.sql DC24H_BASELINE_SHA256)
configure_file(cmake/migration.hpp.in generated/dc24h/database/migration.hpp @ONLY)
add_library(dc24h_database src/database/database.cpp)
target_compile_features(dc24h_database PUBLIC cxx_std_20)
set_target_properties(dc24h_database PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_database PUBLIC include PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(dc24h_database PRIVATE MariaDB::ConnectorC OpenSSL::Crypto dc24h_security)

add_library(dc24h_api src/api/http.cpp src/api/server.cpp)
target_compile_features(dc24h_api PUBLIC cxx_std_20)
set_target_properties(dc24h_api PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_api PUBLIC include PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(dc24h_api PUBLIC dc24h_logging PRIVATE Threads::Threads OpenSSL::Crypto dc24h_security)

add_library(dc24h_configuration src/configuration/configuration.cpp)
target_compile_features(dc24h_configuration PUBLIC cxx_std_20)
set_target_properties(dc24h_configuration PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(dc24h_configuration PUBLIC include)
target_link_libraries(dc24h_configuration PUBLIC dc24h_network dc24h_logging dc24h_database dc24h_api PRIVATE UTF8Proc::UTF8Proc)

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

foreach(target dc24h_i18n dc24h_core dc24h_network dc24h_logging dc24h_adc dc24h_adcs dc24h_security dc24h_configuration dc24h_database dc24h_api dc24h)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endforeach()

include(GNUInstallDirs)
install(TARGETS dc24h RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config/dc24h.ini.example DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dc24h)

install(FILES database/migrations/0001_schema_migrations.sql DESTINATION ${CMAKE_INSTALL_DATADIR}/dc24h/migrations)

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
    add_executable(security_tests tests/security_tests.cpp)
    target_link_libraries(security_tests PRIVATE dc24h_security)
    set_target_properties(security_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_options(security_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME security.unit COMMAND security_tests)
    add_test(NAME security.process COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/security_process_tests.py" "$<TARGET_FILE:dc24h>")
    set_tests_properties(security.unit security.process PROPERTIES TIMEOUT 45)
    add_executable(database_tests tests/database_tests.cpp)
    target_link_libraries(database_tests PRIVATE dc24h_database)
    target_compile_options(database_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME database.unit COMMAND database_tests)
    set_tests_properties(database.unit PROPERTIES TIMEOUT 15)
    find_program(MARIADBD_EXECUTABLE mariadbd PATHS /usr/sbin)
    find_program(MARIADB_INSTALL_DB_EXECUTABLE mariadb-install-db)
    find_program(MARIADB_CLIENT_EXECUTABLE mariadb)
    find_program(MARIADB_ADMIN_EXECUTABLE mariadb-admin)
    if(MARIADBD_EXECUTABLE AND MARIADB_INSTALL_DB_EXECUTABLE AND MARIADB_CLIENT_EXECUTABLE AND MARIADB_ADMIN_EXECUTABLE)
        add_test(NAME database.process COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/database_process_tests.py"
            "$<TARGET_FILE:dc24h>" "$<TARGET_FILE:database_tests>")
        set_tests_properties(database.process PROPERTIES TIMEOUT 120
            ENVIRONMENT "PATH=/usr/sbin:/usr/bin:/sbin:/bin:$ENV{PATH}")
    else()
        message(STATUS "MariaDB server/client tools missing: database.process unavailable")
    endif()
    add_executable(api_tests tests/api_tests.cpp)
    target_link_libraries(api_tests PRIVATE dc24h_api)
    target_compile_options(api_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    add_test(NAME api.unit COMMAND api_tests)
    add_test(NAME api.process COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/api_process_tests.py" "$<TARGET_FILE:dc24h>")
    set_tests_properties(api.unit api.process PROPERTIES TIMEOUT 45)
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
