
// MultipleCameraDlg.cpp : implementation file
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <functional> 
#include <cmath>  
#include <memory>
#include "HikMultipleCameras.h"


// FBS Calculator
thread_local unsigned count = 0;
thread_local double last = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
#define FPS_CALC(_WHAT_, ncurrCameraIndex) \
do \
{ \
    double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); \
    ++count; \
    if (now - last >= 1.0) \
    { \
      std::cerr << "\033[1;31m";\
      std::cerr << ncurrCameraIndex<< ". Camera,"<<" Average framerate("<< _WHAT_ << "): " << double(count)/double(now - last) << " fbs." <<  "\n"; \
      std::cerr << "\033[0m";\
      count = 0; \
      last = now; \
    } \
} while(false)


// HikMultipleCameras dialog
HikMultipleCameras::HikMultipleCameras(ImageBuffer &buf, std::chrono::system_clock::time_point timePoint):
    m_buf(buf)
    , m_nDeviceNum(0)
    , m_timePoint(timePoint)
    , m_bOpenDevice(false)
    , m_bStartGrabbing(false)
    , m_bStartConsuming(false)
    , m_entered(true)
    , m_nTriggerMode(MV_TRIGGER_MODE_OFF)
   
{
	
    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    EnumDevices();

    if (m_nDeviceNum > 0)
    {
       
        m_imageOk.resize(m_nDeviceNum, false);
        m_triggeredEvent.resize(m_nDeviceNum, true);
        m_params.resize(m_nDeviceNum, {0});
        m_stImageInfo.resize(m_nDeviceNum, {0});
        m_nSaveImageBufSize.resize(m_nDeviceNum, 0);
        m_consumeMutexes = std::vector<std::mutex>(m_nDeviceNum);
        m_dataReadyCon = condVector(m_nDeviceNum);

        for (uint i = 0 ; i < m_nDeviceNum ; i++)
        {
            memset(&(m_stImageInfo[i]), 0, sizeof(MV_FRAME_OUT_INFO_EX));
            m_pDataForSaveImage.push_back(nullptr);
            m_pSaveImageBuf.push_back(nullptr);
        }

    } else 
    {
        printf("No device detected! Exiting...\n");
        exit(0);

    }
    
}


//  Set trigger mode
void HikMultipleCameras::SetTriggerMode(void)
{
    int nRet = MV_OK;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            std::cout<<"Trigger Mode nRet:"<<m_nTriggerMode<<std::endl;
            nRet = m_pcMyCamera[i]->SetEnumValue("TriggerMode", m_nTriggerMode);
            
            if (nRet != MV_OK)
            {
                printf("Set Trigger mode fail! DevIndex[%d], TriggerMode[%d], nRet[%#x]\r\n", i+1, m_nTriggerMode, nRet);
            }
        }
    }
}

//  Set trigger source
void HikMultipleCameras::SetTriggerSource(void)
{
    int nRet = MV_OK;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", m_nTriggerSource);
            if (nRet != MV_OK)
            {
                printf("Set Trigger source fail! DevIndex[%d], TriggerSource[%d], nRet[%#x]\r\n", i+1, m_nTriggerSource, nRet);
            }
            float triggerRate = 5.0f;
            float triggerInterval = 1.0f / triggerRate; 
            float triggerDelay = 0.0f;
            nRet = m_pcMyCamera[i]->SetFloatValue("TriggerDelay", triggerDelay);
            if (MV_OK != nRet)
            {
                printf("Set Trigger frequency fail! DevIndex[%d], TriggerSource[%f], nRet[%#x]\r\n", i+1, triggerInterval, nRet);
            }
        }
    }
}

// Software trigger once
void HikMultipleCameras::DoSoftwareOnce(void)
{
    int nRet = MV_OK;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            
            nRet = m_pcMyCamera[i]->CommandExecute("TriggerSoftware");
            if (nRet != MV_OK)
            {
                printf("Soft trigger fail! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            }
        }
    }
}

