#include <stdio.h>
#include <io.h>
#include <stdlib.h>
#include <cmath>  
#include "MvCameraControl.h"
#include <thread>
#include <functional> 
#include <memory>
#include <cstring>
#include <chrono>
#include <iostream>
#include <Windows.h>
#include <mutex>
#include <condition_variable>



std::mutex m_consumeMutex;
std::condition_variable m_dataReadyCon;
bool m_bStartConsuming = false;
bool m_bStartGrabbing = false;

MV_FRAME_OUT_INFO_EX m_stImageInfo;
unsigned char *m_pSaveImageBuf = nullptr;
unsigned char *m_pDataForSaveImage = nullptr;
bool g_bExit = false;
bool m_imageOk = false;
unsigned int g_DeviceKey = 1;
unsigned int g_GroupKey = 1;
unsigned int g_GroupMask= 1;


thread_local unsigned count = 0;
thread_local double last = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
#define FPS_CALC(_WHAT_) \
do \
{ \
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

        // ch:��ӡ��ǰ���ip���û��Զ������� | en:print current ip and user defined name
        printf("CurrentIp: %d.%d.%d.%d\n" , nIp1, nIp2, nIp3, nIp4);
        printf("UserDefinedName: %s\n\n" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    }
    else
    {
        printf("Not support.\n");
    }

    return true;
}

static  unsigned int __stdcall ActionCommandWorkThread(void* pUser)
{
    int nRet = MV_OK;
    MV_ACTION_CMD_INFO stActionCmdInfo = {0};
    MV_ACTION_CMD_RESULT_LIST stActionCmdResults = {0};

    stActionCmdInfo.nDeviceKey = g_DeviceKey;
    stActionCmdInfo.nGroupKey = g_GroupKey;
    stActionCmdInfo.nGroupMask = g_GroupMask;
    stActionCmdInfo.pBroadcastAddress = "255.255.255.255";
    stActionCmdInfo.nTimeOut = 25;
    stActionCmdInfo.bActionTimeEnable = 0;

    while(!g_bExit)
    {
        //Send the PTP clock photo command
        //std::this_thread::sleep_until(m_timePoint);
        nRet = MV_GIGE_IssueActionCommand(&stActionCmdInfo,&stActionCmdResults);
        if (MV_OK != nRet)
        {
            printf("Issue Action Command fail! nRet [0x%x]\n", nRet);
            continue;
        }
        printf("NumResults = %d\r\n",stActionCmdResults.nNumResults);

        MV_ACTION_CMD_RESULT* pResults = stActionCmdResults.pResults;
        for (unsigned int i = 0;i < stActionCmdResults.nNumResults;i++)
        {
            //Print the device infomation
            printf("Ip == %s, Status == 0x%x\r\n",pResults->strDeviceAddress,pResults->nStatus);
            pResults++;
        }
    }

    return 0;
}

static  unsigned int __stdcall ReceiveImageWorkThread(void* pUser)
{
    int nRet = MV_OK;
    MV_FRAME_OUT stImageOut = {0};

    while(m_bStartGrabbing )
    {
        nRet = MV_CC_GetImageBuffer(pUser, &stImageOut, 1000);
        if (nRet == MV_OK)
        {
           { 
            //std::lock_guard<std::mutex> lk(m_consumeMutex);
            printf("Get Image Buffer: Width[%d], Height[%d], FrameNum[%d]\n", 
                stImageOut.stFrameInfo.nWidth, stImageOut.stFrameInfo.nHeight, stImageOut.stFrameInfo.nFrameNum);
            m_pSaveImageBuf = (unsigned char *) malloc(stImageOut.stFrameInfo.nFrameLen);
            if (stImageOut.pBufAddr != NULL)
            {   
    
                        memcpy(m_pSaveImageBuf, stImageOut.pBufAddr, stImageOut.stFrameInfo.nFrameLen);
                        memcpy(&(m_stImageInfo), &(stImageOut.stFrameInfo), sizeof(MV_FRAME_OUT_INFO_EX));                       
                        m_imageOk = true;
                        nRet = nRet = MV_CC_FreeImageBuffer(pUser, &stImageOut);
                        if (MV_OK != nRet)
                        {
                            printf("cannot free buffer! \n");
                            continue;
                        }
                     
            }
           }
          //  m_dataReadyCon.notify_one();
            
        }
        else
        {
            printf("Get Image fail! nRet [0x%x]\n", nRet);
        }
        if(g_bExit)
        {
            m_bStartGrabbing =false;
        }
        FPS_CALC ("Image Grabbing FPS:");

    }

    return 0;
}


