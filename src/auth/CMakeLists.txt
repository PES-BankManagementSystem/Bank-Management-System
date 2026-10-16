# Authentication and sessions: login, logout, lockout after three failed
# attempts, and role-based access control (FR-1, FR-2, NFR-4, SEC-1, SEC-2).
bms_add_module(auth
    SOURCES routes.cpp
    DEPENDS bms::api sodium)
