#include <stdio.h>
#include <string.h>
// #include <io.h>
#include <stdlib.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <crtdefs.h>
#include <process.h>
#include <Windows.h>
#include "MvCameraControl.h"

bool g_bIsGetImage = true;
bool g_bExit = false;
std::chrono::system_clock::time_point m_timePoint = std::chrono::system_clock::now();

#define FPS_CALC(_WHAT_) \
do \
{ \
    static unsigned count = 0;\
    static double last = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();\
    double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); \
    ++count; \
    if (now - last >= 1.0) \
    { \
      std::cerr << "\033[1;31m";\
      std::cerr << "Average framerate("<< _WHAT_ << "): " << double(count)/double(now - last) << " fbs." <<  "\n"; \
      std::cerr << "\033[0m";\
      count = 0; \
      last = now; \
    } \
}while(false)

// Wait for the user to press Enter to stop grabbing or end the program
void PressEnterToExit(void)
{
    int c;
    while ( (c = getchar()) != '\n' && c != EOF );
    fprintf( stderr, "\nPress enter to exit.\n");
    while( getchar() != '\n');
    g_bExit = true;
    Sleep(1);
}
bool PrintDeviceInfo(MV_CC_DEVICE_INFO* pstMVDevInfo)
{
    if (NULL == pstMVDevInfo)
    {
        printf("The Pointer of pstMVDevInfo is NULL!\n");
        return false;
    }
    if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE)
    {
        int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
        int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
        int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
        int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
        // Print the IP address and user defined name of the current camera
        printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chModelName);
        printf("CurrentIp: %d.%d.%d.%d\n" , nIp1, nIp2, nIp3, nIp4);
        printf("UserDefinedName: %s\n\n" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    }
    else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
    {
        printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chModelName);
        printf("UserDefinedName: %s\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
    }
    else
    {
        printf("Not support.\n");
    }
    return true;
}
void __stdcall ImageCallBackEx(unsigned char * pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser)
{   
    if (pFrameInfo)
    {
        printf("GetOneFrame, Width[%d], Height[%d], nFrameNum[%d]\n", 
            pFrameInfo->nWidth, pFrameInfo->nHeight, pFrameInfo->nFrameNum);
        FPS_CALC ("Image Grabbing FPS:");
    } else {
        printf("Frame is not captured.\n");
    }

    
}

 static unsigned int __stdcall WorkThread(void* pUser)
{
    while(1)
    {
        if(g_bExit)
        {
            break;
        }
        std::this_thread::sleep_until(m_timePoint);
       
        int nRet = MV_CC_SetCommandValue(pUser, "TriggerSoftware");
        if(MV_OK != nRet)
        {
            printf("failed in TriggerSoftware[%x]\n", nRet);
        }
       
        m_timePoint += std::chrono::milliseconds(25);

    }
    return NULL;
}

