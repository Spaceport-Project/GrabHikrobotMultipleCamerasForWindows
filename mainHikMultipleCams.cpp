#include <pthread.h>
#include <vector>
#include <condition_variable>
#include <signal.h>
#include "HikMultipleCameras.h"
#include "CircularBuffer.h"
#define CAMERA_NUM   1

bool HikMultipleCameras::m_bExit = false;


void ctrlC (int)
{
  printf ("\nCtrl-C detected, exit condition set to true.\n");
  HikMultipleCameras::m_bExit  = true;
}

int main(int argc, char *arvg[]) {
   
    ImageBuffer  buf;
    const int buff_size = 200;
	buf.setCapacity(buff_size);

    HikMultipleCameras *hikroCams = new HikMultipleCameras(buf);
    signal (SIGINT, ctrlC);
    hikroCams->EnumDevices();
    hikroCams->OpenDevices();
    hikroCams->SetTriggerModeOnOff(MV_TRIGGER_MODE_ON);
   // hikroCams->SetTriggerSoftwareMode();
 
    hikroCams->StartGrabbing();
    hikroCams->SaveImages();
    hikroCams->CloseDevices();

    delete hikroCams;




}