void HikMultipleCameras::SetAcquisitioFrameRate(float rate)
{
    int nRet = MV_OK;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            
            nRet = m_pcMyCamera[i]->SetFloatValue("AcquisitionFrameRate", rate);
            if (nRet != MV_OK)
            {
                printf("Set Frame Rate fail! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            }
        }
    }

}
// Thread Function for save images on disk for every camera
int HikMultipleCameras::ThreadConsumeFun(int nCurCameraIndex)
{
    if (m_pcMyCamera[nCurCameraIndex])
    {
        MV_SAVE_IMAGE_PARAM_EX stParam;
        memset(&stParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));
        uint64_t oldtimeStamp = 0;
        long long int oldmicroseconds = 0;
        while(m_bStartConsuming)
        {
            
            {
             
                std::unique_lock<std::mutex> lk(m_consumeMutexes[nCurCameraIndex]);
                m_dataReadyCon[nCurCameraIndex].wait(lk, [this, nCurCameraIndex] {
                    return m_imageOk[nCurCameraIndex];

                });

                if  (m_pSaveImageBuf[nCurCameraIndex] == nullptr) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    printf("continue \n");
                    continue;
                }
                m_pDataForSaveImage[nCurCameraIndex].reset(new uint8_t[m_stImageInfo[nCurCameraIndex].nWidth * m_stImageInfo[nCurCameraIndex].nHeight * 4 + 2048]);

                if (m_pDataForSaveImage[nCurCameraIndex] == nullptr)
                {
                    break;
                }
           

                stParam.enImageType = MV_Image_Jpeg; 
                stParam.enPixelType =  m_stImageInfo[nCurCameraIndex].enPixelType; 
                stParam.nWidth = m_stImageInfo[nCurCameraIndex].nWidth;       
                stParam.nHeight = m_stImageInfo[nCurCameraIndex].nHeight;       
                stParam.nDataLen = m_stImageInfo[nCurCameraIndex].nFrameLen;
                stParam.pData = m_pSaveImageBuf[nCurCameraIndex].get();
                stParam.pImageBuffer =  m_pDataForSaveImage[nCurCameraIndex].get();
                stParam.nBufferSize = m_stImageInfo[nCurCameraIndex].nWidth * m_stImageInfo[nCurCameraIndex].nHeight * 4 + 2048;;  
                stParam.nJpgQuality = 80;  
                
                m_imageOk[nCurCameraIndex] = false;
                
                int nRet =  m_pcMyCamera[nCurCameraIndex]->SaveImage(&stParam);

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

                uint64_t timeStamp = (((uint64_t) m_stImageInfo[nCurCameraIndex].nDevTimeStampHigh) << 32) + m_stImageInfo[nCurCameraIndex].nDevTimeStampLow;

                uint64_t  timeDif = timeStamp - oldtimeStamp;
                uint64_t systemTimeDiff = microseconds - oldmicroseconds;
                oldtimeStamp = timeStamp; 
                oldmicroseconds = microseconds;
                
                sprintf(filepath, "Image_%d_w%d_h%d_fn%03d.jpg", nCurCameraIndex, stParam.nWidth, stParam.nHeight, m_stImageInfo[nCurCameraIndex].nFrameNum);
                FILE* fp = fopen(filepath, "wb");
                if (fp == NULL)
                {
                    printf("fopen failed\n");
                    break;
                }
                fwrite(m_pDataForSaveImage[nCurCameraIndex].get(), 1, stParam.nImageLen, fp);
                fclose(fp);
              //  printf("%d. Camera save image  succeeded,  nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%ld ms], SystemTimeDiff[%.3f ms]\n", nCurCameraIndex,m_stImageInfo[nCurCameraIndex].nFrameNum, double(timeStamp)/100000, float(timeDif)/100000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000);

                
            }
            if (m_bExit) m_bStartConsuming = false;
            FPS_CALC ("Image Saving FPS:", nCurCameraIndex);

        }
    }
    return 0;
}

