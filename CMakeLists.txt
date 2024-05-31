cmake_minimum_required(VERSION 3.0)

project(gdb_server)

option(SMM_TSAN "Собирать в режиме ThreadSanitizer" OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(SMM_TSAN)
        add_compile_options(-fsanitize=thread)
        add_link_options(-fsanitize=thread)
    else()
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
endif()


add_library(uemu_v8 SHARED IMPORTED)
set_property(TARGET uemu_v8 PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libuemu_v8.so)

set(SOURCES src/tcp-connection.cpp  src/packet.cpp src/request.cpp src/utility.cpp)

find_package(Threads REQUIRED)
add_executable(server ${SOURCES} src/main.cpp)
target_link_libraries(server  uemu_v8 Threads::Threads)

enable_testing()
find_package(GTest 1.8 CONFIG REQUIRED COMPONENTS gtest, gmock)
include(GoogleTest)
add_executable(tests ${SOURCES} tests/gtest.cpp)
target_link_libraries(tests uemu_v8 GTest::gmock GTest::gtest pthread Threads::Threads)

add_test(server_test tests)

