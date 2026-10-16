# Accounts and account types: open, freeze, unfreeze and close (FR-4, FR-5).
bms_add_module(account
    SOURCES routes.cpp
    DEPENDS bms::api)
