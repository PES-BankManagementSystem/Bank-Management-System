# Customer records: register, search, view, update and deactivate (FR-3).
bms_add_module(customer
    SOURCES routes.cpp registration.cpp validation.cpp
    DEPENDS bms::api bms::auth bms::common sodium)