static unsigned int __stdcall SaveImagesWorkThread (void* pUser)
{

        MV_SAVE_IMAGE_PARAM_EX stParam;
        memset(&stParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));
        uint64_t oldtimeStamp = 0;
        long long int oldmicroseconds = 0;
        while(1)
        {
            
            {
             
                std::unique_lock<std::mutex> lk(m_consumeMutex);
                while (!m_imageOk)
                    m_dataReadyCon.wait(lk);
                 
                if  (m_pSaveImageBuf == nullptr) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    printf("continue \n");
                    continue;
                }
                m_pDataForSaveImage = (unsigned char *) malloc (m_stImageInfo.nWidth * m_stImageInfo.nHeight * 4 + 2048);

                if (m_pDataForSaveImage == nullptr)
                {
                    break;
                }
           

                stParam.enImageType = MV_Image_Jpeg; 
                stParam.enPixelType =  m_stImageInfo.enPixelType; 
                stParam.nWidth = m_stImageInfo.nWidth;       
                stParam.nHeight = m_stImageInfo.nHeight;       
                stParam.nDataLen = m_stImageInfo.nFrameLen;
                stParam.pData = m_pSaveImageBuf;
                stParam.pImageBuffer =  m_pDataForSaveImage;
                stParam.nBufferSize = m_stImageInfo.nWidth * m_stImageInfo.nHeight * 4 + 2048;;  
                stParam.nJpgQuality = 80;  
                
                m_imageOk = false;
                
                int nRet =  MV_CC_SaveImageEx2(pUser, &stParam);

                if(nRet != MV_OK)
                {
                    printf("failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;;
                }
                char filepath[256];
               
                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
                long long int microseconds = ms.count();

                uint64_t timeStamp = (((uint64_t) m_stImageInfo.nDevTimeStampHigh) << 32) + m_stImageInfo.nDevTimeStampLow;

                uint64_t  timeDif = timeStamp - oldtimeStamp;
                uint64_t systemTimeDiff = microseconds - oldmicroseconds;
                oldtimeStamp = timeStamp; 
                oldmicroseconds = microseconds;
                
                sprintf(filepath, "Image_w%d_h%d_fn%03d.jpg", stParam.nWidth, stParam.nHeight, m_stImageInfo.nFrameNum);
                FILE* fp = fopen(filepath, "wb");
                if (fp == NULL)
                {
                    printf("fopen failed\n");
                    break;
                }
                fwrite(m_pDataForSaveImage, 1, stParam.nImageLen, fp);
                fclose(fp);
                printf("Save image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%lld ms], SystemTimeDiff[%.3f ms]\n",m_stImageInfo.nFrameNum, double(timeStamp)/100000, float(timeDif)/100000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000);

                
            }
            
            if(g_bExit)
            {
                break;
            }
            FPS_CALC ("Image Saving FPS:");

        }
  return 0;

}

int main()
{
    int nRet = MV_OK;
    void* handle = NULL;

    do 
    {
        // ch:ö���豸 | en:Enum device
        MV_CC_DEVICE_INFO_LIST stDeviceList;
        memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
        nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE, &stDeviceList);
        if (MV_OK != nRet)
        {
            printf("Enum Devices fail! nRet [0x%x]\n", nRet);
            break;
        }

        if (stDeviceList.nDeviceNum > 0)
        {
            for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++)
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

        printf("Please Input camera index(0-%d):", stDeviceList.nDeviceNum-1);
        unsigned int nIndex = 0;
        int sc = scanf("%d", &nIndex);

        if (nIndex >= stDeviceList.nDeviceNum)
        {
            printf("Input error!\n");
            break;
        }

        // ch:ѡ���豸��������� | en:Select device and create handle
        nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
        if (MV_OK != nRet)
        {
            printf("Create Handle fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:���豸 | en:Open device
        nRet = MV_CC_OpenDevice(handle);
        if (MV_OK != nRet)
        {
            printf("Open Device fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:̽��������Ѱ���С(ֻ��GigE�����Ч) | en:Detection network optimal package size(It only works for the GigE camera)
        if (stDeviceList.pDeviceInfo[nIndex]->nTLayerType == MV_GIGE_DEVICE)
        {
            int nPacketSize = MV_CC_GetOptimalPacketSize(handle);
            if (nPacketSize > 0)
            {
                nRet = MV_CC_SetIntValue(handle,"GevSCPSPacketSize",nPacketSize);
                if(nRet != MV_OK)
                {
                    printf("Warning: Set Packet Size fail nRet [0x%x]!", nRet);
                }
            }
            else
            {
                printf("Warning: Get Packet Size fail nRet [0x%x]!", nPacketSize);
            }
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

        // ch:��ʼȡ�� | en:Start grab image
        nRet = MV_CC_StartGrabbing(handle);
        if (MV_OK != nRet)
        {
            printf("Start Grabbing fail! nRet [0x%x]\n", nRet);
            break;
        }
        m_bStartGrabbing = true;

       
        unsigned int nReceiveThreadID =0;
        void* hReceiveThreadHandle = (void*) _beginthreadex( NULL , 0 , ReceiveImageWorkThread , handle, 0 , &nReceiveThreadID );
       // std::thread nReceiveImageThread(std::bind(ReceiveImageWorkThread, handle));
        

        unsigned int nActionCommandThreadID =0;
        void* hActionCommandThreadHandle = (void*) _beginthreadex( NULL , 0 , ActionCommandWorkThread , handle, 0 , &nActionCommandThreadID );
       // std::thread nActionCommandThread(std::bind(ActionCommandWorkThread, handle));
        
        unsigned int nSavingImageThreadID =0;
        void* hSavingImagesThreadHandle = (void*) _beginthreadex( NULL , 0 , SaveImagesWorkThread , handle, 0 , &nSavingImageThreadID );
        //std::thread nSavingImageThread(std::bind(SaveImagesWorkThread, handle));

        printf("Press a key to stop grabbing.\n");
        PressEnterToExit();

        g_bExit = true;
        Sleep(1000);

        // nActionCommandThread.join();
        // nReceiveImageThread.join();
        // nSavingImageThread.join();

        // ch:ֹͣȡ�� | en:Stop grab image
        nRet = MV_CC_StopGrabbing(handle);
        if (MV_OK != nRet)
        {
            printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:�ر��豸 | Close device
        nRet = MV_CC_CloseDevice(handle);
        if (MV_OK != nRet)
        {
            printf("ClosDevice fail! nRet [0x%x]\n", nRet);
            break;
        }

        // ch:���پ�� | Destroy handle
        nRet = MV_CC_DestroyHandle(handle);
        if (MV_OK != nRet)
        {
            printf("Destroy Handle fail! nRet [0x%x]\n", nRet);
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

    printf("Press a key to exit.\n");
    PressEnterToExit();

    return 0;
}
