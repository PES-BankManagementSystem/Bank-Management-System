# Back-end modules, one folder per functional area (NFR-8).
add_subdirectory(common)
add_subdirectory(api)
add_subdirectory(auth)
add_subdirectory(customer)
add_subdirectory(account)
add_subdirectory(transaction)
add_subdirectory(reporting)
add_subdirectory(staff)
add_subdirectory(audit)

add_executable(bms_server main.cpp)
target_link_libraries(bms_server PRIVATE
    bms::api
    bms::auth
    bms::customer
    bms::account
    bms::transaction
    bms::reporting
    bms::staff
    bms::audit
    bms_options)