//Thread function with GetImageBuffer API
int HikMultipleCameras::ThreadGrabWithGetImageBufferFun(int nCurCameraIndex)
{
    if (m_pcMyCamera[nCurCameraIndex])
    {
        MV_FRAME_OUT stImageOut = {0};
        memset(&stImageOut, 0, sizeof(MV_FRAME_OUT));
        uint64_t oldtimeStamp = 0;
        long long int oldmicroseconds = 0;
        uint i = 0;
        while(m_bStartGrabbing)
        {
           // if (m_nTriggerMode == MV_TRIGGER_MODE_ON) m_pcMyCamera[nCurCameraIndex]->CommandExecute("TriggerSoftware");
           // std::this_thread::sleep_until(m_timePoint);

            std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();

            int nRet = m_pcMyCamera[nCurCameraIndex]->GetImageBuffer(&stImageOut, 1000);

            std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
            std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;

            if (nRet == MV_OK)
            {
                
                {
                    std::lock_guard<std::mutex> lk(m_consumeMutexes[nCurCameraIndex]);
                    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                    std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
                    long long int microseconds = ms.count();

                    uint64_t timeStamp = (((uint64_t) stImageOut.stFrameInfo.nDevTimeStampHigh) << 32) + stImageOut.stFrameInfo.nDevTimeStampLow;
                    uint64_t  timeDif = timeStamp - oldtimeStamp;
                    uint64_t systemTimeDiff = microseconds - oldmicroseconds;
                    oldtimeStamp = timeStamp; 
                    oldmicroseconds = microseconds;
                    
                    printf("%d. Camera Grab image succeeded,  nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%ld ms], SystemTimeDiff[%.3f ms]\n", nCurCameraIndex, stImageOut.stFrameInfo.nFrameNum, double(timeStamp)/100000, float(timeDif)/100000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000);

                    if (m_pSaveImageBuf[nCurCameraIndex] == nullptr  || stImageOut.stFrameInfo.nFrameLen > m_nSaveImageBufSize[nCurCameraIndex])
                    {
                       
                        m_pSaveImageBuf[nCurCameraIndex].reset(new uint8_t[stImageOut.stFrameInfo.nFrameLen]);
                        if (m_pSaveImageBuf[nCurCameraIndex] == nullptr)
                        {
                            printf("Failed to allocate memory! Exiting\n");                            
                            return -1;
                        }
                        m_nSaveImageBufSize[nCurCameraIndex] = stImageOut.stFrameInfo.nFrameLen;
                    }


                    if (stImageOut.pBufAddr != NULL)
                    {   
    
                        memcpy(m_pSaveImageBuf[nCurCameraIndex].get(), stImageOut.pBufAddr, stImageOut.stFrameInfo.nFrameLen);
                        memcpy(&(m_stImageInfo[nCurCameraIndex]), &(stImageOut.stFrameInfo), sizeof(MV_FRAME_OUT_INFO_EX));                       
                        m_imageOk[nCurCameraIndex] = true;
                        nRet = m_pcMyCamera[nCurCameraIndex]->FreeImageBuffer(&stImageOut);
                        if (MV_OK != nRet)
                        {
                            printf("cannot free buffer! \n");
                            continue;
                        }
                     
                    }
                }
                m_dataReadyCon[nCurCameraIndex].notify_one();
                m_triggeredEvent[nCurCameraIndex] = true;
                m_triggerCon.notify_one();
            
            }
            // if (i == 3 && m_nTriggerMode ==  MV_TRIGGER_MODE_ON) 
            // {
            //     SetTriggerModeOnOff(MV_TRIGGER_MODE_OFF);
            //     SetAcquisitioFrameRate(15.5f);

            // }
           // m_timePoint += std::chrono::milliseconds(50);
            if (m_bExit) m_bStartGrabbing = false;
            FPS_CALC ("Image Grabbing FPS:", nCurCameraIndex);
            i++;
        }

    }

    return 0;
}

