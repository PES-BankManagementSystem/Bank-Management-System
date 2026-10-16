# HTTP API layer (SADS 2.2): server bootstrap, router, JSON error format and
# access log. Modules register their endpoints through api::Router.
bms_add_module(api
    SOURCES access_log.cpp errors.cpp router.cpp server.cpp
    DEPENDS bms::common httplib::httplib)
