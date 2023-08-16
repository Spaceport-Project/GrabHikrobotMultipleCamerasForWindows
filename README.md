# Hikrobot Mulltiple Cameras Grabber

A tool for grabbing and saving images from syncronized multiple Hikrobot Cameras.

# Dependencies

-[MVS](https://www.hikrobotics.com/en/machinevision/service/download) (SDK for Hikrobot Cameras)

## Build

Use the usual steps to build the application...

```
git clone 
mkdir build
cd build
cmake ..
make -j4
```
## Usage

After setting build type "Release or Debug", please run the executable "GrabMultiHikCams_Release" or "GrabMultiHikCams_Debug" with the argument of <path/to/CameraSettings.json>. 

Example Usage:
./GrabMultiHikCams_Release /home/user/Documents/CameraSettings.json