// Thread function with GetOneFrameTimeOut API
int HikMultipleCameras::ThreadGrabWithGetOneFrameFun(int nCurCameraIndex)
{
    if (m_pcMyCamera[nCurCameraIndex])
    {
        
        uint64_t oldtimeStamp = 0;
        MV_FRAME_OUT_INFO_EX stImageOut = {0};
        memset(&stImageOut, 0, sizeof(MV_FRAME_OUT_INFO_EX));

        while(m_bStartGrabbing)
        {
           
            //std::this_thread::sleep_until(m_timePoint);
            if (m_pSaveImageBuf[nCurCameraIndex] == nullptr )
            {
                
                m_pSaveImageBuf[nCurCameraIndex].reset(new uint8_t[m_params[nCurCameraIndex].nCurValue]);
                if (m_pSaveImageBuf[nCurCameraIndex] == nullptr)
                {
                    printf("Failed to allocate memory! Exiting\n");
                    return -1;
                }
            }
            
            std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();
            int nRet = m_pcMyCamera[nCurCameraIndex]->GetOneFrame(m_pSaveImageBuf[nCurCameraIndex].get(), m_params[nCurCameraIndex].nCurValue, &stImageOut, 1000);
            std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
            std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;

            if (nRet == MV_OK)
            {
                
                
                {
                    std::lock_guard<std::mutex> lk(m_consumeMutexes[nCurCameraIndex]);
                    memcpy(&(m_stImageInfo[nCurCameraIndex]), &(stImageOut), sizeof(MV_FRAME_OUT_INFO_EX)); 
                    m_imageOk[nCurCameraIndex] = true;
                    if (m_pSaveImageBuf[nCurCameraIndex]) {
                       
                       // m_pSaveImageBuf[nCurCameraIndex].reset();

                    }
                  
                }
                m_dataReadyCon[nCurCameraIndex].notify_one();
                m_triggeredEvent[nCurCameraIndex] = true;
                m_triggerCon.notify_one();

            }
            else
            {
                printf("Get Frame out Fail! DevIndex[%d], nRet[%#x]\r\n", nCurCameraIndex, nRet);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
				continue;
            }

            if (m_bExit) m_bStartGrabbing = false;
            FPS_CALC ("Image Grabbing FPS:", nCurCameraIndex);
            //m_timePoint += std::chrono::milliseconds(25);
        }
    }

    return 0;
}


