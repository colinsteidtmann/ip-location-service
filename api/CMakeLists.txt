cmake_minimum_required(VERSION 3.10)
project(ip_location_service CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Crow REQUIRED)
find_package(PostgreSQL REQUIRED)

#JsonCpp
find_path(JSONCPP_INCLUDE_DIR NAMES json/json.h PATHS /usr/include/jsoncpp /usr/local/include/jsoncpp)
find_library(JSONCPP_LIBRARY NAMES jsoncpp PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
    message(FATAL_ERROR "Could not find JsonCpp. Please ensure libjsoncpp-dev is installed correctly.")
endif()

#pqxx
find_path(PQXX_INCLUDE_DIR NAMES pqxx/pqxx PATHS /usr/include /usr/local/include)
find_library(PQXX_LIBRARY NAMES pqxx PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
if(NOT PQXX_INCLUDE_DIR OR NOT PQXX_LIBRARY)
    message(FATAL_ERROR "Could not find pqxx. Please ensure libpqxx-dev is installed correctly.")
endif()

#hiredis
find_path(HIREDIS_INCLUDE_DIR NAMES hiredis/hiredis.h PATHS /usr/include /usr/local/include)
find_library(HIREDIS_LIBRARY NAMES hiredis PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
if(NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIBRARY)
    message(FATAL_ERROR "Could not find hiredis. Please ensure libhiredis-dev is installed correctly.")
endif()

#redis++
find_path(REDISPP_INCLUDE_DIR NAMES sw/redis++/redis++.h PATHS /usr/include /usr/local/include)
find_library(REDISPP_LIBRARY NAMES redis++ PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
if(NOT REDISPP_INCLUDE_DIR OR NOT REDISPP_LIBRARY)
    message(FATAL_ERROR "Could not find redis++. Please ensure it is built and installed correctly.")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${JSONCPP_INCLUDE_DIR})
include_directories(${PQXX_INCLUDE_DIR})
include_directories(${HIREDIS_INCLUDE_DIR})
include_directories(${REDISPP_INCLUDE_DIR})

# Source files
set(SOURCES
    src/main.cpp
    src/config/service_config.cpp
    src/database/database_pool.cpp
    src/handlers/api_handlers.cpp
    src/utils/rate_limiter.cpp
    src/utils/ip_validator.cpp
    src/utils/logger.cpp
)

add_executable(ip_location_service ${SOURCES})

# Link libraries
target_link_libraries(ip_location_service
    PRIVATE
    Crow::Crow
    ${PostgreSQL_LIBRARIES}
    ${JSONCPP_LIBRARY}
    ${PQXX_LIBRARY}
    ${HIREDIS_LIBRARY}
    ${REDISPP_LIBRARY}
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(ip_location_service PRIVATE -g -O0 -Wall -Wextra)
else()
    target_compile_options(ip_location_service PRIVATE -O3 -DNDEBUG)
endif()

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()