cmake_minimum_required(VERSION 3.20)

project(HIMSSoftware VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(hims
  src/main.cpp
  src/app.cpp
  src/core/Inventory.cpp
  src/platform/Console.cpp
  src/platform/HttpServer.cpp
)

target_include_directories(hims PRIVATE src)

if(WIN32)
  target_link_libraries(hims PRIVATE ws2_32 shell32)
endif()

add_executable(hims_tests
  tests/hims_tests.cpp
  src/core/Inventory.cpp
)

target_include_directories(hims_tests PRIVATE src)

