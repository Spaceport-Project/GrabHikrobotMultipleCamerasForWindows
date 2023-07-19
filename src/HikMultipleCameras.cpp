
// MultipleCameraDlg.cpp : implementation file
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <functional> 
#include <cmath>  
#include <memory>
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/bind/bind.hpp>
#include "HikMultipleCameras.h"


#ifdef DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) do {} while (0)
#endif

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
HikMultipleCameras::HikMultipleCameras(std::chrono::system_clock::time_point timePoint):
      m_nDeviceNum(0)
    , m_timePoint(timePoint)
    , m_bOpenDevice(false)
    , m_bStartGrabbing(false)
    , m_bStartConsuming(false)
    , m_entered(true)
    , m_nTriggerMode(MV_TRIGGER_MODE_OFF)
    , m_nDeviceKey(1)
    , n_nGroupKey(1)
    , m_nGroupMask(1)
    , m_sTriggerSource("")
    , m_nTriggerTimeInterval(0)
   
{
	
    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    memset(&m_actionCMDInfo, 0, sizeof(MV_ACTION_CMD_INFO));
    memset(&m_actionCMDResList, 0, sizeof(MV_ACTION_CMD_RESULT_LIST));
   


    EnumDevices();
 
    if (m_nDeviceNum > 0)
    {
       
        m_bImagesOk.resize(m_nDeviceNum, false);
        m_params.resize(m_nDeviceNum, {0});
        m_stImagesInfo.resize(m_nDeviceNum, {0});
        m_nSaveImagesBufSize.resize(m_nDeviceNum, 0);
        m_mConsumeMutexes = std::vector<std::mutex>(m_nDeviceNum);
        m_cDataReadyCon = condVector(m_nDeviceNum);
        

        for (uint i = 0 ; i < m_nDeviceNum ; i++)
        {
            memset(&(m_stImagesInfo[i]), 0, sizeof(MV_FRAME_OUT_INFO_EX));
            m_pDataForSaveImages.push_back(nullptr);
            m_pSaveImagesBuf.push_back(nullptr);
        }

    } else 
    {
        printf("No device detected! Exiting...\n");
        exit(0);

    }
    
}


