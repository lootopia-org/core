cmake_minimum_required(VERSION 3.20)

project(Core
    VERSION 1.0.0
    DESCRIPTION "Lootopia Core C Module"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include_directories(${CMAKE_SOURCE_DIR}/inc)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS 
"${CMAKE_SOURCE_DIR}/src/*.c"
)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/inc/*.h")

find_package(Threads REQUIRED)

find_package(RdKafka CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(libwebsockets CONFIG REQUIRED)


add_definitions(-D_GNU_SOURCE)
add_definitions(-DHAVE_RDKAFKA)
add_definitions(-DHAVE_CURL)
add_definitions(-DHAVE_MICROHTTPD)
add_definitions(-DHAVE_LIBWEBSOCKETS)
add_definitions(-DHAVE_GRPC)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Threads::Threads
        RdKafka::rdkafka
        RdKafka::rdkafka++
        CURL::libcurl
        websockets
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -O3)
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY inc/ DESTINATION include FILES_MATCHING PATTERN "*.h")

if(EXISTS ${CMAKE_SOURCE_DIR}/.env)
    file(COPY ${CMAKE_SOURCE_DIR}/.env
            DESTINATION ${CMAKE_BINARY_DIR})
endif()

message(STATUS "")
message(STATUS "============= Lootopia Core =============")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "==========================================")
