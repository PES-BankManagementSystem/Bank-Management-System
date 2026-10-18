# Code shared by every module: configuration, logging, version info, error types
# and the database helpers (S-03).
configure_file(version.hpp.in ${PROJECT_BINARY_DIR}/generated/common/version.hpp @ONLY)

bms_add_module(common
    SOURCES config.cpp error.cpp log.cpp db.cpp
    DEPENDS nlohmann_json::nlohmann_json SQLite::SQLite3)
target_include_directories(bms_common PUBLIC ${PROJECT_BINARY_DIR}/generated)
target_compile_definitions(bms_common PRIVATE
    BMS_DEFAULT_FRONTEND_DIR="${PROJECT_SOURCE_DIR}/frontend")
