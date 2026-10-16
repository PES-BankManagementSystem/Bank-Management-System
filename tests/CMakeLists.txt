# Unit tests, run with ctest. S-06 adds the test framework and a sample test
# per module; the tests below use the minimal support/check.hpp until then.

# Smoke test: the server binary runs and reports the project version.
add_test(NAME server_reports_version COMMAND bms_server --version)
set_tests_properties(server_reports_version PROPERTIES
    PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")

add_executable(config_test config_test.cpp)
target_link_libraries(config_test PRIVATE bms::common bms_options)
add_test(NAME config_test COMMAND config_test)

add_executable(api_server_test api_server_test.cpp)
target_link_libraries(api_server_test PRIVATE bms::api bms_options)
add_test(NAME api_server_test COMMAND api_server_test)
set_tests_properties(api_server_test PROPERTIES TIMEOUT 60)
