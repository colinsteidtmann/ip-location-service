cmake_minimum_required(VERSION 3.14)
project(IpLocationServiceTests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Crow REQUIRED)
find_package(PostgreSQL REQUIRED)

include(FetchContent)
enable_testing()

# GoogleTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)

# JsonCpp
find_path(JSONCPP_INCLUDE_DIR NAMES json/json.h PATHS /usr/include/jsoncpp /usr/local/include/jsoncpp)
find_library(JSONCPP_LIBRARY NAMES jsoncpp PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
  message(FATAL_ERROR "Could not find JsonCpp. Please install libjsoncpp-dev.")
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
  message(FATAL_ERROR "Could not find hiredis. Please install libhiredis-dev.")
endif()

#redis++
find_path(REDISPP_INCLUDE_DIR NAMES sw/redis++/redis++.h PATHS /usr/include /usr/local/include)
find_library(REDISPP_LIBRARY NAMES redis++ PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
if(NOT REDISPP_INCLUDE_DIR OR NOT REDISPP_LIBRARY)
  message(FATAL_ERROR "Could not find redis++. Please build and install it.")
endif()

# Shared sources
set(SHARED_SOURCES
    ../src/config/service_config.cpp
    ../src/database/database_pool.cpp
    ../src/handlers/api_handlers.cpp
    ../src/utils/rate_limiter.cpp
    ../src/utils/ip_validator.cpp
    ../src/utils/logger.cpp
)

# Test sources
set(TEST_SOURCES
    test_main.cpp
    test_logger.cpp
    test_ip_validator.cpp
    test_rate_limiter.cpp
    test_api_handlers.cpp
)

add_executable(unit_tests ${TEST_SOURCES} ${SHARED_SOURCES})

target_include_directories(unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${JSONCPP_INCLUDE_DIR}
    ${PQXX_INCLUDE_DIR}
    ${HIREDIS_INCLUDE_DIR}
    ${REDISPP_INCLUDE_DIR}
)

target_link_libraries(unit_tests PRIVATE
    gtest
    gtest_main
    Crow::Crow
    ${PostgreSQL_LIBRARIES}
    ${JSONCPP_LIBRARY}
    ${PQXX_LIBRARY}
    ${HIREDIS_LIBRARY}
    ${REDISPP_LIBRARY}
    Threads::Threads
)

target_compile_options(unit_tests PRIVATE -g -O0 -Wall -Wextra)

add_test(NAME UnitTests COMMAND unit_tests)