//  Set trigger mode
int HikMultipleCameras::SetTriggerMode(void)
{
    int nRet = -1;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            nRet = m_pcMyCamera[i]->SetEnumValue("TriggerMode", m_nTriggerMode);
            
            if (nRet != MV_OK)
            {
                printf("Set Trigger mode fail! DevIndex[%d], TriggerMode[%d], nRet[%#x]\r\n. Exiting...", i, m_nTriggerMode, nRet);
                return nRet;
            }
        }
    }
    return nRet;
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
             
                std::unique_lock<std::mutex> lk(m_mConsumeMutexes[nCurCameraIndex]);
                m_cDataReadyCon[nCurCameraIndex].wait(lk, [this, nCurCameraIndex] {
                    return m_bImagesOk[nCurCameraIndex];

                });

                if  (m_pSaveImagesBuf[nCurCameraIndex] == nullptr) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    printf("continue \n");
                    continue;
                }
                m_pDataForSaveImages[nCurCameraIndex].reset(new uint8_t[m_stImagesInfo[nCurCameraIndex].nWidth * m_stImagesInfo[nCurCameraIndex].nHeight * 4 + 2048]);

                if (m_pDataForSaveImages[nCurCameraIndex] == nullptr)
                {
                    break;
                }
           

                stParam.enImageType = MV_Image_Jpeg; 
                stParam.enPixelType =  m_stImagesInfo[nCurCameraIndex].enPixelType; 
                stParam.nWidth = m_stImagesInfo[nCurCameraIndex].nWidth;       
                stParam.nHeight = m_stImagesInfo[nCurCameraIndex].nHeight;       
                stParam.nDataLen = m_stImagesInfo[nCurCameraIndex].nFrameLen;
                stParam.pData = m_pSaveImagesBuf[nCurCameraIndex].get();
                stParam.pImageBuffer =  m_pDataForSaveImages[nCurCameraIndex].get();
                stParam.nBufferSize = m_stImagesInfo[nCurCameraIndex].nWidth * m_stImagesInfo[nCurCameraIndex].nHeight * 4 + 2048;;  
                stParam.nJpgQuality = 80;  
                
                m_bImagesOk[nCurCameraIndex] = false;
                
                int nRet =  m_pcMyCamera[nCurCameraIndex]->SaveImage(&stParam);

                if(nRet != MV_OK)
                {
                    printf("Failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;;
                }
                char filepath[256];
               
                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
                long long int microseconds = ms.count();

                uint64_t timeStamp = (((uint64_t) m_stImagesInfo[nCurCameraIndex].nDevTimeStampHigh) << 32) + m_stImagesInfo[nCurCameraIndex].nDevTimeStampLow;

                uint64_t  timeDif = timeStamp - oldtimeStamp;
                uint64_t systemTimeDiff = microseconds - oldmicroseconds;
                oldtimeStamp = timeStamp; 
                oldmicroseconds = microseconds;
                
                sprintf(filepath, "Image_%d_w%d_h%d_fn%03d.jpg", nCurCameraIndex, stParam.nWidth, stParam.nHeight, m_stImagesInfo[nCurCameraIndex].nFrameNum);
                FILE* fp = fopen(filepath, "wb");
                if (fp == NULL)
                {
                    printf("fopen failed\n");
                    break;
                }
                fwrite(m_pDataForSaveImages[nCurCameraIndex].get(), 1, stParam.nImageLen, fp);
                fclose(fp);
                printf("%d. Camera, Save image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%ld ms], SystemTimeDiff[%.3f ms]\n", nCurCameraIndex,m_stImagesInfo[nCurCameraIndex].nFrameNum, double(timeStamp)/100000, float(timeDif)/100000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000);

                
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
    int nRet = -1;
    if (m_pcMyCamera[nCurCameraIndex])
    {
        MV_FRAME_OUT stImageOut = {0};
        memset(&stImageOut, 0, sizeof(MV_FRAME_OUT));
        uint64_t oldtimeStamp = 0;
        long long int oldmicroseconds = 0;
        while(m_bStartGrabbing)
        {
            
            std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();           
            nRet = m_pcMyCamera[nCurCameraIndex]->GetImageBuffer(&stImageOut, 1000);
            std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
            DEBUG_PRINT("Grabbing duration in DevIndex[%d]= %ld[ms]", nCurCameraIndex, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );
            
            if (nRet == MV_OK)
            {
                
                {
                    std::lock_guard<std::mutex> lk(m_mConsumeMutexes[nCurCameraIndex]);

                    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                    std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
                    long long int microseconds = ms.count();

                    uint64_t timeStamp = (((uint64_t) stImageOut.stFrameInfo.nDevTimeStampHigh) << 32) + stImageOut.stFrameInfo.nDevTimeStampLow;
                    uint64_t  timeDif = timeStamp - oldtimeStamp;
                    u_int64_t hostTimeStamp = stImageOut.stFrameInfo.nHostTimeStamp;
                    uint64_t systemTimeDiff = microseconds - oldmicroseconds;
                    oldtimeStamp = timeStamp; 
                    oldmicroseconds = microseconds;
                    
                    DEBUG_PRINT("DevIndex[%d], Grab image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%ld ms], SystemTimeDiff[%.3f ms], HostTimeStamp[%ld ms]\n", nCurCameraIndex, stImageOut.stFrameInfo.nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000, hostTimeStamp);

                    if (m_pSaveImagesBuf[nCurCameraIndex] == nullptr  || stImageOut.stFrameInfo.nFrameLen > m_nSaveImagesBufSize[nCurCameraIndex])
                    {
                       
                        m_pSaveImagesBuf[nCurCameraIndex].reset(new uint8_t[stImageOut.stFrameInfo.nFrameLen]);
                        if (m_pSaveImagesBuf[nCurCameraIndex] == nullptr)
                        {
                            printf("Failed to allocate memory! Exiting\n");                            
                            return -1;
                        }
                        m_nSaveImagesBufSize[nCurCameraIndex] = stImageOut.stFrameInfo.nFrameLen;
                    }


                    if (stImageOut.pBufAddr != NULL)
                    {   
    
                        memcpy(m_pSaveImagesBuf[nCurCameraIndex].get(), stImageOut.pBufAddr, stImageOut.stFrameInfo.nFrameLen);
                        memcpy(&(m_stImagesInfo[nCurCameraIndex]), &(stImageOut.stFrameInfo), sizeof(MV_FRAME_OUT_INFO_EX));                       
                        m_bImagesOk[nCurCameraIndex] = true;
                        nRet = m_pcMyCamera[nCurCameraIndex]->FreeImageBuffer(&stImageOut);
                        if (MV_OK != nRet)
                        {
                            printf("cannot free buffer! \n");
                            return nRet;
                        }
                     
                    }
                }
                m_cDataReadyCon[nCurCameraIndex].notify_one();
             
            } else {

                printf("Get Image Buffer fail! DevIndex[%d], nRet[%#x], \n", nCurCameraIndex, nRet);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            
           
            if (m_bExit) m_bStartGrabbing = false;
            FPS_CALC ("Image Grabbing FPS:", nCurCameraIndex);
        }

    }
    if (nRet == -1) 
    {
        printf("There is something wrong with the device opened in ThreadGrabWithGetImageBufferFun! DevIndex[%d] \n", nCurCameraIndex);
    }
    return nRet;
}

// Thread function with GetOneFrameTimeOut API
int HikMultipleCameras::ThreadGrabWithGetOneFrameFun(int nCurCameraIndex)
{
    int nRet = -1;
    if (m_pcMyCamera[nCurCameraIndex])
    {
        
        uint64_t oldtimeStamp = 0;
        MV_FRAME_OUT_INFO_EX stImageOut = {0};
        memset(&stImageOut, 0, sizeof(MV_FRAME_OUT_INFO_EX));

        while(m_bStartGrabbing)
        {
           
            if (m_pSaveImagesBuf[nCurCameraIndex] == nullptr )
            {
                
                m_pSaveImagesBuf[nCurCameraIndex].reset(new uint8_t[m_params[nCurCameraIndex].nCurValue]);
                if (m_pSaveImagesBuf[nCurCameraIndex] == nullptr)
                {
                    printf("Failed to allocate memory! Exiting\n");
                    return -1;
                }
            }
            
            std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();
            nRet = m_pcMyCamera[nCurCameraIndex]->GetOneFrame(m_pSaveImagesBuf[nCurCameraIndex].get(), m_params[nCurCameraIndex].nCurValue, &stImageOut, 1000);
            std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
            DEBUG_PRINT("Grabbing duration in DevIndex[%d]= %ld[ms]", nCurCameraIndex, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );

            if (nRet == MV_OK)
            {
                
                
                {
                    std::lock_guard<std::mutex> lk(m_mConsumeMutexes[nCurCameraIndex]);
                    memcpy(&(m_stImagesInfo[nCurCameraIndex]), &(stImageOut), sizeof(MV_FRAME_OUT_INFO_EX)); 
                    m_bImagesOk[nCurCameraIndex] = true;
                    if (m_pSaveImagesBuf[nCurCameraIndex]) {
                       
                       // m_pSaveImageBuf[nCurCameraIndex].reset();

                    }
                  
                }
                m_cDataReadyCon[nCurCameraIndex].notify_one();
                
            }
            else
            {
                printf("Get Frame out Fail! DevIndex[%d], nRet[%#x]\r\n", nCurCameraIndex, nRet);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
				continue;
            }

            if (m_bExit) m_bStartGrabbing = false;
            FPS_CALC ("Image Grabbing FPS:", nCurCameraIndex);
        }
    }
    if (nRet == -1) 
    {
        printf("There is something wrong with the device opened in ThreadGrabWithGetImageBufferFun! DevIndex[%d] \n", nCurCameraIndex);
    }
    return nRet;
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
            DEBUG_PRINT("Device Model Name: %s\n", pDeviceInfo->SpecialInfo.stGigEInfo.chModelName);
            DEBUG_PRINT("CurrentIp: %d.%d.%d.%d\n" , nIp1, nIp2, nIp3, nIp4);
          //  printf("UserDefinedName: %s\n\n" , pDeviceInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
            DEBUG_PRINT("SerialNumber: %s\n\n", pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber);
            m_mapSerials.insert(std::make_pair(i , (char *)pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber));
            m_mapModels.insert(std::make_pair(i, (char *) pDeviceInfo->SpecialInfo.stGigEInfo.chModelName));
            
        }
        else if (pDeviceInfo->nTLayerType == MV_USB_DEVICE)
        {
            
            DEBUG_PRINT("Device Model Name: %s\n", pDeviceInfo->SpecialInfo.stUsb3VInfo.chModelName);
            DEBUG_PRINT("UserDefinedName: %s\n\n", pDeviceInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
            m_mapSerials.insert(std::make_pair( i, (char *)pDeviceInfo->SpecialInfo.stUsb3VInfo.chSerialNumber));
            m_mapModels.insert(std::make_pair(i, (char *) pDeviceInfo->SpecialInfo.stUsb3VInfo.chModelName));

        }
        else {
            printf("Camera not supported!\n");
        }    
    
        
       
    }

}


