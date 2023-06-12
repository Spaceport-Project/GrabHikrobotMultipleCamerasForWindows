cmake_minimum_required(VERSION 3.5)

project(GrabMultiHikrobotCameras VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(PROJECT_SOURCES
        main.cpp      
)


# Hikrobot Camera
  
link_directories(/opt/MVS/lib/64)
SET(PROJECT_INCLUDE_DIRECTORIES ${PROJECT_INCLUDE_DIRECTORIES} /opt/MVS/include)
SET(PROJECT_SOURCES ${PROJECT_SOURCES}
    /opt/MVS/include/MvCameraControl.h
)
SET(PROJECT_LIBS  MvCameraControl)

add_executable(GrabMultiHikrobotCameras  ${PROJECT_SOURCES})
target_include_directories(GrabMultiHikrobotCameras PRIVATE ${PROJECT_INCLUDE_DIRECTORIES})
target_link_libraries(GrabMultiHikrobotCameras PRIVATE ${PROJECT_LIBS})
