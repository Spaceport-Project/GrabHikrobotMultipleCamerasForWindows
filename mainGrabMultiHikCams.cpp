#include <pthread.h>
#include <iostream>
#include <vector>
#include <condition_variable>
#include <signal.h>
#include "HikMultipleCameras.h"
#include "CircularBuffer.h"

bool HikMultipleCameras::m_bExit = false;


void ctrlC (int)
{
  printf ("\nCtrl-C detected, exit condition set to true.\n");
  HikMultipleCameras::m_bExit  = true;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: %s <path/to/CameraSettings.json>\n", argv[0]);
        return -1;
    } 
    std::string cameraSettingsFile(argv[1]);
    ImageBuffer<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]>>  > >  buf;
    const int buff_size = 500;
    buf.setCapacity(buff_size);

    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    tp += std::chrono::milliseconds{1000};

    std::unique_ptr<HikMultipleCameras> hikroCams (new HikMultipleCameras(buf, tp, cameraSettingsFile));

    signal (SIGINT, ctrlC);
    
   // hikroCams->OpenDevices();
   
    hikroCams->OpenDevicesInThreads();
    hikroCams->JoinOpenDevicesInThreads();

    hikroCams->OpenThreadsTimeStampControlReset();
    hikroCams->JoinThreadsTimeStampControlReset();

    if (MV_OK != hikroCams->ConfigureCameraSettings()) 
    {
      return -1;
    }
    if (MV_OK != hikroCams->SetTriggerModeOnOff(MV_TRIGGER_MODE_ON))
    {
      return -1;
    }
   
    if (MV_OK != hikroCams->StartGrabbing()) 
    {
      return -1;
    }
    if (MV_OK != hikroCams->Save2BufferThenDisk()) 
    {
      return -1;
    }
 
    if (-1 == hikroCams->StopGrabbing())
    {
        return -1;
    } 


    // hikroCams->CloseDevices();
    hikroCams->CloseDevicesInThreads();
    hikroCams->JoinCloseDevicesInThreads();


   


    return 0;


}
