# Accounts and account types: open, freeze, unfreeze and close (FR-4, FR-5).
bms_add_module(account
    SOURCES account_status.cpp account_types.cpp routes.cpp
    DEPENDS bms::api bms::audit bms::auth bms::common)
