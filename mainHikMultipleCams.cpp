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
    //long long int postime;
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    tp += std::chrono::milliseconds{3000};
  

    ImageBuffer  buf;
    const int buff_size = 200;
    buf.setCapacity(buff_size);

    HikMultipleCameras *hikroCams = new HikMultipleCameras(buf, tp);
    signal (SIGINT, ctrlC);
    hikroCams->EnumDevices();
    hikroCams->OpenDevices();
    hikroCams->OpenThreadsTimeStampControlReset();
    hikroCams->JoinThreadsTimeStampControlReset();

    hikroCams->SetHeightWidth(1200,1920);
    hikroCams->SetTriggerModeOnOff(MV_TRIGGER_MODE_ON);
   
    hikroCams->StartGrabbing();
    hikroCams->SaveImages();
    hikroCams->CloseDevices();

    delete hikroCams;




}
