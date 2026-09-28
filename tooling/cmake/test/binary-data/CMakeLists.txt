cmake_minimum_required(VERSION 3.18)
project(tanh_binary_data_test CXX)
set(CMAKE_CXX_STANDARD 17)
enable_testing()
include(${CMAKE_CURRENT_LIST_DIR}/../../binary-data.cmake)

tanh_add_binary_data(Assets
    NAMESPACE Assets
    HEADER_NAME Assets.h
    SOURCES assets/greeting.txt assets/noise.bin)
add_executable(assets_app main.cpp)
target_link_libraries(assets_app PRIVATE Assets)
add_test(NAME assets_app COMMAND assets_app)
