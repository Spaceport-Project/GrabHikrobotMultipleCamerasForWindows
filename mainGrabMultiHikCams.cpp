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
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    tp += std::chrono::milliseconds{1000};

    std::unique_ptr<HikMultipleCameras> hikroCams (new HikMultipleCameras(tp));

    signal (SIGINT, ctrlC);
    
    hikroCams->OpenDevices();

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
    if (MV_OK != hikroCams->SaveImages()) 
    {
      return -1;
    }
 
    hikroCams->CloseDevices();


    return 0;


}
