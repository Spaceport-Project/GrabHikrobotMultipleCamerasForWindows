cmake_minimum_required(VERSION 3.5)

project(GrabMultiHikrobotCameras VERSION 0.1 LANGUAGES C CXX)
set(CMAKE_GENERATOR "Ninja")
# if (CMAKE_BUILD_TYPE)
#     unset(CMAKE_BUILD_TYPE CACHE)
# endif()
# set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build (Debug, Release)")
message("CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_SOURCES
        mainGrabMultiHikCams.cpp
        src/HikCamera.cpp 
        src/HikMultipleCameras.cpp 
        
       
)
# Boost 
#find_package(Boost REQUIRED)

# Hikrobot Camera
#link_directories(/opt/MVS/lib/64)
link_directories("C:/Program Files (x86)/MVS/Development/Libraries/win64"  "C:/ffmpeg-win64-gpl-shared/lib" )
SET(PROJECT_INCLUDE_DIRECTORIES ${PROJECT_INCLUDE_DIRECTORIES} "C:/Program Files (x86)/MVS/Development/Includes")
SET(PROJECT_SOURCES ${PROJECT_SOURCES}
    "C:/Program Files (x86)/MVS/Development/Includes/MvCameraControl.h"
)
SET(PROJECT_LIBS  MvCameraControl)
set (Boost_INCLUDE_DIRS "C:/boost_1_83/")
set (ffmpeg_INCLUDE_DIRS  "C:/ffmpeg-win64-gpl-shared/include")
set(GrabMultiHikCams GrabMultiHikCams_${CMAKE_BUILD_TYPE})
add_executable(${GrabMultiHikCams}  ${PROJECT_SOURCES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${GrabMultiHikCams}  PRIVATE DEBUG)
endif()
target_include_directories(${GrabMultiHikCams}  PRIVATE ${PROJECT_INCLUDE_DIRECTORIES} ${Boost_INCLUDE_DIRS} ${ffmpeg_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include )
target_link_libraries(${GrabMultiHikCams}  PRIVATE ${PROJECT_LIBS} avformat avcodec swscale avutil)

add_executable(GetImageBuffer  mainGetImageBuffer.cpp)
target_include_directories(GetImageBuffer PRIVATE ${PROJECT_INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/include )
target_link_libraries(GetImageBuffer PRIVATE ${PROJECT_LIBS})

# add_executable(SoftTrigger  mainSoftTrigger.cpp)
# target_include_directories(SoftTrigger PRIVATE ${PROJECT_INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/include )
# target_link_libraries(SoftTrigger PRIVATE ${PROJECT_LIBS})

add_executable(SoftTriggerNoFlag  mainSoftTriggerNoFlag.cpp)
target_include_directories(SoftTriggerNoFlag PRIVATE ${PROJECT_INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/include )
target_link_libraries(SoftTriggerNoFlag PRIVATE ${PROJECT_LIBS})

add_executable(TriggerWithPTP  mainTriggerWithPTP.cpp)
target_include_directories(TriggerWithPTP PRIVATE ${PROJECT_INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/include )
target_link_libraries(TriggerWithPTP PRIVATE ${PROJECT_LIBS})

add_executable(TriggerWithPTP1  mainTriggerWithPTP1.cpp)
target_include_directories(TriggerWithPTP1 PRIVATE ${PROJECT_INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/include )
target_link_libraries(TriggerWithPTP1 PRIVATE ${PROJECT_LIBS})

add_executable(TriggerWithPTP2  mainTriggerWithPTP2.cpp)
target_include_directories(TriggerWithPTP2 PRIVATE ${PROJECT_INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/include )
target_link_libraries(TriggerWithPTP2 PRIVATE ${PROJECT_LIBS})

add_executable(SetIpCameras  mainSetIpCameras.cpp src/HikCamera.cpp)
target_include_directories(SetIpCameras PRIVATE ${PROJECT_INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/include )
target_link_libraries(SetIpCameras PRIVATE ${PROJECT_LIBS})

add_executable(ConnectViaIPAddress  mainConnectViaIPAddress.cpp src/HikCamera.cpp)
target_include_directories(ConnectViaIPAddress PRIVATE ${PROJECT_INCLUDE_DIRECTORIES} ${CMAKE_SOURCE_DIR}/include )
target_link_libraries(ConnectViaIPAddress PRIVATE ${PROJECT_LIBS})

add_executable(ReadMp4Write2Disk  mainReadMp4Write2Disk.cpp src/HikCamera.cpp)
target_include_directories(ReadMp4Write2Disk PRIVATE ${PROJECT_INCLUDE_DIRECTORIES} ${ffmpeg_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include )
target_link_libraries(ReadMp4Write2Disk PRIVATE ${PROJECT_LIBS} avformat avcodec swscale avutil)