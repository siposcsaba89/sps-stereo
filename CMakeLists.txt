#Project
cmake_minimum_required(VERSION 2.4)
project(spsstereo)

## Flags
#set(CMAKE_CXX_FLAGS_RELEASE "-Wall -O3 -msse4.2")
#if (APPLE)
#set(CMAKE_CXX_FLAGS_RELEASE "-Wall -O3 -msse4.2 -stdlib=libstdc++")
#endif (APPLE)
#set(CMAKE_BUILD_TYPE Release)

find_package(OpenCV REQUIRED)


# SPS-Stereo
file(GLOB SPSSTEREO_SRC_FILES "*.cpp")
add_executable(spsstereo ${SPSSTEREO_SRC_FILES})
target_include_directories(spsstereo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(spsstereo ${OpenCV_LIBRARIES})
