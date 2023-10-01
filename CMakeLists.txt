cmake_minimum_required(VERSION 3.6)
project(KCEF)
set(CMAKE_CXX_STANDARD 20)
set(CEF_BUILD_DIR "third_party/cef")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g")

add_definitions(-DCEF_USE_SANDBOX)

include_directories(
  src
  ${CEF_BUILD_DIR}
  "third_party/kproto/include"
  "third_party/json/include")

link_directories(
  ${CEF_BUILD_DIR}/libcef_dll
  ${CEF_BUILD_DIR}/Release
  ${CEF_BUILD_DIR}/Resources
  ${CEF_BUILD_DIR}/libcef_dll_wrapper)

set(SOURCES
  src/main.cpp
  src/app.cpp
  src/app.hpp
  src/handler.cpp
  src/handler.hpp)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} -lcef -lcef_dll_wrapper -lX11 -lzmq)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN")
set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ./)
