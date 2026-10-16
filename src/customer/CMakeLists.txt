# Customer records: register, search, view, update and deactivate (FR-3).
bms_add_module(customer
    SOURCES routes.cpp
    DEPENDS bms::api)
