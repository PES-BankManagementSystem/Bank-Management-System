# Balances, transaction history, interest crediting and statements
# (FR-9 to FR-12).
bms_add_module(reporting
    SOURCES routes.cpp
    DEPENDS bms::api)
