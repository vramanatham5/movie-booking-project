cmake_minimum_required(VERSION 3.16)
project(CineBook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(cinebook-server src/main.cpp)
target_compile_definitions(cinebook-server PRIVATE CINEBOOK_WEB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/web")

if(WIN32)
  target_link_libraries(cinebook-server PRIVATE ws2_32)
endif()