//  Initialzation, include opening device
void HikMultipleCameras::OpenDevices()
{
    if (true == m_bOpenDevice || m_nDeviceNum == 0)
    {
        printf("'m_bOpenDevice'set to true Or 'm_nDeviceNum' set to 0! Exiting from OpenDevices... \n");
        return;
    }
    
    
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        m_pcMyCamera.push_back(std::make_unique<HikCamera>());
        int nRet = m_pcMyCamera[i]->Open(m_stDevList.pDeviceInfo[i]);
        if (nRet != MV_OK)
        {
            m_pcMyCamera[i].reset();
            printf("Open device failed! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
            return;
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
                        printf("Set Packet Size fail! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
                    }
                }
                else
                {
                    printf("Get Packet Size fail! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
                }
            }

            
        }
        
    }


   
}

int HikMultipleCameras::ConfigureCameraSettings()
{
    boost::property_tree::ptree pt;

    // Read the JSON file
    std::ifstream file("./CameraSettings.json");
    if (!file.good()) 
    {
        printf("Error in opening 'CameraSettings.json' file! Exiting... \n");
        return -1;
    }

    boost::property_tree::read_json(file, pt);
    m_sTriggerSource = pt.get<std::string>("TriggerSource");
    m_nTriggerTimeInterval = pt.get<int>("TriggerTimeInterval");
    int height = pt.get<int>("Height");
    int width = pt.get<int>("Width");
    int exposureAuto = pt.get<int>("ExposureAuto");
    float exposureTime = pt.get<float>("ExposureTime");
    bool acquisitionFrameRateEnable = pt.get<bool>("AcquisitionFrameRateEnable");
    float acquisitionFrameRate = pt.get<float>("AcquisitionFrameRate");
    bool gevPAUSEFrameReception = pt.get<bool>("GevPAUSEFrameReception");
    bool gevIEEE1588 = pt.get<bool>("GevIEEE1588");
    float gain = pt.get<float>("Gain");
   
    
    file.close();
    
 
    
    int nRet = -1;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i]) 
        {
            nRet = m_pcMyCamera[i]->SetIntValue("Height", height);
            if (nRet != MV_OK){
                printf("Cannot set Height fail! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet ;
            }
            nRet = m_pcMyCamera[i]->SetIntValue("Width", width);
            if (nRet != MV_OK){
                printf(" Cannot set Width! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet ;
            }

            nRet = m_pcMyCamera[i]->SetEnumValue("ExposureAuto", exposureAuto);
            if (nRet != MV_OK)
            {
                printf("Cannot set Exposure Auto value! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet ;
            }

            nRet = m_pcMyCamera[i]->SetFloatValue("ExposureTime", exposureTime);
            if (nRet != MV_OK)
            {
                printf("Cannot set Exposure Time value! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet;
            }
            nRet = m_pcMyCamera[i]->SetBoolValue("AcquisitionFrameRateEnable", acquisitionFrameRateEnable);
            if (nRet != MV_OK)
            {
                printf("Cannot set Acquisition FrameRate Enable!. DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet ;
            }

            nRet = m_pcMyCamera[i]->SetFloatValue("AcquisitionFrameRate", acquisitionFrameRate); 
            if (nRet != MV_OK)
            {
                printf("Cannot set Acquisition Frame Rate value! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet;
            }
        
            nRet = m_pcMyCamera[i]->SetBoolValue("GevPAUSEFrameReception", gevPAUSEFrameReception);
            if (nRet != MV_OK)
            {
                printf("Cannot set GevPAUSEFrameReception Acquisition Enable! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet;
            }
        
            nRet = m_pcMyCamera[i]->SetBoolValue("GevIEEE1588", gevIEEE1588);
            if (nRet != MV_OK)
            {
                printf("Cannot set  GevIEEE1588 Enable! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet;
            }
            nRet = m_pcMyCamera[i]->SetFloatValue("Gain", gain);

            if (nRet != MV_OK) 
            {
                printf("Cannot set  Gain Value! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet;
            }
           
            
        }
    }
    if (nRet == -1) 
    {
        printf("There is something wrong with the number of opened devices in ConfigureCameraSettings! \n");
    }
    return nRet;

}

// Opening threads for resetting timestamp control
void HikMultipleCameras::OpenThreadsTimeStampControlReset()
{

    for (uint i = 0; i < m_nDeviceNum; i++)
    {
            if (m_pcMyCamera[i])
            {
               m_tResetTimestampThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadTimeStampControlResetFun, this, i)));
            }
            

    }
}

// Thread function for resetting timestamp control
int HikMultipleCameras::ThreadTimeStampControlResetFun(int nCurCameraIndex) {

    if (m_pcMyCamera[nCurCameraIndex])
    {
        int nRet;
        
        // if ( m_mapModels[nCurCameraIndex] == std::string("MV-CA023-10GC")) 
        // {
        //    // std::this_thread::sleep_for(std::chrono::milliseconds(14));
        // }
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
            m_tResetTimestampThreads[i]->join();
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



// Close, include destroy handle
void HikMultipleCameras::CloseDevices()
{
  
    if (MV_OK != StopGrabbing()){
        printf("Cannot stop grabbing for all cameras!. Trying to close them... \n ");
    }

	int nRet = -1;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            nRet = m_pcMyCamera[i]->Close();
            if (MV_OK != nRet)
            {
                printf("Close device fail! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
            }
            else {

                printf("Close device success! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
            }

            
          
        }

    }

    
}

void HikMultipleCameras::SaveToBuffer()
{
    // while(true) {
    //         std::cout<<std::endl;
    //         {
    //             std::unique_lock<std::mutex> lk(m_mGrabMutex);
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
int HikMultipleCameras::StartGrabbing()
{
    if (m_bStartGrabbing == true)
    {        
        printf("'m_bStartGrabbing' already set to true! Exiting... \n");
        return -1;
    }

   
    int nRet = -1;

    for (int i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            memset(&(m_stImagesInfo[i]), 0, sizeof(MV_FRAME_OUT_INFO_EX));
            
            
            nRet = m_pcMyCamera[i]->StartGrabbing();
            if (MV_OK != nRet)
            {
                printf("Start grabbing fail! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }
            m_bStartGrabbing = true;

            memset(&m_params[i], 0, sizeof(MVCC_INTVALUE));
            nRet =  m_pcMyCamera[i]->GetIntValue("PayloadSize", &m_params[i]);
            if (nRet != MV_OK) {
                printf("Get PayloadSize failed! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }

           
            m_tGrabThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadGrabWithGetImageBufferFun, this, i)));
           
            if (m_tGrabThreads[i] == nullptr)
            {
                printf("Create grab thread fail! DevIndex[%d]. Exiting...\r\n", i);
                return -1;
            }
        }
    }
    
    std::this_thread::sleep_until(m_timePoint + std::chrono::milliseconds(10));
    if (m_nTriggerMode == MV_TRIGGER_MODE_ON)
    {
        if (m_sTriggerSource == "Action1")
            m_tTriggerThread = std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadTriggerGigActionCommandFun, this));
        else if (m_sTriggerSource == "Software")
            m_tTriggerThread = std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadSoftwareTriggerFun, this));
        else {
            printf("Only GigE Action Command Trigger and Softare Trigger supported! Exiting... \n");
            return -1;
        }
    }

// Consider including the line below incase of saving in buffer.
    // m_grabThread = new std::thread(std::bind(&HikMultipleCameras::SaveToBuffer, this));
   
    
    if (nRet == -1) 
    {
        printf("There is something wrong with the number of opened devices in StartGrabbing! \n");
    }
    return nRet;

}

// Thread function for triggering
int HikMultipleCameras::ThreadSoftwareTriggerFun() 
{

    while(true) 
    {
      
        std::this_thread::sleep_until(m_timePoint);
        
        for (uint i = 0; i < m_nDeviceNum; i++)
        {
                if (m_pcMyCamera[i])
                {
                    m_pcMyCamera[i]->CommandExecute("TriggerSoftware");
                }

        }
           
        m_timePoint += std::chrono::milliseconds(m_nTriggerTimeInterval);

        
        if (m_bExit) break;
    }
    return 0;
}

// Thread function for triggering with mutex
int HikMultipleCameras::ThreadTriggerGigActionCommandFun() 
{
    
    int nRet = -1;
    while(true) 
    {
        
        
            nRet = HikCamera::GIGEIssueActionCommand(&m_actionCMDInfo, &m_actionCMDResList);
            if (MV_OK != nRet)
            {
                printf("Issue Action Command fail! nRet [0x%x]\n", nRet);
                continue;
            }
            printf("NumResults = %d\r\n",m_actionCMDResList.nNumResults);

            MV_ACTION_CMD_RESULT* pResults = m_actionCMDResList.pResults;
            for (unsigned int i = 0;i < m_actionCMDResList.nNumResults;i++)
            {
                //Print the device infomation
                DEBUG_PRINT("Ip == %s, Status == 0x%x\r\n",pResults->strDeviceAddress,pResults->nStatus);
                pResults++;
            }
       
        if (m_bExit) break;
       
    }
    return nRet;
}



// Stop grabbing
int HikMultipleCameras::StopGrabbing()
{
    if ( m_bOpenDevice == false || m_bStartGrabbing == false)
    {        
        printf("'m_bOpenDevice'set to false Or 'm_bStartGrabbing' set to false! Exiting from StopGrabbing... \n");
        return -1;
    }

    if (m_nTriggerMode == MV_TRIGGER_MODE_ON)
        m_tTriggerThread->join();

    int nRet = -1, nRetOne;
    bool bRet = false;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            
            m_tGrabThreads[i]->join();
            m_tConsumeThreads[i]->join();
           
            nRet = m_pcMyCamera[i]->StopGrabbing();
            if (MV_OK != nRet)
            {
                printf("Stop grabbing fail! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
                bRet = true;
                nRetOne = nRet;
            } else {

                printf("Stop grabbing success! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
            }

          
        }
        
    }
    if (nRet == -1) 
    {
        printf("There is something wrong with the number of opened devices  in StopGrabbing! \n");
        return nRet;
    }
    if (bRet)
    {
        printf("Cannot stop grabbing for at least one camera\n");
        return nRetOne;


    }
    return nRet;
   

}

// Set trigger mode on or off
int HikMultipleCameras::SetTriggerModeOnOff(int triggerMode)
{
    if (m_nDeviceNum == 0) return -1;

    m_nTriggerMode = triggerMode;
    int nRet =  SetTriggerMode();
    if (nRet != MV_OK)
        return nRet;

    if (m_nTriggerMode == MV_TRIGGER_MODE_ON)  
    {
        if (m_sTriggerSource == "Action1") 
        {
            m_timePoint =  std::chrono::system_clock::now();
            return SetTriggerGigEAction();

        } else if (m_sTriggerSource == "Software")
        {
            return SetTriggerSoftwareMode();
        } else{
            printf("Only GigE Action Command Trigger and Softare Trigger supported! Exiting... \n");
            return -1;
        }
    }
    return MV_OK;
}

// Software trigger
int HikMultipleCameras::SetTriggerSoftwareMode()
{
    int nRet = -1;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            nRet = m_pcMyCamera[i]->SetEnumValueByString("TriggerSource", m_sTriggerSource.c_str());
            if (nRet != MV_OK)
            {
                printf("Cannot set software Trigger! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }
        }
    }
    if (nRet == -1) 
    {
        printf("There is something wrong with the number of opened devices in SetTriggerSoftwareMode! Exiting... \n");
    }
    
    return nRet;
}

// GigE Action Command Trigger
int  HikMultipleCameras::SetTriggerGigEAction() 
{
    int nRet = -1;
    for (uint i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {
            nRet = m_pcMyCamera[i]->SetEnumValueByString("TriggerSource", m_sTriggerSource.c_str());
            if (nRet != MV_OK)
            {
                printf("Cannot set  Trigger GigE Action1! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }
            nRet = m_pcMyCamera[i]->SetIntValue("ActionDeviceKey", m_nDeviceKey);
            if (nRet != MV_OK)
            {
                printf("Cannot set Action Device Key! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }            

            nRet = m_pcMyCamera[i]->SetIntValue("ActionGroupMask", m_nGroupMask);
            if (nRet != MV_OK)
            {
                printf("Cannot set Action Group Mask! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }  
            nRet = m_pcMyCamera[i]->SetIntValue("ActionGroupKey", n_nGroupKey);
            if (nRet != MV_OK)
            {
                printf("Cannot set Action Group Key! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }  
            
        }
    }
    if (nRet == -1) 
    {
        printf("There is something wrong with the number of opened devices in SetTriggerGigEAction! Exiting... \n");
    }

    m_actionCMDInfo.nDeviceKey = m_nDeviceKey;
    m_actionCMDInfo.nGroupKey = n_nGroupKey;
    m_actionCMDInfo.nGroupMask = m_nGroupMask;
    m_actionCMDInfo.pBroadcastAddress = "255.255.255.255";
    m_actionCMDInfo.nTimeOut = m_nTriggerTimeInterval;
    m_actionCMDInfo.bActionTimeEnable = 0;


    return nRet;

}


// Save Images of Cameras in threads
int HikMultipleCameras::SaveImages()
{
    
    if (false == m_bStartGrabbing)
    {        
        printf("'m_bStartGrabbing' set to false! Cannot save images. \n");
        return -1;
    }

    
    for (int i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCamera[i])
        {

            m_bStartConsuming = true;
            m_tConsumeThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadConsumeFun, this, i)));
            if (m_tConsumeThreads[i] == nullptr)
            {
                printf("Create consume thread fail! DevIndex[%d]\r\n", i);
                return -1;
            }

        }
    }
    return MV_OK;

}   



