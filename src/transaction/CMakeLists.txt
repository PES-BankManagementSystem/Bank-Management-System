# Transaction engine: deposits, withdrawals and transfers as atomic units of
# work, with minimum-balance and daily-limit rules (FR-6, FR-7, FR-8, NFR-3).
bms_add_module(transaction
    SOURCES transaction.cpp
    DEPENDS bms::common)
