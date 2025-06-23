cmake_minimum_required(VERSION 3.10)
project(maps_project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define M_PI for Windows
if(MSVC)
    add_definitions(-D_USE_MATH_DEFINES)
endif()

# Add executable
add_executable(${PROJECT_NAME} main.cpp)

# For Windows HTTP API (use the PRIVATE keyword)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winhttp)
endif()

# For OpenSSL (required for HTTPS support)
find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)