int main()
{
    int nRet = MV_OK;
    void* handle = NULL;
    
    do 
    {
        MV_CC_DEVICE_INFO_LIST stDeviceList;
        memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
        // Enumerate devices
        nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
        if (MV_OK != nRet)
        {
            printf("MV_CC_EnumDevices fail! nRet [%x]\n", nRet);
            break;
        }
        if (stDeviceList.nDeviceNum > 0)
        {
            for (int i = 0; i < stDeviceList.nDeviceNum; i++)
            {
                printf("[device %d]:\n", i);
                MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
                if (NULL == pDeviceInfo)
                {
                    break;
                } 
                PrintDeviceInfo(pDeviceInfo);            
            }  
        } 
        else
        {
            printf("Find No Devices!\n");
            break;
        }
        printf("Please Intput camera index: ");
        unsigned int nIndex = 0;
        int sc = scanf("%d", &nIndex);
        if (nIndex >= stDeviceList.nDeviceNum)
        {
            printf("Intput error!\n");
            break;
        }

        m_timePoint += std::chrono::milliseconds{3000};

        // Create a handle for the selected device
        nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
        if (MV_OK != nRet)
        {
            printf("MV_CC_CreateHandle fail! nRet [%x]\n", nRet);
            break;
        }
        // Open the device
        nRet = MV_CC_OpenDevice(handle);
        if (MV_OK != nRet)
        {
            printf("MV_CC_OpenDevice fail! nRet [%x]\n", nRet);
            break;
        }
        // Detect the optimal packet size (it is valid for GigE cameras only)
        if (stDeviceList.pDeviceInfo[nIndex]->nTLayerType == MV_GIGE_DEVICE)
        {
            int nPacketSize = MV_CC_GetOptimalPacketSize(handle);
            if (nPacketSize > 0)
            {
                nRet = MV_CC_SetIntValue(handle,"GevSCPSPacketSize",nPacketSize);
                if(nRet != MV_OK)
                {
                    printf("Warning: Set Packet Size fail nRet [0x%x]!\n", nRet);
                }
            }
            else
            {
                printf("Warning: Get Packet Size fail nRet [0x%x]!\n", nPacketSize);
            }
        }
        
        
        nRet = MV_CC_SetEnumValue(handle,"ExposureAuto", 0);
        if (MV_OK != nRet)
        {
            printf("Set ExposureAuto fail! nRet [0x%x]\n", nRet);
            break;
        }
        nRet = MV_CC_SetFloatValue(handle,"ExposureTime", 15000.0f);
        if (MV_OK != nRet)
        {
            printf("Set Exposure Time fail! nRet [0x%x]\n", nRet);
            break;
        }
        nRet = MV_CC_SetBoolValue(handle,"AcquisitionFrameRateEnable", true);
        if (MV_OK != nRet)
        {
            printf("Set AcquisitionFrameRateEnable fail! nRet [0x%x]\n", nRet);
            break;
        }
        nRet = MV_CC_SetFloatValue(handle,"AcquisitionFrameRate", 100.0f); 
        if (MV_OK != nRet)
        {
            printf("Set AcquisitionFrameRate fail! nRet [0x%x]\n", nRet);
            break;
        }
        
        // Set the trigger mode to on
        nRet = MV_CC_SetEnumValue(handle, "TriggerMode", 1);
        if (MV_OK != nRet)
        {
            printf("MV_CC_SetTriggerMode fail! nRet [%x]\n", nRet);
            break;
        }
        // Set the trigger source
        nRet = MV_CC_SetEnumValue(handle, "TriggerSource", MV_TRIGGER_SOURCE_SOFTWARE);
        if (MV_OK != nRet)
        {
            printf("MV_CC_SetTriggerSource fail! nRet [%x]\n", nRet);
            break;
        }

   

        // Register the image callback function
        nRet = MV_CC_RegisterImageCallBackEx(handle, ImageCallBackEx, handle);
        if (MV_OK != nRet)
        {
            printf("MV_CC_RegisterImageCallBackEx fail! nRet [%x]\n", nRet);
            break; 
        }
        // Start grabbing images
        nRet = MV_CC_StartGrabbing(handle);
        if (MV_OK != nRet)
        {
            printf("MV_CC_StartGrabbing fail! nRet [%x]\n", nRet);
            break;
        }
        unsigned int nThreadID =0;
         void* hThreadHandle = (void*) _beginthreadex( NULL , 0 , WorkThread , handle, 0 , &nThreadID );

        // pthread_t nThreadID;
        // nRet = pthread_create(&nThreadID, NULL ,WorkThread , handle);
        // if (nRet != 0)
        // {
        //     printf("thread create failed.ret = %d\n",nRet);
        //     break;
        // }
        PressEnterToExit(); 
        // Stop grabbing images
        nRet = MV_CC_StopGrabbing(handle);
        if (MV_OK != nRet)
        {
            printf("MV_CC_StopGrabbing fail! nRet [%x]\n", nRet);
            break;
        }
        // Shut down the device
        nRet = MV_CC_CloseDevice(handle);
        if (MV_OK != nRet)
        {
            printf("MV_CC_CloseDevice fail! nRet [%x]\n", nRet);
            break;
        }
        // Destroy the handle
        nRet = MV_CC_DestroyHandle(handle);
        if (MV_OK != nRet)
        {
            printf("MV_CC_DestroyHandle fail! nRet [%x]\n", nRet);
            break;
        }
    } while (0);
    if (nRet != MV_OK)
    {
        if (handle != NULL)
        {
            MV_CC_DestroyHandle(handle);
            handle = NULL;
        }
    }
    printf("exit\n");
    return 0;
}