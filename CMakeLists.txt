cmake_minimum_required(VERSION 3.24...4.0)

project(BankManagementSystem
    VERSION 0.1.0
    DESCRIPTION "Bank Management System mini-project"
    LANGUAGES C CXX)

# Executables land in build/bin (build/bin/<Config> with Visual Studio).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(BUILD_TESTING "Build the unit tests" ON)
option(BMS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(cmake/Dependencies.cmake)
include(cmake/BmsModule.cmake)

# Compiler settings shared by every first-party target.
add_library(bms_options INTERFACE)
target_compile_features(bms_options INTERFACE cxx_std_17)
if(MSVC)
    target_compile_options(bms_options INTERFACE /W4 /permissive- /utf-8 /EHsc)
    target_compile_definitions(bms_options INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    if(BMS_WARNINGS_AS_ERRORS)
        target_compile_options(bms_options INTERFACE /WX)
    endif()
else()
    target_compile_options(bms_options INTERFACE -Wall -Wextra -Wpedantic)
    if(BMS_WARNINGS_AS_ERRORS)
        target_compile_options(bms_options INTERFACE -Werror)
    endif()
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
