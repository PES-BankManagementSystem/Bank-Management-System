# Code shared by every module: configuration, version info, error types and the
# database helpers (S-03).
configure_file(version.hpp.in ${PROJECT_BINARY_DIR}/generated/common/version.hpp @ONLY)

bms_add_module(common
    SOURCES version.cpp
    DEPENDS nlohmann_json::nlohmann_json SQLite::SQLite3)
target_include_directories(bms_common PUBLIC ${PROJECT_BINARY_DIR}/generated)
