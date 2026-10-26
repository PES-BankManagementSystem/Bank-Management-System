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

# S-07: login with user ID and password for every role.
add_executable(auth_login_test auth_login_test.cpp support/seeded_database.hpp)
target_link_libraries(auth_login_test PRIVATE
    bms::api bms::auth bms::common GTest::gtest_main bms_options)
target_compile_definitions(auth_login_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME auth_login_test COMMAND auth_login_test)

add_executable(transaction_test transaction_test.cpp)
target_link_libraries(transaction_test PRIVATE bms::transaction bms_options)
target_compile_definitions(transaction_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME transaction_test COMMAND transaction_test)

# S-08: session tokens, expiry and logout.
add_executable(auth_session_test auth_session_test.cpp support/seeded_database.hpp)
target_link_libraries(auth_session_test PRIVATE
    bms::api bms::auth bms::common GTest::gtest_main bms_options)
target_compile_definitions(auth_session_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME auth_session_test COMMAND auth_session_test)

add_executable(withdraw_test withdraw_test.cpp)
target_link_libraries(withdraw_test PRIVATE bms::transaction bms_options)
target_compile_definitions(withdraw_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME withdraw_test COMMAND withdraw_test)

# S-09: lockout after three failed logins and Administrator unlock.
add_executable(auth_lockout_test auth_lockout_test.cpp support/seeded_database.hpp)
target_link_libraries(auth_lockout_test PRIVATE
    bms::api bms::auth bms::common GTest::gtest_main bms_options)
target_compile_definitions(auth_lockout_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME auth_lockout_test COMMAND auth_lockout_test)

add_executable(module_tests module_tests.cpp support/seeded_database.hpp)
target_link_libraries(module_tests PRIVATE
    bms::account bms::api bms::audit bms::auth bms::common bms::customer
    bms::reporting bms::staff bms::transaction GTest::gtest_main bms_options)
target_compile_definitions(module_tests PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME module_tests COMMAND module_tests)

add_executable(customer_test customer_test.cpp support/seeded_database.hpp)
target_link_libraries(customer_test PRIVATE
    bms::api bms::auth bms::common bms::customer GTest::gtest_main bms_options)
target_compile_definitions(customer_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME customer_test COMMAND customer_test)

add_executable(transfer_test transfer_test.cpp)
target_link_libraries(transfer_test PRIVATE bms::transaction bms_options)
target_compile_definitions(transfer_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME transfer_test COMMAND transfer_test)

add_executable(interest_test interest_test.cpp)
target_link_libraries(interest_test PRIVATE bms::reporting bms_options)
target_compile_definitions(interest_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME interest_test COMMAND interest_test)

# S-24: Bank Officer management (create, list, update, deactivate).
add_executable(staff_test staff_test.cpp support/seeded_database.hpp)
target_link_libraries(staff_test PRIVATE
    bms::api bms::auth bms::common bms::staff GTest::gtest_main bms_options)
target_compile_definitions(staff_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME staff_test COMMAND staff_test)

# S-26: audit trail written inside each balance-changing transaction.
add_executable(audit_test audit_test.cpp support/seeded_database.hpp)
target_link_libraries(audit_test PRIVATE
    bms::audit bms::common bms::transaction GTest::gtest_main bms_options)
target_compile_definitions(audit_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME audit_test COMMAND audit_test)

# S-27: Administrator audit trail viewer.
add_executable(audit_viewer_test audit_viewer_test.cpp support/seeded_database.hpp)
target_link_libraries(audit_viewer_test PRIVATE
    bms::api bms::audit bms::auth bms::common bms::transaction GTest::gtest_main bms_options)
target_compile_definitions(audit_viewer_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME audit_viewer_test COMMAND audit_viewer_test)

# S-10: role-based access control in the core, tested through the API without the UI.
add_executable(rbac_test rbac_test.cpp support/seeded_database.hpp support/login.hpp)
target_link_libraries(rbac_test PRIVATE
    bms::api bms::audit bms::auth bms::common bms::customer bms::reporting bms::staff
    bms::transaction GTest::gtest_main bms_options)
target_compile_definitions(rbac_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME rbac_test COMMAND rbac_test)

# S-28: HTTPS with a certificate and key from configuration.
add_executable(tls_test tls_test.cpp)
target_link_libraries(tls_test PRIVATE bms::api bms::common GTest::gtest_main bms_options)
add_test(NAME tls_test COMMAND tls_test)
set_tests_properties(tls_test PROPERTIES TIMEOUT 60)

# S-28: passwords, one-time credentials and tokens never appear in the log.
add_executable(log_secrets_test log_secrets_test.cpp support/seeded_database.hpp)
target_link_libraries(log_secrets_test PRIVATE
    bms::api bms::auth bms::common bms::customer bms::staff GTest::gtest_main bms_options)
target_compile_definitions(log_secrets_test PRIVATE BMS_DB_DIR="${PROJECT_SOURCE_DIR}/db")
add_test(NAME log_secrets_test COMMAND log_secrets_test)
