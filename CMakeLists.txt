cmake_minimum_required(VERSION 3.0.0)
project(cppserver VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

include(CTest)
enable_testing()

# Header files directories
set(INCLUDES_DIR
    ${CMAKE_SOURCE_DIR}/client.hpp
    ${CMAKE_SOURCE_DIR}/http.hpp
    ${CMAKE_SOURCE_DIR}/mime.hpp
    ${CMAKE_SOURCE_DIR}/request.hpp
    ${CMAKE_SOURCE_DIR}/response.hpp
    ${CMAKE_SOURCE_DIR}/server.hpp
    ${CMAKE_SOURCE_DIR}/threadpool.hpp
    ${CMAKE_SOURCE_DIR}/url.hpp
    ${CMAKE_SOURCE_DIR}/router.hpp
)

set(SRCS
    client.cpp
    http.cpp
    main.cpp
    mime.cpp
    request.cpp
    response.cpp
    server.cpp
    threadpool.cpp
    url.cpp
    router.cpp
)

add_executable(cppserver ${SRCS} ${INCLUDES_DIR})

target_link_libraries(cppserver CURL::libcurl pcre2-8)
target_compile_options(cppserver PRIVATE -ggdb -Wall -Wextra -Werror -pedantic)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
