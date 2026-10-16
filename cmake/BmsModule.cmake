# bms_add_module(<name> SOURCES <files...> [DEPENDS <targets...>])
#
# Declares one back-end module (NFR-8) as a static library named bms_<name>,
# with the alias bms::<name>. Headers are included relative to src/, e.g.
#   #include "account/account_service.hpp"
function(bms_add_module name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;DEPENDS")
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "bms_add_module(${name}) needs at least one source file")
    endif()

    add_library(bms_${name} STATIC ${ARG_SOURCES})
    add_library(bms::${name} ALIAS bms_${name})
    target_include_directories(bms_${name} PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(bms_${name} PUBLIC ${ARG_DEPENDS} PRIVATE bms_options)
endfunction()
