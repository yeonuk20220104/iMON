cmake_minimum_required(VERSION 2.8)

project(slt_gpu_light)

set(GL_SRCS
    gldemo.cpp
    gl_code_yuvinput.h
    gl_code_yuvinput.cpp)

include_directories(
    ./
    ./include
    ./include/libdrm/include/drm
    )

set(LIB m mali drm pthread)

set(CMAKE_CXX_FLAGS "-std=c++11 -o -fpermissive -pthread")

#lib
add_library(sltgpulight SHARED ${GL_SRCS})
target_link_libraries(sltgpulight ${LIB})

#demo
set(GL_DEMO_SRCS gldemo.cpp)
add_executable(slt_gpu_light ${GL_DEMO_SRCS})
target_link_libraries(slt_gpu_light sltgpulight)

install(TARGETS slt_gpu_light DESTINATION sbin)
install(PROGRAMS 02gpu DESTINATION lib/pm-utils/sleep.d)
install(PROGRAMS S02gpu DESTINATION ../etc/init.d)
install(TARGETS sltgpulight DESTINATION lib)
install(DIRECTORY gpu_data_light DESTINATION lib/slt)
