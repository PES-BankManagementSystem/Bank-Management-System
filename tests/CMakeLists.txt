# Unit tests, run with ctest. S-06 adds the test framework and a sample test
# per module.

# Smoke test: the server binary runs and reports the project version.
add_test(NAME server_reports_version COMMAND bms_server --version)
set_tests_properties(server_reports_version PROPERTIES
    PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")