void HikMultipleCameras::EnumDevices()
{
    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    int nRet = HikCamera::EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &m_stDevList);
    if ( nRet != MV_OK || m_stDevList.nDeviceNum == 0)
    {
        printf("Find no device!\r\n");
        return;
    }
    printf("Find %d devices!\r\n", m_stDevList.nDeviceNum);
    
    m_nDeviceNum =  m_stDevList.nDeviceNum;

    for (uint i = 0; i < m_nDeviceNum; i++)
    {
       
        MV_CC_DEVICE_INFO* pDeviceInfo = m_stDevList.pDeviceInfo[i];
        if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE)
        {
            int nIp1 = ((pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
            int nIp2 = ((pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
            int nIp3 = ((pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
            int nIp4 = (pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
            // Print the IP address and user defined name of the current camera
            printf("Device Model Name: %s\n", pDeviceInfo->SpecialInfo.stGigEInfo.chModelName);
            printf("CurrentIp: %d.%d.%d.%d\n" , nIp1, nIp2, nIp3, nIp4);
          //  printf("UserDefinedName: %s\n\n" , pDeviceInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
            printf("SerialNumber: %s\n\n", pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber);
            m_mapSerials.insert(std::make_pair(i , (char *)pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber));
            m_mapModels.insert(std::make_pair(i, (char *) pDeviceInfo->SpecialInfo.stGigEInfo.chModelName));
            
        }
        else if (pDeviceInfo->nTLayerType == MV_USB_DEVICE)
        {
            
            printf("Device Model Name: %s\n", pDeviceInfo->SpecialInfo.stUsb3VInfo.chModelName);
            printf("UserDefinedName: %s\n\n", pDeviceInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
            m_mapSerials.insert(std::make_pair( i, (char *)pDeviceInfo->SpecialInfo.stUsb3VInfo.chSerialNumber));
            m_mapModels.insert(std::make_pair(i, (char *) pDeviceInfo->SpecialInfo.stUsb3VInfo.chModelName));

        }
        else {
            printf("Not support.\n");
        }    
    
        
       
    }

}


//  Initialzation, include opening device
void HikMultipleCameras::OpenDevices()
{
    if (true == m_bOpenDevice || m_nDeviceNum == 0)
    {
        return;
    }
    
    
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        m_pcMyCamera.push_back(std::make_unique<HikCamera>());
        int nRet = m_pcMyCamera[i]->Open(m_stDevList.pDeviceInfo[i]);
        if (nRet != MV_OK)
        {
            m_pcMyCamera[i].reset();
            printf("Open device failed! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            continue;
        }
        else
        {
            // Detect the optimal packet size (it is valid for GigE cameras only)
            m_bOpenDevice = true;
            if (m_stDevList.pDeviceInfo[i]->nTLayerType == MV_GIGE_DEVICE)
            {
                uint nPacketSize = 0;
                nRet = m_pcMyCamera[i]->GetOptimalPacketSize(&nPacketSize);
                if (nPacketSize > 0)
                {
                    nRet = m_pcMyCamera[i]->SetIntValue("GevSCPSPacketSize",nPacketSize);
                    if(nRet != MV_OK)
                    {
                        printf("Set Packet Size fail! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
                    }
                }
                else
                {
                    printf("Get Packet Size fail! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
                }
            }

            m_pcMyCamera[i]->SetEnumValue("ExposureAuto", 0);
            m_pcMyCamera[i]->SetFloatValue("ExposureTime", 15000.0f);
            m_pcMyCamera[i]->SetBoolValue("AcquisitionFrameRateEnable", true);
            m_pcMyCamera[i]->SetFloatValue("AcquisitionFrameRate", 10.0f); 
            
           // nRet = m_pcMyCamera[i]->SetFloatValue("TriggerDelay", 0.0f);
           // if (nRet != MV_OK) printf("Cannot set trigger delay!. %d. Camera. \n ", i);
            //m_pcMyCamera[i]->SetBoolValue("TriggerCacheEnable", true);
           // m_pcMyCamera[i]->SetEnumValue("AcquisitionMode", MV_ACQ_MODE_CONTINUOUS);
           // m_pcMyCamera[i]->SetIntValue("AcquisitionBurstFrameCount", 1);
            //m_pcMyCamera[i]->SetBoolValue("FullFrameTransmission", true);
           // m_pcMyCamera[i]->SetIntValue("GevSCPSPacketSize", 8194);
           // m_pcMyCamera[i]->SetIntValue("GevSCPD", 10);
            m_pcMyCamera[i]->SetBoolValue("GevPAUSEFrameReception", true);
            m_pcMyCamera[i]->SetBoolValue("ChunkModeActive", false);
            nRet = m_pcMyCamera[i]->SetEnumValue("EventSelector", 0x9000);
            if (nRet != MV_OK) printf("Cannot set event selector!. %d. Camera. \n ", i);
            
            // m_pcMyCamera[i]->SetEnumValue("ChunkSelector", 1);
            // m_pcMyCamera[i]->SetBoolValue("ChunkEnable", true);
           // m_pcMyCamera[i]->SetBoolValue("GevGVCPHeartbeatDisable", true);
            m_pcMyCamera[i]->SetBoolValue("GevIEEE1588", true);
            nRet = m_pcMyCamera[i]->SetFloatValue("Gain", 15.0f);
            if (nRet != MV_OK) printf("Cannot set gain value!. %d. Camera. \n ", i);
        }
        
    }


   
}

// Opening threads for resetting timestamp control
void HikMultipleCameras::OpenThreadsTimeStampControlReset()
{

    for (uint i = 0; i < m_nDeviceNum; i++)
    {
            if (m_pcMyCamera[i])
            {
               m_resetTimestampThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadTimeStampControlResetFun, this, i)));
            }
            

    }
}

// Thread function for resetting timestamp control
int HikMultipleCameras::ThreadTimeStampControlResetFun(int nCurCameraIndex) {

    if (m_pcMyCamera[nCurCameraIndex])
    {
        int nRet;
        
        if ( m_mapModels[nCurCameraIndex] == std::string("MV-CA023-10GC")) 
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(14));
        }
        nRet = m_pcMyCamera[nCurCameraIndex]->CommandExecute("GevTimestampControlReset") ;
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
        long long int microseconds = ms.count();
        printf("Time Stamp of  %d. Camera: %lld\n", nCurCameraIndex, microseconds);

        if (nRet != MV_OK) 
        {
            printf("%d. Camera, TimeStampControlReset failed! \n", nCurCameraIndex);
            return -1;
        }
       return 0; 
    }

    return -1;
}

// Join threads for resetting timestamp control
void HikMultipleCameras::JoinThreadsTimeStampControlReset()
{

    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            m_resetTimestampThreads[i]->join();
        }
    }
}

void HikMultipleCameras::TimeStampControlReset() 
{
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            int nRet;
            nRet = m_pcMyCamera[i]->CommandExecute("GevTimestampControlReset") ;
            if (nRet != MV_OK) printf("%d. Camera, TimeStampControlReset failed! \n", i);
        }
    }
}

