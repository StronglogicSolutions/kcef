cmake_minimum_required(VERSION 3.6)
project(KCEF)
set(CMAKE_CXX_STANDARD 20)
set(CEF_BUILD_DIR "third_party/cef")
include_directories(src ${CEF_BUILD_DIR})

set(SOURCE_FILES
  src/main.cpp
)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -std=c++20 -g")

link_directories(
  ${CEF_BUILD_DIR}/libcef_dll
  ${CEF_BUILD_DIR}/Release
  ${CEF_BUILD_DIR}/Resources
  ${CEF_BUILD_DIR}/libcef_dll_wrapper
)

add_definitions(-DCEF_USE_SANDBOX)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} -lcef -lcef_dll_wrapper)
