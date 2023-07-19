#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <chrono>
#include <iostream>
#include "MvCameraControl.h"
bool g_bIsGetImage = true;
bool g_bExit = false;
unsigned int g_DeviceKey = 1;
unsigned int g_GroupKey = 1;
unsigned int g_GroupMask= 1;



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
    sleep(1);
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
        g_bIsGetImage = true;
       FPS_CALC ("Image Grabbing FPS:");
    }

}
static __stdcall void* WorkThread(void* pUser)
{
    MV_ACTION_CMD_INFO stActionCmdInfo = {0};
    MV_ACTION_CMD_RESULT_LIST stActionCmdResults = {0};

    stActionCmdInfo.nDeviceKey = g_DeviceKey;
    stActionCmdInfo.nGroupKey = g_GroupKey;
    stActionCmdInfo.nGroupMask = g_GroupMask;
    stActionCmdInfo.pBroadcastAddress = "255.255.255.255";
    stActionCmdInfo.nTimeOut = 25;
    stActionCmdInfo.bActionTimeEnable = 0;



    while(1)
    {
        if(g_bExit)
        {
            break;
        }
       // if (true == g_bIsGetImage)
        if (true)
        {
            int nRet = MV_GIGE_IssueActionCommand(&stActionCmdInfo, &stActionCmdResults);
            // int nRet = MV_CC_SetCommandValue(pUser, "TriggerSoftware");
            if(MV_OK != nRet)
            {
                printf("failed in Trigger Action 1[%x]\n", nRet);
            }
            // else
            // {
            //     g_bIsGetImage = false;
            // }
        }
        else
        {
            continue;
        }

        
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
        
       

        // Set the trigger mode to on
        nRet = MV_CC_SetEnumValue(handle, "TriggerMode", 1);
        if (MV_OK != nRet)
        {
            printf("MV_CC_SetTriggerMode fail! nRet [%x]\n", nRet);
            break;
        }
       


        nRet = MV_CC_SetEnumValue(handle,"ExposureAuto", 0);
        if (MV_OK != nRet)
        {
            printf("Set ExposureAuto fail! nRet [0x%x]\n", nRet);
            break;
        }
        nRet = MV_CC_SetFloatValue(handle,"ExposureTime", 20000.0f);
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
        nRet = MV_CC_SetBoolValue(handle,"GevIEEE1588", true);
        if (MV_OK != nRet)
        {
            printf("Set GevIEEE1588 fail! nRet [0x%x]\n", nRet);
            break;
        }

        nRet = MV_CC_SetFloatValue(handle,"Gain", 15.0f);
        if (nRet != MV_OK) printf("Cannot set gain value!.  \n ");
        
        // ch:���ô���ģʽΪon | en:Set trigger mode as on
        nRet = MV_CC_SetEnumValue(handle, "TriggerMode", 1);
        if (MV_OK != nRet)
        {
            printf("Set Trigger Mode fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:���ô���ԴΪAction1 | en:Set trigger source as Action1
        nRet = MV_CC_SetEnumValueByString(handle, "TriggerSource", "Action1");
        if (MV_OK != nRet)
        {
            printf("Set Trigger Source fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:����Action Device Key | en:Set Action Device Key
        nRet = MV_CC_SetIntValue(handle, "ActionDeviceKey", g_DeviceKey);
        if (MV_OK != nRet)
        {
            printf("Set Action Device Key fail! nRet [0x%x]\n", nRet);
            break;
        }
       

        // ch:����Action Group Key | en:Set Action Group Key100
        nRet = MV_CC_SetIntValue(handle, "ActionGroupKey", g_GroupKey);
        if (MV_OK != nRet)
        {
            printf("Set Action Group Key fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:����Action Group Mask | en:Set Action Group Mask
        nRet = MV_CC_SetIntValue(handle, "ActionGroupMask", g_GroupMask);
        if (MV_OK != nRet)
        {
            printf("Set Action Group Mask fail! nRet [0x%x]\n", nRet);
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
        pthread_t nThreadID;
        nRet = pthread_create(&nThreadID, NULL ,WorkThread , handle);
        if (nRet != 0)
        {
            printf("thread create failed.ret = %d\n",nRet);
            break;
        }
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