// Setting height and width of frames
void HikMultipleCameras::SetHeightWidth(int height, int width)
{
    int nRet;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            nRet = m_pcMyCamera[i]->SetIntValue("Height", height);
            if (nRet != MV_OK){
                printf("Set height fail! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            }
            nRet = m_pcMyCamera[i]->SetIntValue("Width", width);
            if (nRet != MV_OK){
                printf("Set height fail! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            }


        }



    }
}

// Close, include destroy handle
void HikMultipleCameras::CloseDevices()
{
	StopGrabbing();

	int nRet = MV_OK;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            nRet = m_pcMyCamera[i]->Close();
            if (MV_OK != nRet)
            {
                printf("Close device fail! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            }
            else {

                printf("Close device success! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            }

            
          
        }

    }

    
}

void HikMultipleCameras::SaveToBuffer()
{
    // while(true) {
    //         std::cout<<std::endl;
    //         {
    //             std::unique_lock<std::mutex> lk(m_grabMutex);
    //             m_dataReadyCon.wait(lk, [this] {
    //                 bool sum=true;
    //                 int i = 0;
    //                 for (; i < m_images.size(); i++){
                        
    //                         sum = sum && m_imageOk[i];

    //                 }
    //                 if (i == 0 ) return false;
    //                 else return sum;
    
    //             });

    //             for (int i =0 ; i < m_images.size(); i++){
    //                m_imageOk[i]= false;    

    //             }
               
    //             if (!m_buf.pushBack ( m_images) )
    //             {
                   
    //                 printf ("Warning! Buffer was full, overwriting data!\n");
                    
    //             }
    //             FPS_CALC ("image callback.", m_buf);
            

    //       }  

    //     std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // }


}

// Start grabbing
void HikMultipleCameras::StartGrabbing()
{
    if (m_bStartGrabbing == true)
    {        
        return;
    }

    int nRet = MV_OK;

    for (int i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            memset(&(m_stImageInfo[i]), 0, sizeof(MV_FRAME_OUT_INFO_EX));
            
            
            nRet = m_pcMyCamera[i]->StartGrabbing();
            if (MV_OK != nRet)
            {
                printf("Start grabbing fail! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            }
            m_bStartGrabbing = true;

            memset(&m_params[i], 0, sizeof(MVCC_INTVALUE));
            nRet =  m_pcMyCamera[i]->GetIntValue("PayloadSize", &m_params[i]);
            if (nRet != MV_OK) {
                printf("Get PayloadSize failed! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
                continue;;
            }

           
            m_grabThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadGrabWithGetImageBufferFun, this, i)));
           
            if (m_grabThreads[i] == nullptr)
            {
                printf("Create grab thread fail! DevIndex[%d]\r\n", i+1);
            }
        }
    }

// Consider including the line below incase of saving in buffer.
    // m_grabThread = new std::thread(std::bind(&HikMultipleCameras::SaveToBuffer, this));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_triggerThread = std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadTriggerWithMutexFun, this));

}

// Thread function for triggering
int HikMultipleCameras::ThreadTriggerFun() 
{

    while(true) 
    {
        std::this_thread::sleep_until(m_timePoint);
        bool sum = true;
        int i = 0;
        for (; i < m_nDeviceNum; i++)
                sum = sum && m_triggeredEvent[i];
        if (i == 0 ) sum = false;
        
        if (m_nTriggerMode == MV_TRIGGER_MODE_ON && sum == true) 
        {
            for (uint i = 0; i < m_nDeviceNum; i++)
            {
                    if (m_pcMyCamera[i])
                    {
                        m_pcMyCamera[i]->CommandExecute("TriggerSoftware");
                        m_triggeredEvent[i] = false;
                    }

            }
           
        }
        
        m_timePoint += std::chrono::milliseconds(40);
        if (m_bExit) break;
    }
    return 0;
}

// Thread function for triggering with mutex
int HikMultipleCameras::ThreadTriggerWithMutexFun() 
{
    
    while(true) 
    {
        
        {
           // std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();

            std::unique_lock<std::mutex> lk(m_triggerMutex);

            m_triggerCon.wait(lk,  [this] {
                bool sum = true;
                int i = 0;
                for (; i < this->m_nDeviceNum; i++)
                    sum = sum && this->m_triggeredEvent[i];
                if (i == 0 ) sum = false;
                return sum;
            });

            //std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
            //std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
           // std::this_thread::sleep_until(m_timePoint);

            if (m_nTriggerMode == MV_TRIGGER_MODE_ON && m_entered) 
            {
                m_entered = false;
                for (uint i = 0; i < m_nDeviceNum; i++)
                {
                        if ( m_pcMyCamera[i])
                        {
                            //m_pcMyCamera[i]->SetFloatValue("TriggerDelay", -10000.0f);
                            m_pcMyCamera[i]->TriggerExecuteSoftware();
                            m_triggeredEvent[i] = false;
                        }

                }
                
            }
           

        }
       
        m_timePoint += std::chrono::milliseconds(65);
        if (m_bExit) break;
       
    }
    return 0;
}



// Stop grabbing
void HikMultipleCameras::StopGrabbing()
{
    if ( m_bOpenDevice == false || m_bStartGrabbing == false)
    {        
        return;
    }

    m_triggerThread->join();

    int nRet = MV_OK;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            
            m_grabThreads[i]->join();
            m_consumeThreads[i]->join();
           
            nRet = m_pcMyCamera[i]->StopGrabbing();
            if (MV_OK != nRet)
            {
                printf("Stop grabbing fail! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            } else {

                printf("Stop grabbing success! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            }

          
        }
        
    }

}

// Set trigger mode on or off
void HikMultipleCameras::SetTriggerModeOnOff(int triggerMode)
{
    if (m_nDeviceNum == 0) return;

    m_nTriggerMode = triggerMode;
    SetTriggerMode();

    if (m_nTriggerMode == MV_TRIGGER_MODE_ON)  {
        printf("Only Software Trigger is supported! Setting Trigger Software Mode. \n");
        SetTriggerSoftwareMode();
    }
}

// Software trigger
void HikMultipleCameras::SetTriggerSoftwareMode()
{
    m_nTriggerSource = MV_TRIGGER_SOURCE_SOFTWARE;
    SetTriggerSource();

}



// Software trigger
void HikMultipleCameras::SetSoftwareOnce()
{
    if ( m_bStartGrabbing == false)
    {
        printf("Please start grabbing first!\r\n");
        return;
    }

    DoSoftwareOnce();
}
// Save Images of Cameras in threads
void HikMultipleCameras::SaveImages()
{
    if (false == m_bStartGrabbing)
    {        
        return;
    }

    for (int i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {

            m_bStartConsuming = true;
            m_consumeThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadConsumeFun, this, i)));

        }
    }


}   



