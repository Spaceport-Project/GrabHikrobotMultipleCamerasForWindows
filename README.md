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

After setting build type "Release or Debug", please run the executable "GrabMultiHikCams_Release" or "GrabMultiHikCams_Debug" depending on your choise, make sure the json file named "CameraSettings.json" which contains some camera settings, e.g., exposure time, trigger time interval, is in the same folder as the executable. 