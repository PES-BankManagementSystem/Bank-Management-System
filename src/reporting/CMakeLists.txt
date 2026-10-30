# Balances, transaction history, interest crediting and statements
# (FR-9 to FR-12).
bms_add_module(reporting
    SOURCES routes.cpp interest.cpp statement.cpp
    DEPENDS bms::api bms::audit bms::auth bms::common bms::transaction)
