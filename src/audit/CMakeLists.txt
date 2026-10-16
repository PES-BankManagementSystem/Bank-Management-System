# Append-only audit trail of every balance change (NFR-5, SEC-4).
bms_add_module(audit
    SOURCES routes.cpp
    DEPENDS bms::api)
