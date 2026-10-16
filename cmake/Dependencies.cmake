# Third-party libraries, fetched at configure time and pinned to exact versions
# (see docs/decisions/001-tech-stack.md). Archives are checked against SHA-256
# hashes so every machine builds the same code.
#
# Targets provided:
#   httplib::httplib               HTTP server/client (header-only)
#   nlohmann_json::nlohmann_json   JSON (header-only)
#   SQLite::SQLite3                SQLite, compiled from the amalgamation
#   sodium                         libsodium (Argon2id password hashing, secure RNG)

include(FetchContent)

# Don't re-check the network on every configure once the sources are present.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

# --- cpp-httplib -------------------------------------------------------------
# Optional features stay off so the build is identical on every machine; TLS is
# switched on later for HTTPS (S-28).
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_NON_BLOCKING_GETADDRINFO OFF CACHE BOOL "" FORCE)
set(HTTPLIB_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(httplib
    URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.57.1.tar.gz
    URL_HASH SHA256=5c9e56b6638eb415dc451ac57531e84931fcb50e261ca3dd74eec6b484fb4712)

# --- nlohmann/json -----------------------------------------------------------
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa)

# --- SQLite ------------------------------------------------------------------
FetchContent_Declare(sqlite3
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3530300.zip
    URL_HASH SHA256=646421e12aac110282ef8cc68f1a62d4bb15fc7b8f09da0b53e29ee690500431)

# --- libsodium ---------------------------------------------------------------
# libsodium-cmake wraps the official sources (a git submodule) in a CMake build
# that also works with MSVC.
set(SODIUM_DISABLE_TESTS ON CACHE BOOL "" FORCE)
FetchContent_Declare(sodium
    GIT_REPOSITORY https://github.com/robinlinden/libsodium-cmake.git
    GIT_TAG 9b2848dfc1b917a9410f0de9d81059b26cbfaa8d
    GIT_SHALLOW FALSE)

FetchContent_MakeAvailable(httplib json sqlite3 sodium)

# The amalgamation ships without a build script.
add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
add_library(SQLite::SQLite3 ALIAS sqlite3)
target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
target_compile_definitions(sqlite3 PUBLIC
    SQLITE_DEFAULT_FOREIGN_KEYS=1   # enforce FOREIGN KEY constraints on every connection
    SQLITE_THREADSAFE=1
    SQLITE_DQS=0                    # reject "string" used where 'string' was meant
    SQLITE_OMIT_LOAD_EXTENSION)
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3 PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()
