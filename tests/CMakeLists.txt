include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(BUILD_GMOCK OFF)
set(INSTALL_GTEST OFF)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    TLS_VERIFY ON
)
FetchContent_MakeAvailable(googletest)

add_executable(dc24h_cli_tests cli_test.cpp)
target_link_libraries(dc24h_cli_tests PRIVATE dc24h_cli GTest::gtest_main)
dc24h_enable_warnings(dc24h_cli_tests)

include(GoogleTest)
gtest_discover_tests(dc24h_cli_tests)

add_executable(dc24h_adc_tests
    adc/parser_test.cpp
    adc/protocol_test.cpp
    adc/session_test.cpp
)
target_link_libraries(dc24h_adc_tests PRIVATE dc24h_adc GTest::gtest_main)
dc24h_enable_warnings(dc24h_adc_tests)
gtest_discover_tests(dc24h_adc_tests)

add_executable(dc24h_presence_tests hub/presence_test.cpp hub/peer_routing_test.cpp)
target_link_libraries(dc24h_presence_tests PRIVATE dc24h_presence GTest::gtest_main)
dc24h_enable_warnings(dc24h_presence_tests)
gtest_discover_tests(dc24h_presence_tests)

add_executable(dc24h_net_tests net/server_test.cpp)
target_link_libraries(dc24h_net_tests
    PRIVATE dc24h_net dc24h_adc GTest::gtest_main OpenSSL::SSL Threads::Threads
)
dc24h_enable_warnings(dc24h_net_tests)
gtest_discover_tests(dc24h_net_tests PROPERTIES TIMEOUT 12)

add_test(NAME hub.starts COMMAND dc24h-hub)
