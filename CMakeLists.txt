cmake_minimum_required(VERSION 3.10)
project(ip_location_service CXX)

find_package(Crow REQUIRED)

find_package(PostgreSQL REQUIRED)

find_path(JSONCPP_INCLUDE_DIR NAMES json/json.h PATHS /usr/include/jsoncpp /usr/local/include/jsoncpp)
find_library(JSONCPP_LIBRARY NAMES jsoncpp PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
    message(FATAL_ERROR "Could not find JsonCpp. Please ensure libjsoncpp-dev is installed correctly.")
endif()

find_path(PQXX_INCLUDE_DIR NAMES pqxx/pqxx PATHS /usr/include /usr/local/include) # pqxx.h is often directly under a pqxx/ subdirectory
find_library(PQXX_LIBRARY NAMES pqxx PATHS /usr/lib/x86_64-linux-gnu /usr/local/lib)
if(NOT PQXX_INCLUDE_DIR OR NOT PQXX_LIBRARY)
    message(FATAL_ERROR "Could not find pqxx. Please ensure libpqxx-dev is installed correctly.")
endif()


add_executable(ip_location_service main.cpp)

set_property(TARGET ip_location_service PROPERTY CXX_STANDARD 20)
set_property(TARGET ip_location_service PROPERTY CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

target_link_libraries(ip_location_service
    PRIVATE
    Crow::Crow
    ${PostgreSQL_LIBRARIES}
    ${JSONCPP_LIBRARY}
    ${PQXX_LIBRARY}
)

target_include_directories(ip_location_service PRIVATE ${JSONCPP_INCLUDE_DIR} ${PQXX_INCLUDE_DIR})
