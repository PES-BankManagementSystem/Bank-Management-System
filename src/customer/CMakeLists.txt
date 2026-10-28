# Customer records: register, search, view, update and deactivate (FR-3).
bms_add_module(customer
    SOURCES routes.cpp registration.cpp records.cpp validation.cpp
    DEPENDS bms::api bms::audit bms::auth bms::common sodium)
