# Unit tests, run with ctest.

include(FetchContent)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(googletest)

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

add_executable(db_test db_test.cpp)
target_link_libraries(db_test PRIVATE bms::common bms_options)
target_compile_definitions(db_test PRIVATE DB_PATH="${PROJECT_SOURCE_DIR}/db" BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME db_test COMMAND db_test)

add_executable(module_tests module_tests.cpp support/seeded_database.hpp)
target_link_libraries(module_tests PRIVATE
    bms::account bms::api bms::audit bms::auth bms::common bms::customer
    bms::reporting bms::staff bms::transaction GTest::gtest_main bms_options)
target_compile_definitions(module_tests PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME module_tests COMMAND module_tests)
