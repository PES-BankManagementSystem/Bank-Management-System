# Accounts and account types: open, freeze, unfreeze and close (FR-4, FR-5).
bms_add_module(account
    SOURCES account.cpp
    DEPENDS bms::common)
