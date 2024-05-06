
// MultipleCameraDlg.cpp : implementation file
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <functional> 
#include <cmath>  
#include <memory>
#include <cstdint>
#include <exception>
#include <numeric>
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/bind/bind.hpp>
#include "HikMultipleCameras.h"
#include "Container.h"
#include "SafeVector.h"



#ifdef DEBUG
#ifdef _MSC_VER 
#define DEBUG_PRINT(...) printf_s(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#endif
#else
#define DEBUG_PRINT(...) do {} while (0)
#endif



template<typename T>
std::vector<std::size_t> tag_sort(const std::vector<T>& v)
{
    std::vector<std::size_t> result(v.size());
    std::iota(std::begin(result), std::end(result), 0);
    std::sort(std::begin(result), std::end(result),
            [&v](const auto & lhs, const auto & rhs)
            {
                return v[lhs] < v[rhs];
            }
    );
    return result;
}

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

// thread_local unsigned count_buf = 0;
// thread_local unsigned counter = 0;

// thread_local double last_buf = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();

// #define FPS_CALC_THREAD_BUF(_WHAT_, buff) \
// do \
// { \
//     double now_buf = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); \
//     ++count_buf; \
//     ++counter; \
//     if (now_buf - last_buf >= 1.0) \
//     { \
//       std::cerr << "Average framerate("<< _WHAT_ << "): " << double(count_buf)/double(now_buf - last_buf) << " Hz. Queue size: " << buff.getSize () << " Frame Number: "<<counter <<"\n"; \
//       count_buf = 0; \
//       last_buf = now_buf; \
//     } \
// }while(false)


#define FPS_CALC_BUF(_WHAT_, buff) \
do \
{ \
    static unsigned count_buf = 0;\
    static unsigned counter = 0; \
    static double last_buf = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();\
    double now_buf = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); \
    ++count_buf; \
    ++counter; \
    if (now_buf - last_buf >= 1.0) \
    { \
      std::cerr << "Average framerate("<< _WHAT_ << "): " << double(count_buf)/double(now_buf - last_buf) << " Hz. Queue size: " << buff.size_approx () << " Frame Number: "<<counter <<"\n"; \
      count_buf = 0; \
      last_buf = now_buf; \
    } \
}while(false)


// HikMultipleCameras dialog
HikMultipleCameras::HikMultipleCameras(moodycamel::ConcurrentQueue<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>> &buf,  std::chrono::system_clock::time_point timePoint, const std::string& cameraSettingsFile):
      m_buf(buf)
    , m_nDeviceNum(0)
    , m_nDeviceNumDouble(0)
    , m_nWriteThreads(3)
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
    , m_sCameraSettingsFile(cameraSettingsFile)
    , m_nTriggerTimeInterval(0)
 

    
    
    
   
{
	
   
    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    memset(&m_stDevListCorr, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    memset(&m_actionCMDInfo, 0, sizeof(MV_ACTION_CMD_INFO));
    memset(&m_actionCMDResList, 0, sizeof(MV_ACTION_CMD_RESULT_LIST));
   
    
    // EnumDevicesAvoidDublication();
   // EnumDevicesByIPAddress();
    EnumDevices();
    // m_pair_images_info_buff_ptok=m_pair_images_info_buff;
    // m_pair_images_info_buff_ctok= m_pair_images_info_buff;

    if (m_nDeviceNum > 0)
    {
        
        converter = std::make_unique<BayerToH264Converter2>(m_nDeviceNum, m_nWriteThreads, 1920, 1200);
        converter->initializeContexts("AllCameras.mp4");
        m_bImagesOk.resize(m_nDeviceNum, false);
        m_bImagesCheck.resize(m_nDeviceNum, false);
        m_bImagesReady.resize(m_nDeviceNum, false);
        m_bDataReady.resize(m_nDeviceNum, false);
        bDataReady.resize(m_nDeviceNum, true);
        m_params.resize(m_nDeviceNum, {0});
        m_stImagesInfo.resize(m_nDeviceNum, {0});
        m_nSaveImagesBufSize.resize(m_nDeviceNum, 0);
        m_Containers.resize(m_nDeviceNum, Container());
        m_its.resize(m_nDeviceNum, 0);
        
        m_vectorAvPacketBuff.resize(m_nDeviceNum, nullptr);
        
        m_currentPairImagesInfo_Buff.resize(m_nDeviceNum, std::make_pair(MV_FRAME_OUT_INFO_EX{0}, nullptr));
        m_currentPairImagesInfo_Buff_Prev.resize(m_nDeviceNum, std::make_pair(MV_FRAME_OUT_INFO_EX{0}, nullptr));

        m_mProduceMutexes = std::vector<std::mutex>(m_nDeviceNum);
        m_mProduceMutexes = std::vector<std::mutex>(m_nDeviceNum);
        m_mCheckMutexes = std::vector<std::mutex>(m_nDeviceNum);
        codecMutexes_ =  std::vector<std::shared_mutex>(m_nDeviceNum);
        m_cDataReadyCon = condVector(m_nDeviceNum);
    

        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        std::chrono::seconds sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
        for (unsigned int i = 0 ; i < m_nDeviceNum ; i++)
        {
            memset(&(m_stImagesInfo[i]), 0, sizeof(MV_FRAME_OUT_INFO_EX));
            m_pDataForSaveImages.push_back(nullptr);
            m_pSaveImagesBuf.push_back(nullptr);
            m_pcMyCameras.push_back(std::make_unique<HikCamera>());
            // m_pairImagesInfo_Buff_New.set(i, std::make_pair(MV_FRAME_OUT_INFO_EX{0}, nullptr));
            printf("%d Camera serial number:%s\n", i, m_mapSerials[i].c_str());
           
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
    for (unsigned int i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            nRet = m_pcMyCameras[i]->SetEnumValue("TriggerMode", m_nTriggerMode);
            
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
    if (m_pcMyCameras[nCurCameraIndex])
    {
        MV_SAVE_IMAGE_PARAM_EX stParam;
        memset(&stParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));
        uint64_t oldtimeStamp = 0;
        long long int oldmicroseconds = 0;
        while(m_bStartConsuming)
        {
            {
             
                std::unique_lock<std::mutex> lk(m_mProduceMutexes[nCurCameraIndex]);
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
                stParam.nJpgQuality = 99;  
                
                m_bImagesOk[nCurCameraIndex] = false;
                
                int nRet =  m_pcMyCameras[nCurCameraIndex]->SaveImage(&stParam);

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
                
                #ifdef _MSC_VER 
                sprintf_s(filepath, sizeof(filepath),"Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[nCurCameraIndex].c_str(), stParam.nWidth, stParam.nHeight, m_stImagesInfo[nCurCameraIndex].nFrameNum);
                FILE* fp ;
                fopen_s(&fp, filepath, "wb");
                #else
                sprintf(filepath,"Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[nCurCameraIndex].c_str(), stParam.nWidth, stParam.nHeight, m_stImagesInfo[nCurCameraIndex].nFrameNum);
                FILE* fp = fopen( filepath, "wb");
                #endif               

                
                if (fp == NULL)
                {
                    printf("fopen failed\n");
                    break;
                }
                fwrite(m_pDataForSaveImages[nCurCameraIndex].get(), 1, stParam.nImageLen, fp);
                fclose(fp);

                // #ifdef _MSC_VER 
                // DEBUG_PRINT("%d. Camera, Save image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%lld ms], SystemTimeDiff[%.3f ms]\n", nCurCameraIndex,m_stImagesInfo[nCurCameraIndex].nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000);
                // #else
                // DEBUG_PRINT("%d. Camera, Save image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%ld ms], SystemTimeDiff[%.3f ms]\n", nCurCameraIndex,m_stImagesInfo[nCurCameraIndex].nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000);
                // #endif
               // printf_s("%d. Camera, Save image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%lld ms], SystemTimeDiff[%.3f ms]\n", nCurCameraIndex,m_stImagesInfo[nCurCameraIndex].nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000);
                
            }
            

            if (m_bExit) m_bStartConsuming = false;
            //FPS_CALC ("Image Saving FPS:", nCurCameraIndex);

        }
    }
    return 0;
}

//Thread function with GetImageBuffer API
int HikMultipleCameras::ThreadGrabWithGetImageBufferFun(int nCurCameraIndex)
{
    srand((unsigned) time(NULL)); 
   
    int nRet = -1;
    if (m_pcMyCameras[nCurCameraIndex])
    {
        MV_FRAME_OUT stImageOut = {0};
        memset(&stImageOut, 0, sizeof(MV_FRAME_OUT));
        uint64_t oldtimeStamp = 0;
        long long int oldmicroseconds = 0;
        while(m_bStartGrabbing)
        {
            
                while (!ready_for_start.load(std::memory_order_acquire)) {;}

                std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();      
                // printf_s("%d Camera, start time :%lld\n", nCurCameraIndex, std::chrono::duration_cast<std::chrono::milliseconds>( begin.time_since_epoch()).count()) ;
                nRet = m_pcMyCameras[nCurCameraIndex]->GetImageBuffer(&stImageOut, 1000);
           


                MV_FRAME_OUT_INFO_EX tmpFrame={0};
                memset(&tmpFrame, 0, sizeof(MV_FRAME_OUT_INFO_EX));
                //printf("Grabbing 0 %d. Camera, %d ....\n", nCurCameraIndex, tmpFrame.stFrameInfo.nFrameNum);

                std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
                #ifdef _MSC_VER 
                // DEBUG_PRINT("Grabbing duration in DevIndex[%d]= %lld[ms]\n", nCurCameraIndex, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );
                #else
                DEBUG_PRINT("Grabbing duration in DevIndex[%d]= %ld[ms]\n", nCurCameraIndex, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );
                #endif  
            //    printf("Grabbing duration in DevIndex[%d]= %lld[ms]\n", nCurCameraIndex, ready_for_start.store(false, std::memory_order_release);;
                
                if (nRet == MV_OK)
                {
                    
                    {
                        // std::unique_lock<std::mutex> lk(m_mProduceMutexes[nCurCameraIndex]);
                        // m_cDataReadySingleCon2.wait(lk, [this, nCurCameraIndex]{

                        //     return bDataReady[nCurCameraIndex];

                        // });
                        // bDataReady[nCurCameraIndex]=false;
                        
                        // if (m_cnt[nCurCameraIndex] > 1 )  
                        //     m_cnt.set(nCurCameraIndex, 1) ;
                        // else  m_cnt.set(nCurCameraIndex, m_cnt[nCurCameraIndex]+1) ; 
                        //printf("%d. Camera, %d \n", nCurCameraIndex, m_cnt[nCurCameraIndex]);
                        
                        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                        std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
                        long long int microseconds = ms.count();

                        uint64_t timeStamp = (((uint64_t) stImageOut.stFrameInfo.nDevTimeStampHigh) << 32) + stImageOut.stFrameInfo.nDevTimeStampLow;
                        uint64_t  timeDif = timeStamp - oldtimeStamp;
                        uint64_t hostTimeStamp = stImageOut.stFrameInfo.nHostTimeStamp;
                        uint64_t systemTimeDiff = microseconds - oldmicroseconds;
                        oldtimeStamp = timeStamp; 
                        oldmicroseconds = microseconds;
                        #ifdef _MSC_VER 
                        DEBUG_PRINT("DevIndex[%d], Grab image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%lld ms], SystemTimeDiff[%.3f ms], HostTimeStamp[%lld ms]\n", nCurCameraIndex, stImageOut.stFrameInfo.nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000, hostTimeStamp);
                        #else
                        DEBUG_PRINT("DevIndex[%d], Grab image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%ld ms], SystemTimeDiff[%.3f ms], HostTimeStamp[%ld ms]\n", nCurCameraIndex, stImageOut.stFrameInfo.nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000, hostTimeStamp);
                        #endif
                        // printf_s("DevIndex[%d], nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%lld ms], SystemTimeDiff[%.3f ms], HostTimeStamp[%lld ms]\n", nCurCameraIndex, stImageOut.stFrameInfo.nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000, hostTimeStamp);
                        // printf_s("DevIndex[%d], nFrameNum[%d], DeviceTimeStamp[%.3f ms], SystemTimeStamp[%lld ms], HostTimeStamp[%lld ms]\n", nCurCameraIndex,  stImageOut.stFrameInfo.nFrameNum, double(timeStamp)/1000000,   uint64_t(round(double(microseconds)/1000)),  hostTimeStamp);



                        if (stImageOut.pBufAddr != NULL)
                        {   
        
                            std::shared_ptr<uint8_t[]>  tmpSharedptr (new uint8_t[stImageOut.stFrameInfo.nFrameLen]);
                            memcpy(tmpSharedptr.get(), stImageOut.pBufAddr, stImageOut.stFrameInfo.nFrameLen);
                          

                            memcpy(&tmpFrame, &(stImageOut.stFrameInfo), sizeof(MV_FRAME_OUT_INFO_EX)); 
                            strcpy_s(tmpFrame.nSerialNum, m_mapSerials[nCurCameraIndex].c_str());

                            m_pair_images_info_buff.enqueue(std::make_pair(tmpFrame, tmpSharedptr));


                            if ( done_producers.fetch_add(1, std::memory_order_acq_rel) + 1 == m_nDeviceNum)
                            {
                                // binary_sem_prod.release();
                                light_sem_prod.signal();
                                // std::cout<<"Cam. Index:"<<nCurCameraIndex<<std::endl;
                            }
                        
                            // m_bImagesCheck[nCurCameraIndex] = true;

                           
                            nRet = m_pcMyCameras[nCurCameraIndex]->FreeImageBuffer(&stImageOut);
                            if (MV_OK != nRet)
                            {
                                printf("cannot free buffer! \n");
                                return nRet;
                            }
                        
                        }
                    
                    }
                    
                   
              
                   
                   

                    
                }   else {

                        printf("Get Image Buffer fail! DevIndex[%d], nRet[%#x], \n", nCurCameraIndex, nRet);
                       
                         m_bExit = true;
                
                    
                 }

               

            if (m_bExit) m_bStartGrabbing = false;
           // FPS_CALC ("Image Grabbing FPS:", nCurCameraIndex);
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
    if (m_pcMyCameras[nCurCameraIndex])
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
            nRet = m_pcMyCameras[nCurCameraIndex]->GetOneFrame(m_pSaveImagesBuf[nCurCameraIndex].get(), m_params[nCurCameraIndex].nCurValue, &stImageOut, 1000);
            std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
            #ifdef _MSC_VER 
            DEBUG_PRINT("Grabbing duration in DevIndex[%d]= %lld[ms]\n", nCurCameraIndex, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );
            #else
            DEBUG_PRINT("Grabbing duration in DevIndex[%d]= %ld[ms]\n", nCurCameraIndex, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );
            #endif

            if (nRet == MV_OK)
            {
                
                
                {
                    std::lock_guard<std::mutex> lk(m_mProduceMutexes[nCurCameraIndex]);
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
            //FPS_CALC ("Image Grabbing FPS:", nCurCameraIndex);
        }
    }
    if (nRet == -1) 
    {
        printf("There is something wrong with the device opened in ThreadGrabWithGetImageBufferFun! DevIndex[%d] \n", nCurCameraIndex);
    }
    return nRet;
}

void HikMultipleCameras::EnumDevicesByIPAddress()
{
   
   
   
    
   
    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

   
   
    unsigned int nIp1 = 192, nIp2 = 168, nIp3 = 88, nIp4 = 50, nIp;
    unsigned int n_ExIp1 = 192, n_ExIp2 = 168, n_ExIp3 = 88, n_ExIp4 = 20, n_ExIp;
    int k = 0;
    for (int i = 0 ; i < 2 ; i++)
    {
        
        MV_GIGE_DEVICE_INFO stGigEDev = {0};
        n_ExIp =  (n_ExIp1 << 24) | (n_ExIp2 << 16) | (n_ExIp3 << 8) | n_ExIp4;
        stGigEDev.nNetExport = n_ExIp;
        n_ExIp4++;

        for (int j = 0; j < 12; j++) {
            MV_CC_DEVICE_INFO stDevInfo = {0};
            nIp = (nIp1 << 24) | (nIp2 << 16) | (nIp3 << 8) | nIp4;
            stGigEDev.nCurrentIp = nIp;
           // stGigEDev.chSerialNumber = 
            stDevInfo.nTLayerType = MV_GIGE_DEVICE;
            stDevInfo.SpecialInfo.stGigEInfo = stGigEDev;
            m_stDevList.pDeviceInfo[k] = new MV_CC_DEVICE_INFO (stDevInfo);
            nIp4++; k++;



        }
    }
    
    
    



    m_nDeviceNum = k;

   
    for (unsigned int i = 0; i < m_nDeviceNum; i++)
    {
        
        MV_CC_DEVICE_INFO* pDeviceInfo = m_stDevList.pDeviceInfo[i];

        m_pcMyCameras.push_back(std::make_unique<HikCamera>());
        int nRet = m_pcMyCameras[i]->CreateHandle(pDeviceInfo);
        if (MV_OK != nRet)
        {
            printf("Create Handle fail! DevIndex[%d], nRet[0x%x]\n",i, nRet);
            break;
        }



       
    }

    
}


void HikMultipleCameras::EnumDevicesAvoidDublication()
{
   // memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    int nRet = HikCamera::EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &m_stDevList);
    if ( nRet != MV_OK || m_stDevList.nDeviceNum == 0)
    {
        printf("Find no device!\r\n");
        return;
    }
   // printf("Find %d devices!\r\n", m_stDevList.nDeviceNum);
    
    m_nDeviceNumDouble = m_stDevList.nDeviceNum;
    std::vector<std::tuple<unsigned int, unsigned int, std::string>> vectorExportSerials1;
    
    unsigned int  l = 0;

    for (unsigned int i = 0; i < m_nDeviceNumDouble; i++) {
        MV_CC_DEVICE_INFO* pDeviceInfo = m_stDevList.pDeviceInfo[i];

       if (i < (unsigned int)std::ceil(m_nDeviceNumDouble/4.0f)) {

            m_stDevListCorr.pDeviceInfo[l] = new MV_CC_DEVICE_INFO(*pDeviceInfo);
            vectorExportSerials1.push_back(std::make_tuple (i, pDeviceInfo->SpecialInfo.stGigEInfo.nNetExport , (char *)pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber ));
            m_mapSerials.insert(std::make_pair(l , (char *)pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber));

            l++;
       } else {
            bool skipped =false;
            for (auto it = vectorExportSerials1.begin(); it != vectorExportSerials1.end(); ++it) 
            {
                

                if (std::get<2>(*it) == std::string((char *)pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber) || std::get<1>(*it)== pDeviceInfo->SpecialInfo.stGigEInfo.nNetExport )
                {
                
                    skipped = true;
                    break;
                }
            }
            if (!skipped) 
            {
                m_stDevListCorr.pDeviceInfo[l] = new MV_CC_DEVICE_INFO(*pDeviceInfo);
                m_mapSerials.insert(std::make_pair(l , (char *)pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber));
                l++;
            }



       }
        
       
    }
    
 
    m_nDeviceNum =l;
    printf("Device Number: %d\n", l);
    

}

void HikMultipleCameras::EnumDevices()
{
    memset(&m_stDevListCorr, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    int nRet = HikCamera::EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &m_stDevListCorr);
    if ( nRet != MV_OK || m_stDevListCorr.nDeviceNum == 0)
    {
        printf("Find no device!\r\n");
        return;
    }
    printf("Find %d devices!\r\n", m_stDevListCorr.nDeviceNum);
    
    m_nDeviceNum = m_stDevListCorr.nDeviceNum;
   
    for (unsigned int i = 0; i < m_nDeviceNum; i++)
    {
        
        MV_CC_DEVICE_INFO* pDeviceInfo = m_stDevListCorr.pDeviceInfo[i];
        if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE)
        {
            int nIp1 = ((pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
            int nIp2 = ((pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
            int nIp3 = ((pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
            int nIp4 = (pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
            // int nIp1 = ((pDeviceInfo->SpecialInfo.stGigEInfo.nNetExport & 0xff000000) >> 24);
            // int nIp2 = ((pDeviceInfo->SpecialInfo.stGigEInfo.nNetExport & 0x00ff0000) >> 16);
            // int nIp3 = ((pDeviceInfo->SpecialInfo.stGigEInfo.nNetExport & 0x0000ff00) >> 8);
            // int nIp4 = (pDeviceInfo->SpecialInfo.stGigEInfo.nNetExport & 0x000000ff);
            // Print the IP address and user defined name of the current camera
            DEBUG_PRINT("Device Model Name: %s\n", pDeviceInfo->SpecialInfo.stGigEInfo.chModelName);
            printf("%d Cam. CurrentIp: %d.%d.%d.%d\n" ,i, nIp1, nIp2, nIp3, nIp4);
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
    
    
   
    
    for (unsigned int i = 0; i < m_nDeviceNum; i++)
    {
       // m_pcMyCamera.push_back(std::make_unique<HikCamera>());
        //std::cout<<"step 0"<<std::endl;
       // printf ("nExport %d Camera\n", m_stDevList.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp);

             int nIp1 = ((m_stDevListCorr.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
            int nIp2 = ((m_stDevListCorr.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
            int nIp3 = ((m_stDevListCorr.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
            int nIp4 = (m_stDevListCorr.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
            printf("%d Cam, CurrentIp: %d.%d.%d.%d\n" , i, nIp1, nIp2, nIp3, nIp4);

            int nRet = m_pcMyCameras[i]->Open(m_stDevListCorr.pDeviceInfo[i]);
            printf("%d. Camera, serial number: %s\n", i, (char*)m_stDevListCorr.pDeviceInfo[i]->SpecialInfo.stGigEInfo.chSerialNumber);

            if (nRet != MV_OK)
            {
                m_pcMyCameras[i].reset();
                printf("Open device failed! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
                continue;
            }
            else
            {
                
                            
                // Detect the optimal packet size (it is valid for GigE cameras only)
                m_bOpenDevice = true;
                if (m_stDevListCorr.pDeviceInfo[i]->nTLayerType == MV_GIGE_DEVICE)
                {
                    unsigned int  nPacketSize = 0;
                    nRet = m_pcMyCameras[i]->GetOptimalPacketSize(&nPacketSize);
                    if (nPacketSize > 0)
                    {
                        nRet = m_pcMyCameras[i]->SetIntValue("GevSCPSPacketSize",nPacketSize);
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


void HikMultipleCameras::OpenDevicesInThreads()
{

    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
            m_tOpenDevicesThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadOpenDevicesFun, this, i)));
       
    }

}

int HikMultipleCameras::ThreadOpenDevicesFun(int nCurCameraIndex) 
{
    if (true == m_bOpenDevice || m_nDeviceNum == 0)
    {
        printf("'m_bOpenDevice'set to true Or 'm_nDeviceNum' set to 0! Exiting from OpenDevices... \n");
        return -1;
    }
   
   
    
    int nRet = m_pcMyCameras[nCurCameraIndex]->Open(m_stDevListCorr.pDeviceInfo[nCurCameraIndex]);
    if (nRet != MV_OK)
    {
        m_pcMyCameras[nCurCameraIndex].reset();
        printf("Open device failed! DevIndex[%d], nRet[%#x]\r\n", nCurCameraIndex , nRet);
        exit(-1);
    }
    else
    {
        
                    
        // Detect the optimal packet size (it is valid for GigE cameras only)
        m_bOpenDevice = true;
        if (m_stDevListCorr.pDeviceInfo[nCurCameraIndex]->nTLayerType == MV_GIGE_DEVICE)
        {
            unsigned int nPacketSize = 0;
            nRet = m_pcMyCameras[nCurCameraIndex]->GetOptimalPacketSize(&nPacketSize);
            if (nPacketSize > 0)
            {
                nRet = m_pcMyCameras[nCurCameraIndex]->SetIntValue("GevSCPSPacketSize",nPacketSize);
                if(nRet != MV_OK)
                {
                    printf("Set Packet Size fail! DevIndex[%d], nRet[%#x]\r\n", nCurCameraIndex, nRet);
                }
            }
            else
            {
                printf("Get Packet Size fail! DevIndex[%d], nRet[%#x]\r\n", nCurCameraIndex, nRet);
            }
        }

        
    }
    

    return MV_OK;
        



}

void HikMultipleCameras::JoinOpenDevicesInThreads() {

    for (unsigned int i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            m_tOpenDevicesThreads[i]->join();
        }
    }


}


void HikMultipleCameras::CloseDevicesInThreads()
{

    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            m_tCloseDevicesThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadCloseDevicesFun, this, i)));
        }
    }

}

int HikMultipleCameras::ThreadCloseDevicesFun(int nCurCameraIndex)
{

    int nRet = -1;
   
    if (m_pcMyCameras[nCurCameraIndex])
    {
        nRet = m_pcMyCameras[nCurCameraIndex]->Close();
        if (MV_OK != nRet)
        {
            printf("Close device fail! DevIndex[%d], nRet[%#x]\r\n", nCurCameraIndex, nRet);
        }
        else {

            printf("Close device success! DevIndex[%d], nRet[%#x]\r\n", nCurCameraIndex, nRet);
        }

        
        
    }

    return nRet; 

}
void HikMultipleCameras::JoinCloseDevicesInThreads() {

    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            m_tCloseDevicesThreads[i]->join();
        }
    }


}


int HikMultipleCameras::ConfigureCameraSettings()
{
    boost::property_tree::ptree pt;

    // Read the JSON file
    std::ifstream file(m_sCameraSettingsFile);
    if (!file.good()) 
    {
        printf("Error in opening 'CameraSettings.json' file! Exiting... \n");
        return -1;
    }

    boost::property_tree::read_json(file, pt);
    m_sTriggerSource = pt.get<std::string>("TriggerSource");
    m_nTriggerTimeInterval = pt.get<int>("TriggerTimeInterval");
    m_pBroadcastAddress = pt.get<std::string>("BroadcastAddress");
    int height = pt.get<int>("Height");
    int width = pt.get<int>("Width");
    int exposureAuto = pt.get<int>("ExposureAuto");
    float exposureTime = pt.get<float>("ExposureTime");
    bool acquisitionFrameRateEnable = pt.get<bool>("AcquisitionFrameRateEnable");
    float acquisitionFrameRate = pt.get<float>("AcquisitionFrameRate");
    bool gevPAUSEFrameReception = pt.get<bool>("GevPAUSEFrameReception");
    bool gevIEEE1588 = pt.get<bool>("GevIEEE1588");
    float gain = pt.get<float>("Gain");
    std::string pixelFormat = pt.get<std::string>("PixelFormat");

    
    file.close();
    
 
    
    int nRet = -1;
    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i]) 
        {
            nRet = m_pcMyCameras[i]->SetEnumValueByString("PixelFormat", pixelFormat.c_str());
            if (nRet != MV_OK)
            {
                printf("Cannot set Pixel Format! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet ;
            }
            // nRet = m_pcMyCameras[i]->SetEnumValue("PixelFormat", PixelType_Gvsp_BayerRG8);
            // if (nRet != MV_OK)
            // {
            //     printf("Cannot set Exposure Auto value! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
            //     return nRet ;
            // }
            
            nRet = m_pcMyCameras[i]->SetIntValue("Height", height);
            if (nRet != MV_OK){
                printf("Cannot set Height fail! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet ;
            }
            nRet = m_pcMyCameras[i]->SetIntValue("Width", width);
            if (nRet != MV_OK){
                printf(" Cannot set Width! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet ;
            }

            nRet = m_pcMyCameras[i]->SetEnumValue("ExposureAuto", exposureAuto);
            if (nRet != MV_OK)
            {
                printf("Cannot set Exposure Auto value! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet ;
            }

            nRet = m_pcMyCameras[i]->SetFloatValue("ExposureTime", exposureTime);
            if (nRet != MV_OK)
            {
                printf("Cannot set Exposure Time value! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet;
            }
            nRet = m_pcMyCameras[i]->SetBoolValue("AcquisitionFrameRateEnable", acquisitionFrameRateEnable);
            if (nRet != MV_OK)
            {
                printf("Cannot set Acquisition FrameRate Enable!. DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet ;
            }

            nRet = m_pcMyCameras[i]->SetFloatValue("AcquisitionFrameRate", acquisitionFrameRate); 
            if (nRet != MV_OK)
            {
                printf("Cannot set Acquisition Frame Rate value! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet;
            }
        
            nRet = m_pcMyCameras[i]->SetBoolValue("GevPAUSEFrameReception", gevPAUSEFrameReception);
            if (nRet != MV_OK)
            {
                printf("Cannot set GevPAUSEFrameReception Acquisition Enable! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet;
            }
        
            nRet = m_pcMyCameras[i]->SetBoolValue("GevIEEE1588", gevIEEE1588);
            if (nRet != MV_OK)
            {
                printf("Cannot set  GevIEEE1588 Enable! DevIndex[%d], nRet[%#x]. Exiting...\r\n ", i, nRet);
                return nRet;
            }
            nRet = m_pcMyCameras[i]->SetFloatValue("Gain", gain);

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

    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
            if (m_pcMyCameras[i])
            {
               m_tResetTimestampThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadTimeStampControlResetFun, this, i)));
            }
            

    }
}

// Thread function for resetting timestamp control
int HikMultipleCameras::ThreadTimeStampControlResetFun(int nCurCameraIndex) {

    if (m_pcMyCameras[nCurCameraIndex])
    {
        int nRet;
        
        // if ( m_mapModels[nCurCameraIndex] == std::string("MV-CA023-10GC")) 
        // {
        //    // std::this_thread::sleep_for(std::chrono::milliseconds(14));
        // }
        nRet = m_pcMyCameras[nCurCameraIndex]->CommandExecute("GevTimestampControlReset") ;
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        std::chrono::microseconds ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
        long long int microseconds = ms.count();
        //printf("Time Stamp of  %d. Camera: %lld\n", nCurCameraIndex, microseconds);

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

    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            m_tResetTimestampThreads[i]->join();
        }
    }
}

void HikMultipleCameras::TimeStampControlReset() 
{
    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            int nRet;
            nRet = m_pcMyCameras[i]->CommandExecute("GevTimestampControlReset") ;
            if (nRet != MV_OK) printf("%d. Camera, TimeStampControlReset failed! \n", i);
        }
    }
}



// Close, include destroy handle
void HikMultipleCameras::CloseDevices()
{

	int nRet = -1;
    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            nRet = m_pcMyCameras[i]->Close();
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


int HikMultipleCameras::Save2BufferThenDisk()
{
    
    if (false == m_bStartGrabbing)
    {        
        printf("'m_bStartGrabbing' set to false! Cannot save images. \n");
        return -1;
    }
    //std::this_thread::sleep_for(std::chrono::milliseconds(5));



   
    m_tCheckBuffThread = std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadCheckBufferFun, this));


   

    for (unsigned int i = 0; i < m_nWriteThreads ; i++)
        m_tSaveAsMP4Threads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::Write2H264FromBayerAtomic, this, i)));

    // m_tWrite2MP4Thread = std::make_unique<std::thread>(std::bind(&HikMultipleCameras::Write2MP4, this));

    return MV_OK;

}   


int HikMultipleCameras::ThreadCheckBufferFun() 
{
    std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> tmp_item;

    while(true)
    {
        if (m_bExit) {
            // m_buf.pop_back();
            break;
        }




        tmp_item.resize(m_nDeviceNum, std::make_pair(MV_FRAME_OUT_INFO_EX{0}, nullptr));
        // if (done_producers.load(std::memory_order_acquire)  == m_nDeviceNum)
        //     binarySem.release();

        // while (flag.test_and_set(std::memory_order_acquire)){;}
        
        // while (done_producers.load(std::memory_order_acquire)  != m_nDeviceNum){        
        //     std::this_thread::yield();
        // ;}
        // binary_sem_prod.acquire();
        light_sem_prod.wait();
		ready_for_start.store(false, std::memory_order_release);
		
		
        // printf("Before size:%zd\n", m_pairImagesInfo_Buff.size_approx());
        m_pair_images_info_buff.try_dequeue_bulk(tmp_item.data(), m_nDeviceNum);
        done_producers.store(0, std::memory_order_release);
        ready_for_start.store(true, std::memory_order_release);
        
		// printf("size:%zd\n", m_pairImagesInfo_Buff.size_approx()); 
        if (!m_buf.enqueue(tmp_item) )
        {
            
            printf ("Warning! Buffer was full, overwriting data!\n");
            
        } else{
            // binary_sem_buf.release();
            // light_sem_buf.signal();
        }

            

        

        FPS_CALC_BUF ("Copying to Buffer callback", m_buf);

       
        
    
    }
    return 0;

}

// int HikMultipleCameras::ThreadCheckBufferFun() 
// {
//     while(true)
//     {

        
//             std::unique_lock<std::mutex> lk(m_mGrabMutex);
//             m_cDataReadySingleCon1.wait(lk, [this] {
//                 bool sum =true;
//                 int i = 0;
//                 for (; i < m_bImagesCheck.size(); i++){
                    
//                     //std::cout<<"m_bImagesCheck[i]: " <<i << " "<< m_bImagesCheck[i]<<" ";
//                     sum = sum && m_bImagesCheck[i];

//                 }
//                 //printf("\n");
//                 if (i == 0 ) return false;
//                 else return sum;

//             });
            
            
//             for (int i =0 ; i < m_bImagesCheck.size(); i++)
//                 m_bImagesCheck[i]= false;
            
            

//         // printf("End before\n");
       
//     //    std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t []>>> tmpVector ;
//     //    // if (m_cnt.size() > 1) printf("Begin cnt:\n");
//     //     std::vector<int> indexes(m_nDeviceNum, -1);

//     //     for (unsigned int i = 0 ; i <  m_nDeviceNum ; i++)
//     //     {
//     //        //if ( m_pairImagesInfo_Buff_New[i].second != nullptr)
//     //          //printf("%d camera, m_pairImagesInfo_Buff_New[i].second: %x \n",i,  m_pairImagesInfo_Buff_New[i].second.get());
//     //        if ( m_pairImagesInfo_Buff_New[i].second ) {
//     //             // std::shared_ptr<uint8_t[]>  tmpSharedptr (new uint8_t[m_pairImagesInfo_Buff_New[i].first.nFrameLen]);

//     //             // memcpy(tmpSharedptr.get(), m_pairImagesInfo_Buff_New[i].second.get(), m_pairImagesInfo_Buff_New[i].first.nFrameLen);
                
//     //             //m_pairImagesInfo_Buff_New.set(i, std::make_pair(MV_FRAME_OUT_INFO_EX(m_pairImagesInfo_Buff_New[i].first), tmpSharedptr));
//     //             tmpVector.push_back(m_pairImagesInfo_Buff_New[i]); 
//     //             // tmpVector.push_back(std::make_pair(MV_FRAME_OUT_INFO_EX(m_pairImagesInfo_Buff_New[i].first), tmpSharedptr));
//     //             //
//     //            // m_pairImagesInfo_Buff_New.erase(i);
//     //             m_pairImagesInfo_Buff_New.set(i, std::make_pair(MV_FRAME_OUT_INFO_EX{0}, nullptr));
//     //             //m_pairImagesInfo_Buff_New[i].second.reset(m_pairImagesInfo_Buff_New[i].first.nFrameLen);
//     //             //m_pairImagesInfo_Buff_New[i].second = nullptr;
//     //            // printf("Inside %d camera, m_pairImagesInfo_Buff_New[i].second: %x \n",i,  m_pairImagesInfo_Buff_New[i].second.get());
//     //             indexes[i] = i ;
//     //        }
//     //       // printf("indexes[%d]:%d\n",i, indexes[i]);
//     //     }

//         // for (unsigned int i = 0 ; i <  m_nDeviceNum ; i++)
//         // {
//         //     printf("Inside %d camera, m_pairImagesInfo_Buff_New[i].second: %x \n",i,  m_pairImagesInfo_Buff_New[i].second.get());

//         // }

//         // {
//         //    // std::lock_guard<std::mutex> lk(m_mIoMutex);
//         //     for (unsigned int i = 0 ; i <  m_nDeviceNum ; i++)
//         //     {
//         //         if (indexes[i] == -1)
//         //         {
//         //             //printf("%d. Camera, m_cnt[%d]\n", i, m_cnt[i]);
//         //             if (m_cnt[i] == 2) {
//         //                 tmpVector.push_back(m_pairImagesInfo_Buff_Prev[i]);
                        
                            
//         //                 // printf("%d. Camera counter set to 2, frameNum: %d\n", i, m_pairImagesInfo_Buff[i].first.nFrameNum);
//         //                    // printf("Prev %d. Camera counter set to 2, frameNum: %d\n", i, m_pairImagesInfo_Buff_Prev[i].first.nFrameNum);
//         //                     std::shared_ptr<uint8_t[]>  tmpSharedptr (new uint8_t[m_pairImagesInfo_Buff[i].first.nFrameLen]);
//         //                     memcpy(tmpSharedptr.get(), m_pairImagesInfo_Buff[i].second.get(), m_pairImagesInfo_Buff[i].first.nFrameLen);
//         //                     m_pairImagesInfo_Buff_New.set(i, std::make_pair(MV_FRAME_OUT_INFO_EX(m_pairImagesInfo_Buff[i].first), tmpSharedptr));
                        
//         //             }
//         //             else 
//         //                 tmpVector.push_back(m_pairImagesInfo_Buff[i]);
//         //         }
//         //        // m_cnt.set(i, 0);

//         //     }
//         // }
//         // for (unsigned int i = 0 ; i < m_nDeviceNum; i++) 
//         //     m_cnt.set(i, 0);

      
      
//         // for (int i = 0 ; i <  m_pairImagesInfo_Buff.size() ; i++)
//         // {
//         //    tmpVector.push_back(m_pairImagesInfo_Buff[i]);

//         // }
//         // for (int i = 0; i < m_bDataReady.size(); i++) {
//         //     m_bDataReady[i] = true;
//         // }
//         // m_cdataCheckCon.notify_all();

//         // bool break_flag = false;
//         // while (true) {
//         //     for (int i = 0 ; i <  m_pairImagesInfo_Buff.size() ; i++)
//         //     {
//         //         if (tmpVector.size() == m_nDeviceNum)
//         //         {
//         //              break_flag = true;
//         //              break;
                     
//         //         }
//         //         if (m_bImagesCheck[i]) tmpVector.push_back(m_pairImagesInfo_Buff[i]);
                
//         //     }
//         //     if (break_flag) break;

//         // }
 
           

//        // printf("End before\n");

//     //    std::fill(bDataReady.begin(), bDataReady.end(),true);
//     //    m_cDataReadySingleCon2.notify_all();

//     //    printf("Begin: \n");

//     //     for (int i =0; i <tmpVector.size(); i++) {
//     //          uint64_t timeStamp = (((uint64_t) tmpVector[i].first.nDevTimeStampHigh) << 32) + tmpVector[i].first.nDevTimeStampLow;
//     //         printf("tmpVector %d. %d, ts[%lld ms]\n", i, tmpVector[i].first.nFrameNum,  tmpVector[i].first.nHostTimeStamp);

//     //     }
        
        
//         // printf("End. \n");
//         if (!m_buf.pushBack(m_pairImagesInfo_Buff.data))
//         {
            
//             printf ("Warning! Buffer was full, overwriting data!\n");
            
//         }
       
//         //barrier.wait();
        

//         FPS_CALC_BUF ("Copying to Buffer callback.", m_buf);

//         if (m_bExit)
//             break;
        
    
//     }
//     return 0;

// }

// int HikMultipleCameras::ThreadSave2BufferFun(int nCurCameraIndex)
// {
//     if (m_pcMyCameras[nCurCameraIndex])
//     {   

//         while(true)
//         {
            
//             {
//                 //std::lock(m_mGrabMutex, m_mProduceMutexes[nCurCameraIndex] );
              

                
//                 std::unique_lock<std::mutex> lk(m_mProduceMutexes[nCurCameraIndex]);
                

//                 m_cDataReadyCon[nCurCameraIndex].wait(lk, [this, nCurCameraIndex] {
//                     return m_bImagesOk[nCurCameraIndex];

//                 });
               
//                 if  (m_pSaveImagesBuf[nCurCameraIndex] == nullptr) {
//                     std::this_thread::sleep_for(std::chrono::milliseconds(5));
//                     printf("continue \n");
//                     continue;
//                 }
//                 std::shared_ptr<uint8_t[]> clonedSharedPtr (new uint8_t[m_stImagesInfo[nCurCameraIndex].nFrameLen]);
//                 memcpy(clonedSharedPtr.get(), m_pSaveImagesBuf[nCurCameraIndex].get(),  m_stImagesInfo[nCurCameraIndex].nFrameLen * sizeof(uint8_t));
//                 //MV_FRAME_OUT_INFO_EX tmpFrame = m_stImagesInfo[nCurCameraIndex];
//                 //printf("%d. Camera, tmpFrame.nFrameNum: %d\n",nCurCameraIndex, m_stImagesInfo[nCurCameraIndex].nFrameNum);
//                 //m_pairImagesInfo_Buff[nCurCameraIndex] = std::make_pair(m_stImagesInfo[nCurCameraIndex], clonedSharedPtr);
//                 uint64_t timeStamp = (((uint64_t) m_stImagesInfo[nCurCameraIndex].nDevTimeStampHigh) << 32) + m_stImagesInfo[nCurCameraIndex].nDevTimeStampLow;
               
//                // printf("Before tmpVector %d. %d, ts[%lldms]\n", nCurCameraIndex, m_stImagesInfo[nCurCameraIndex].nFrameNum,  m_stImagesInfo[nCurCameraIndex].nHostTimeStamp );
                 
                  
                
//                 m_pairImagesInfo_Buff.set(nCurCameraIndex, std::make_pair(m_stImagesInfo[nCurCameraIndex], clonedSharedPtr));
//                 m_bImagesOk[nCurCameraIndex] = false;
//                 m_bImagesCheck[nCurCameraIndex] = true;
                 
                
//                // printf("%d. Camera, tmpFrame.nFrameNum: %d\n",nCurCameraIndex,m_pairImagesInfo_Buff[nCurCameraIndex].first.nFrameNum);
                
//             }

//            // barrier2.wait();
            
//             // if (nCurCameraIndex == 0) 
//             // {
//             //     printf("End barrier\n");
//             //     // printf("Begin: \n");

//             //     // for (int i =0; i <m_pairImagesInfo_Buff.size(); i++) {
//             //     //     uint64_t timeStamp = (((uint64_t) m_pairImagesInfo_Buff[i].first.nDevTimeStampHigh) << 32) + m_pairImagesInfo_Buff[i].first.nDevTimeStampLow;
//             //     //     printf("tmpVector %d. %d, ts[%lld ms]\n", i, m_pairImagesInfo_Buff[i].first.nFrameNum,  m_pairImagesInfo_Buff[i].first.nHostTimeStamp);

//             //     // }
        
//             //     // printf("End. \n");
//             //     if (!m_buf.pushBack(m_pairImagesInfo_Buff.data))
//             //     {
                    
//             //         printf ("Warning! Buffer was full, overwriting data!\n");
                    
//             //     }
                
//             //     FPS_CALC_BUF ("image callback.", m_buf);
//             // }


//             m_cDataReadySingleCon1.notify_one();
            
//             // {
//             //     std::unique_lock<std::mutex>  lk(m_mCheckMutexes[nCurCameraIndex]);
//             //     m_cdataCheckCon.wait(lk, [this, nCurCameraIndex ] {
//             //         return m_bDataReady[nCurCameraIndex];


//             //     });

//             //      m_bDataReady[nCurCameraIndex] = false;



//             // }
             
            
              
        

            

            

//             if (m_bExit) break;
//            // FPS_CALC ("Image Saving To Buffer FPS:", nCurCameraIndex);

//         }

//     }  
//     return 0;
// }

// int HikMultipleCameras::ThreadSave2DiskFun(){

//     // while (true)
//     // {
//     //     if (m_bExit) break;
//     //     {
//     //         std::unique_lock<std::mutex> lk(m_mSaveMutex);
//     //         m_cDataReadySingleCon2.wait(lk, [this] {
//     //             bool sum = true;
//     //             unsigned int i = 0;
//     //             for (; i < m_bImagesReady.size(); i++){
                    
//     //                 sum = sum && m_bImagesReady[i];

//     //             }
//     //             if (i == 0 ) return false;
//     //             else return sum;

//     //         });
//     //         for (unsigned int i =0 ; i < m_bImagesReady.size(); i++)
//     //             m_bImagesReady[i]= false;
//     //         printf("its buffering.......\n");

//     //         if (m_buf.isEmpty())
//     //         {
//     //             std::this_thread::sleep_for(std::chrono::milliseconds(5));
//     //             continue;
//     //         }
//     //         m_currentPairImagesInfo_Buff = std::move(m_buf.getFront());
            
//     //     }
        

//     // }

//     // while (!m_buf.isEmpty ()) 
//     // {
//     //     {
//     //         std::unique_lock<std::mutex> lk(m_mSaveMutex);
//     //         m_cDataReadySingleCon2.wait(lk, [this] {
//     //             bool sum = true;
//     //             unsigned int i = 0;
//     //             for (; i < m_bImagesReady.size(); i++){
                    
//     //                 sum = sum && m_bImagesReady[i];

//     //             }
//     //             if (i == 0 ) return false;
//     //             else return sum;

//     //         });
//     //         for (unsigned int i =0 ; i < m_bImagesReady.size(); i++)
//     //             m_bImagesReady[i]= false;

//     //         m_currentPairImagesInfo_Buff = m_buf.getFront();
        
//     //     }

//     //    printf("Buffer size:%d\n",m_buf.getSize());  
//     // }

   
    
    
//     while (true)
//     {
//         if (m_bExit)   break;
        
//         Write2Disk(m_buf.getFront ());
//     }

      
//     while (!m_buf.isEmpty ()) {

//         Write2Disk(m_buf.getFront ());
//         printf("Buffer size:%d\n",m_buf.getSize());
//     }







//     // while(true) 
//     // {
//     //     if (m_bExit) break;
//     //     {
//     //         //std::this_thread::sleep_for(std::chrono::milliseconds(m_nTriggerTimeInterval));
//     //         std::unique_lock<std::mutex> lk(m_mSaveMutex);
//     //         m_cDataReadySingleCon2.wait(lk, [this] {
//     //             bool sum = true;
//     //             unsigned int i = 0;
//     //             for (; i < m_bImagesReady.size(); i++){
//     //                 printf("m_bImagesReady:%d ",m_bImagesReady[i] );
//     //                 sum = sum && m_bImagesReady[i];

//     //             }
//     //              printf("\n");
//     //             if (i == 0 ) return false;
//     //             else return sum;

//     //         });
//     //         for (unsigned int i =0 ; i < m_bImagesReady.size(); i++)
//     //             m_bImagesReady[i]= false;
//     //         printf("its buffering.......\n");

//     //         if (m_buf.isEmpty())
//     //         {
//     //             std::this_thread::sleep_for(std::chrono::milliseconds(5));
//     //             continue;
//     //         }
//     //         m_currentPairImagesInfo_Buff = std::move(m_buf.getFront());
            
//     //     }
        

//     // }


//     // while (!m_buf.isEmpty ()) 
//     // {
//     //     {
//     //         std::unique_lock<std::mutex> lk(m_mSaveMutex);
//     //         m_cDataReadySingleCon2.wait(lk, [this] {
//     //             bool sum = true;
//     //             unsigned int i = 0;
//     //             for (; i < m_bImagesReady.size(); i++){
                    
//     //                 sum = sum && m_bImagesReady[i];

//     //             }
//     //             if (i == 0 ) return false;
//     //             else return sum;

//     //         });
//     //         for (unsigned int i =0 ; i < m_bImagesReady.size(); i++)
//     //             m_bImagesReady[i]= false;

//     //         m_currentPairImagesInfo_Buff = std::move(m_buf.getFront());
        
//     //     }

//     //    printf("Buffer size:%d\n",m_buf.getSize());  
//     // }




//     // std::vector<std::thread> ths;

//     // while (true)
//     // {
//     //      auto  buff= m_buf.getFront();
//     //     for (unsigned int i = 0 ; i < m_nDeviceNum ; i++)
//     //     {
//     //         ths.emplace_back(std::bind(&HikMultipleCameras::ThreadWrite2DiskFun, this, std::ref(buff[i]) , i));

//     //     }
//     //     for ( unsigned int i = 0 ; i < m_nDeviceNum ; i++) ths[i].join();

//     //     ths.clear();
      

//     //     if (m_bExit)  {
//     //         ths.clear();
//     //         break;
//     //     } 
        
//     // }

      
//     // while (!m_buf.isEmpty ()) {

//     //      auto  buff= m_buf.getFront();

//     //     for (unsigned int i = 0 ; i < m_nDeviceNum ; i++)
//     //     {
            
           
//     //         ths.emplace_back(std::bind(&HikMultipleCameras::ThreadWrite2DiskFun, this, std::ref(buff[i]), i));

//     //     }
//     //     for ( unsigned int i = 0 ; i < m_nDeviceNum ; i++) ths[i].join();
//     //     ths.clear();


//     //     printf("Buffer size:%d\n",m_buf.getSize());
//     // }






//     return 0;
// }
void HikMultipleCameras::Write2Disk( const std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]>>> & buff_item){
    unsigned char * pDataForSaveImage = NULL;
   // auto now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch());
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now(); 
     std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());


    for (int i = 0 ; i < buff_item.size(); i++)
    {
        MV_SAVE_IMAGE_PARAM_EX stParam;
        memset(&stParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));

        if ( !pDataForSaveImage) 
            pDataForSaveImage = (unsigned char*)malloc(buff_item[i].first.nWidth * buff_item[i].first.nHeight * 4 + 2048);


        stParam.enImageType = MV_Image_Jpeg; 
        stParam.enPixelType =  buff_item[i].first.enPixelType; 
        stParam.nWidth = buff_item[i].first.nWidth;       
        stParam.nHeight = buff_item[i].first.nHeight;       
        stParam.nDataLen = buff_item[i].first.nFrameLen;
        stParam.pData = buff_item[i].second.get();
        stParam.pImageBuffer =  pDataForSaveImage;
        stParam.nBufferSize = buff_item[i].first.nWidth * buff_item[i].first.nHeight * 4 + 2048;;  
        stParam.nJpgQuality = 99;  

        int nRet =  m_pcMyCameras[i]->SaveImage(&stParam);

        if(nRet != MV_OK)
        {
            printf("Failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
           // std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        char filepath[256];
        
     

        uint64_t timeStamp = (((uint64_t)  buff_item[i].first.nDevTimeStampHigh) << 32) +  buff_item[i].first.nDevTimeStampLow;

        #ifdef _MSC_VER 
        sprintf_s(filepath, sizeof(filepath), "ts_%03d_%s_w%d_h%d.jpg", buff_item[i].first.nFrameNum, m_mapSerials[i].c_str(), stParam.nWidth, stParam.nHeight );
        //printf ("%d. Image: %s \n", i, filepath);
        FILE* fp;
        fopen_s(&fp, filepath, "wb");
        #else
        sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[i].c_str(), stParam.nWidth, stParam.nHeight,  buff_item[i].first.nFrameNum);
        FILE* fp = fopen(filepath, "wb");
        #endif
      
        if (fp == NULL)
        {
            printf("fopen failed\n");
            break;
        }
        fwrite(pDataForSaveImage, 1, stParam.nImageLen, fp);
        fclose(fp);
       // DEBUG_PRINT("%d. Camera, Save image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%ld ms], SystemTimeDiff[%.3f ms]\n", i,m_stImagesInfo[i].nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000);




    }
    delete pDataForSaveImage;


}
// void HikMultipleCameras::Write2MP4FromBayer( int nCurrCamera)
// {
   
//     while (true)
//     {
       
//         if (m_bExit)   break;
//        // barrier.wait();
//         if (nCurrCamera == 0) 
//              buff_item = std::move(m_buf.getFront());
//         //    m_buffItem = std::make_unique<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>>(m_buf.getFront());
//          barrier1.wait();
      
//       converters[nCurrCamera]->convertAndEncodeBayerToH264(buff_item.at(nCurrCamera).second.get());

//     }

      
//     while (!m_buf.isEmpty ()) {
    
//         if (nCurrCamera == 0) 
//              buff_item = std::move(m_buf.getFront());
//             // m_buffItem = std::make_unique<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>>(m_buf.getFront());
//          barrier1.wait();
//         converters[nCurrCamera]->convertAndEncodeBayerToH264(buff_item.at(nCurrCamera).second.get());
       
//         printf("Buffer size:%d\n",m_buf.getSize());
//     }

  
   

   

// }


void HikMultipleCameras::Write2H264FromBayer3(int nCurrWriteThread) {

    std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> tmp_item;//(m_nDeviceNum, std::make_pair(MV_FRAME_OUT_INFO_EX{0}, nullptr));

    while(true)
    {
    
        if (m_bExit) break;
        // binary_sem_buf.acquire();
        // light_sem_buf.wait();
        if (m_buf.try_dequeue(tmp_item)){
          ; 
        } else {
            // printf_s("There is no item to be dequeued! Trying again ...\n");
           continue;
        }
        for (int i = 0 ; i < tmp_item.size(); i++)
        {
            
            std::shared_lock<std::shared_mutex> lk(codecMutexes_[i]);
            bool res = converter->convertAndEncodeBayerToH264(tmp_item[i].second.get(), i, nCurrWriteThread, tmp_item[i].first.nFrameNum);
            
            // if (res ) std::cout<<i<<". Cam Timestamp:"<<buffItem[i].first.nHostTimeStamp<<", "<<buffItem[i].first.nFrameNum<<std::endl;
           

        }
      
        
        bool res = std::all_of(converter->getResults()[nCurrWriteThread].begin(), converter->getResults()[nCurrWriteThread].end(), [](bool v) { return v; });
        if (res)
            converter->writeAllFrames2MP42(nCurrWriteThread);
        

        

    }

    while(m_buf.try_dequeue(tmp_item) != 0) {


        for (int i = 0 ; i < tmp_item.size(); i++)
        {
            
            std::shared_lock<std::shared_mutex> lk(codecMutexes_[i]);
            bool res = converter->convertAndEncodeBayerToH264(tmp_item[i].second.get(), i, nCurrWriteThread, tmp_item[i].first.nFrameNum);
        
           
        }


        

        bool res = std::all_of(converter->getResults()[nCurrWriteThread].begin(), converter->getResults()[nCurrWriteThread].end(), [](bool v) { return v; });
        if (res)
            converter->writeAllFrames2MP42(nCurrWriteThread); 
        
        printf_s("Buffer size:%zd\n",m_buf.size_approx());

        if (m_buf.size_approx() == 0){
            break;

        }  

    }
    BayerToH264Converter2::m_bExit = true;



}

void HikMultipleCameras::Write2H264FromBayerAtomic(int nCurrWriteThread) {

    std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> tmp_item;//(m_nDeviceNum, std::make_pair(MV_FRAME_OUT_INFO_EX{0}, nullptr));

    while(true)
    {
    
        if (m_bExit) break;
        // binary_sem_buf.acquire();
        // light_sem_buf.wait();
        if (m_buf.try_dequeue(tmp_item)){
          ; 
        } else {
            // printf_s("There is no item to be dequeued! Trying again ...\n");
           continue;
        }
        for (int i = 0 ; i < tmp_item.size(); i++)
        {
            
            // std::shared_lock<std::shared_mutex> lk(codecMutexes_[i]);
            bool res = converter->convertAndEncodeBayerToH264(tmp_item[i].second.get(), i, nCurrWriteThread, tmp_item[i].first.nFrameNum);
            
            // if (res ) std::cout<<i<<". Cam Timestamp:"<<buffItem[i].first.nHostTimeStamp<<", "<<buffItem[i].first.nFrameNum<<std::endl;
           

        }
      
        
        bool res = std::all_of(converter->getResults()[nCurrWriteThread].begin(), converter->getResults()[nCurrWriteThread].end(), [](bool v) { return v; });
        if (res)
            converter->writeAllFrames2MP42(nCurrWriteThread); 

        

    }

    while(m_buf.try_dequeue(tmp_item) != 0) {


        for (int i = 0 ; i < tmp_item.size(); i++)
        {
            
            // std::shared_lock<std::shared_mutex> lk(codecMutexes_[i]);
            bool res = converter->convertAndEncodeBayerToH264(tmp_item[i].second.get(), i, nCurrWriteThread, tmp_item[i].first.nFrameNum);
        
           
        }


        

        bool res = std::all_of(converter->getResults()[nCurrWriteThread].begin(), converter->getResults()[nCurrWriteThread].end(), [](bool v) { return v; });
        if (res)
            converter->writeAllFrames2MP42(nCurrWriteThread); 
        
        printf_s("Buffer size:%zd\n",m_buf.size_approx());

        if (m_buf.size_approx() == 0){
            break;

        }  

    }
    BayerToH264Converter2::m_bExit = true;

}


void HikMultipleCameras::Write2H264FromBayer4(int nCurrWriteThread) {
    
    std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> tmp_item;//(m_nDeviceNum, std::make_pair(MV_FRAME_OUT_INFO_EX{0}, nullptr));

    while(true)
    {
    
        if (m_bExit) break;
        // binary_sem_buf.acquire();
        // light_sem_buf.wait();
        if (m_buf.try_dequeue(tmp_item)){
          ; 
        } else {
            // printf_s("There is no item to be dequeued! Trying again ...\n");
           continue;
        }
        // printf_s("Buffer size:%zd\n",m_buf.size_approx());
        for (int i = 0 ; i < tmp_item.size(); i++)
        {
            
            std::shared_lock<std::shared_mutex> lk(codecMutexes_[i]);

            bool res = converter->convertAndEncodeBayerToH264(tmp_item[i].second.get(), i, nCurrWriteThread, tmp_item[i].first.nFrameNum);
            
           
        }
        bool res = std::all_of(converter->getResults()[nCurrWriteThread].begin(), converter->getResults()[nCurrWriteThread].end(), [](bool v) { return v; });
        if (res) {
            converter->push2Queue(nCurrWriteThread);

        }
    
    
    }


    while(m_buf.try_dequeue(tmp_item) != 0) {


        for (int i = 0 ; i < tmp_item.size(); i++)
        {
        
            std::shared_lock<std::shared_mutex> lk(codecMutexes_[i]);
            bool res = converter->convertAndEncodeBayerToH264(tmp_item[i].second.get(), i, nCurrWriteThread, tmp_item[i].first.nFrameNum);
            
           
        }
        bool res = std::all_of(converter->getResults()[nCurrWriteThread].begin(), converter->getResults()[nCurrWriteThread].end(), [](bool v) { return v; });
        if (res) {
            converter->push2Queue(nCurrWriteThread);

        }
        
        printf_s("Buffer size:%zd\n",m_buf.size_approx());

        if (m_buf.size_approx() == 0){
            break;

        }  

    }
    BayerToH264Converter2::m_bExit = true;

    


}



// void HikMultipleCameras::Write2H264FromBayer2(int nCurrCamera)
// {
//     while (true)
//     {
       
//         if (m_bExit)   break;
//         // barrier0.wait();

//         if (nCurrCamera == 0)
//         {
//             buff_item = std::move(m_buf.getFront());
//             converter->incrementCounter();
//             barr_cnt++;
//         }
//         barrier1.wait();
//         // converter->reset(nCurrCamera);
//         bool res = converter->convertAndEncodeBayerToH264(buff_item[nCurrCamera].second.get(), nCurrCamera);
//         // std::cout<<"result: "<<res<<std::endl;
//         // if (barr_cnt < 100) 
//         // barrier2.wait();
//         // if (res) {
//         //     // AVPacket *clone = av_packet_alloc();

//         //     // clone->data = reinterpret_cast<uint8_t*>(new uint64_t[(converter->getPacket(nCurrCamera)->size + AV_INPUT_BUFFER_PADDING_SIZE)/sizeof(uint64_t) + 1]);
//         //     // memcpy(clone->data, converter->getPacket(nCurrCamera)->data, converter->getPacket(nCurrCamera)->size);
            
//         //     // m_vectorAvPacketBuff.set(nCurrCamera, clone);
//         //     // AVPacket *pkt = converter->getPacket(nCurrCamera);
//         //     // av_packet_unref(converter->getPacket(nCurrCamera));
//         //     // printf("result: %d: %d\n", nCurrCamera, converter->getResults()[nCurrCamera]);
//         //     std::cout<<"result: "<<nCurrCamera<<", "<<converter->getResults()[nCurrCamera]<<", "<<converter->getPacket(nCurrCamera)->size<<", " <<std::endl;
//         //     m_cDataReadyWriMP4Con.notify_one();
//         //     converter->reset(nCurrCamera);

//         //     // av_packet_unref(pkt);

//         //     // av_packet_unref(m_vectorAvPacketBuff[nCurrCamera]);

//         // }

//         // if (converter->getResults()[nCurrCamera])
//         //     converter->writeSingleFrame2MP4(nCurrCamera);

//         barrier2.wait();
//         if (nCurrCamera == 0) {
//            bool res = std::all_of(converter->getResults().begin(), converter->getResults().end(), [](bool v) { return v; });
//            if (res)
//                converter->writeAllFrames2MP4(); 

//         }
           



    
//     }

//      while (!m_buf.isEmpty ()) {
    
//         if (nCurrCamera == 0)
//         {
//             buff_item = std::move(m_buf.getFront());
//             converter->incrementCounter();

//         }
//             // m_buffItem = std::make_unique<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>>(m_buf.getFront());
//         barrier1.wait();
//         bool res = converter->convertAndEncodeBayerToH264(buff_item.at(nCurrCamera).second.get(), nCurrCamera);
//         // converter->convertAndEncodeBayerToH264(buff_item[nCurrCamera].second.get(), nCurrCamera);
//         // m_vectorAvPacketBuff.set(nCurrCamera, converter->getPacket(nCurrCamera));
//         // m_cDataReadyWriMP4Con.notify_one();

        
        
//         barrier2.wait();
//         if (nCurrCamera == 0) {
//            bool res = std::all_of(converter->getResults().begin(), converter->getResults().end(), [](bool v) { return v; });
//            if (res)
//                converter->writeAllFrames2MP4(); 
//             printf("Buffer size:%d\n",m_buf.getSize());

//         }
       
       
//     }



// }



// int HikMultipleCameras::ThreadWrite2DiskFun2()
// {
//     unsigned char * pDataForSaveImage = NULL;
    
//     while(true)
//     {    
//         if (m_bExit) break;
//         const auto& buffItem = m_buf.getFront();
        
//         for (unsigned int i = 0 ; i < buffItem.size(); i++)
//         {
//             MV_SAVE_IMAGE_PARAM_EX stParam;
//             memset(&stParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));

//             if ( !pDataForSaveImage) 
//                 pDataForSaveImage = (unsigned char*)malloc(buffItem[i].first.nWidth * buffItem[i].first.nHeight * 4 + 2048);


//             stParam.enImageType = MV_Image_Jpeg; 
//             stParam.enPixelType =  buffItem[i].first.enPixelType; 
//             stParam.nWidth = buffItem[i].first.nWidth;       
//             stParam.nHeight = buffItem[i].first.nHeight;       
//             stParam.nDataLen = buffItem[i].first.nFrameLen;
//             stParam.pData = buffItem[i].second.get();
//             stParam.pImageBuffer =  pDataForSaveImage;
//             stParam.nBufferSize = buffItem[i].first.nWidth * buffItem[i].first.nHeight * 4 + 2048;;  
//             stParam.nJpgQuality = 99;  

//             int nRet =  m_pcMyCameras[i]->SaveImage(&stParam);

//             if(nRet != MV_OK)
//             {
//                 printf("Failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
//             // std::this_thread::sleep_for(std::chrono::milliseconds(5));
//                 continue;
//             }
//             char filepath[256];
            
        

//             uint64_t timeStamp = (((uint64_t)  buffItem[i].first.nDevTimeStampHigh) << 32) +  buffItem[i].first.nDevTimeStampLow;

//             #ifdef _MSC_VER 
//             sprintf_s(filepath, sizeof(filepath), "data1/ts_%03d_%s_w%d_h%d.jpg", buffItem[i].first.nFrameNum, m_mapSerials[i].c_str(), stParam.nWidth, stParam.nHeight );
//             //printf ("%d. Image: %s \n", i, filepath);
//             FILE* fp;
//             fopen_s(&fp, filepath, "wb");
//             #else
//             sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[i].c_str(), stParam.nWidth, stParam.nHeight,  buffItem[i].first.nFrameNum);
//             FILE* fp = fopen(filepath, "wb");
//             #endif
        
//             if (fp == NULL)
//             {
//                 printf("fopen failed\n");
//                 continue;
//             }
//             fwrite(pDataForSaveImage, 1, stParam.nImageLen, fp);
//             fclose(fp);




//         }
        
//     }




//     while(!m_buf.isEmpty())
//     {    
//         const auto& buffItem = m_buf.getFront();
        
//         for (unsigned int i = 0 ; i < buffItem.size(); i++)
//         {
//             MV_SAVE_IMAGE_PARAM_EX stParam;
//             memset(&stParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));

//             if ( !pDataForSaveImage) 
//                 pDataForSaveImage = (unsigned char*)malloc(buffItem[i].first.nWidth * buffItem[i].first.nHeight * 4 + 2048);


//             stParam.enImageType = MV_Image_Jpeg; 
//             stParam.enPixelType =  buffItem[i].first.enPixelType; 
//             stParam.nWidth = buffItem[i].first.nWidth;       
//             stParam.nHeight = buffItem[i].first.nHeight;       
//             stParam.nDataLen = buffItem[i].first.nFrameLen;
//             stParam.pData = buffItem[i].second.get();
//             stParam.pImageBuffer =  pDataForSaveImage;
//             stParam.nBufferSize = buffItem[i].first.nWidth * buffItem[i].first.nHeight * 4 + 2048;;  
//             stParam.nJpgQuality = 99;  

//             int nRet =  m_pcMyCameras[i]->SaveImage(&stParam);

//             if(nRet != MV_OK)
//             {
//                 printf("Failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
//             // std::this_thread::sleep_for(std::chrono::milliseconds(5));
//                 return -1;
//             }
//             char filepath[256];
            
        

//             uint64_t timeStamp = (((uint64_t)  buffItem[i].first.nDevTimeStampHigh) << 32) +  buffItem[i].first.nDevTimeStampLow;

//             #ifdef _MSC_VER 
//             sprintf_s(filepath, sizeof(filepath), "data1/ts_%03d_%s_w%d_h%d.jpg", buffItem[i].first.nFrameNum, m_mapSerials[i].c_str(), stParam.nWidth, stParam.nHeight );
//             //printf ("%d. Image: %s \n", i, filepath);
//             FILE* fp;
//             fopen_s(&fp, filepath, "wb");
//             #else
//             sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[i].c_str(), stParam.nWidth, stParam.nHeight,  buffItem[i].first.nFrameNum);
//             FILE* fp = fopen(filepath, "wb");
//             #endif
        
//             if (fp == NULL)
//             {
//                 printf("fopen failed\n");
//                 return -1;
//             }
//             fwrite(pDataForSaveImage, 1, stParam.nImageLen, fp);
//             fclose(fp);




//         }
//         printf("Buffer size:%d\n",m_buf.getSize());

//     }



        
    
//     delete pDataForSaveImage;


//     return 0;


// }

// int HikMultipleCameras::ThreadWrite2DiskFunEx2()
// {
//     unsigned char * pDataForSaveImage = NULL;
    
//     while(true)
//     {    
//         if (m_bExit) break;
//         const auto& buffItem = m_buf.getFront();
//         for (int i = 0 ; i < buffItem.size(); i++)
//         {
//             MV_SAVE_IMAGE_TO_FILE_PARAM_EX stParamFile;
//             memset(&stParamFile, 0, sizeof(MV_SAVE_IMAGE_TO_FILE_PARAM_EX));

//             if ( !pDataForSaveImage) 
//                 pDataForSaveImage = (unsigned char*)malloc(buffItem[i].first.nWidth * buffItem[i].first.nHeight * 4 + 2048);
//                 // pDataForSaveImage = (unsigned char*)malloc( m_params[i].nCurValue);


//             stParamFile.enImageType = MV_Image_Jpeg; 
//             stParamFile.enPixelType =  buffItem[i].first.enPixelType; 
//             stParamFile.nWidth = buffItem[i].first.nWidth;       
//             stParamFile.nHeight = buffItem[i].first.nHeight;       
//             stParamFile.nDataLen = buffItem[i].first.nFrameLen;
//             stParamFile.pData = buffItem[i].second.get();
//              char filepath[256];
            
        

//             uint64_t timeStamp = (((uint64_t)  buffItem[i].first.nDevTimeStampHigh) << 32) +  buffItem[i].first.nDevTimeStampLow;

//             #ifdef _MSC_VER 
//             sprintf_s(filepath, sizeof(filepath), "data1/ts_%03d_%s_w%d_h%d.jpg", buffItem[i].first.nFrameNum, m_mapSerials[i].c_str(), stParamFile.nWidth, stParamFile.nHeight );
//             //printf ("%d. Image: %s \n", i, filepath);
           
//             #else
//             sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[i].c_str(), stParamFile.nWidth, stParamFile.nHeight,  buffItem[i].first.nFrameNum);
//             FILE* fp = fopen(filepath, "wb");
//             #endif

//             stParamFile.pcImagePath = filepath ;
//             stParamFile.nQuality = 99;
//             // std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();
//             int nRet =  m_pcMyCameras[i]->SaveImage2File(&stParamFile);
//             // std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
//             // printf("Duration in Save Image DevIndex[%d]= %ld[ms]\n", i, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );

//             if(nRet != MV_OK)
//             {
//                 printf("Failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
//             // std::this_thread::sleep_for(std::chrono::milliseconds(5));
//                 continue;
//             }
        
//         }
       
//     }

//      while(!m_buf.isEmpty())
//     {    
//         const auto& buffItem = m_buf.getFront();
//         for (int i = 0 ; i < buffItem.size(); i++)
//         {
//             MV_SAVE_IMAGE_TO_FILE_PARAM_EX stParamFile;
//             memset(&stParamFile, 0, sizeof(MV_SAVE_IMAGE_TO_FILE_PARAM_EX));

//             if ( !pDataForSaveImage) 
//                 pDataForSaveImage = (unsigned char*)malloc(buffItem[i].first.nWidth * buffItem[i].first.nHeight * 4 + 2048);
//                 // pDataForSaveImage = (unsigned char*)malloc( m_params[i].nCurValue);


//             stParamFile.enImageType = MV_Image_Jpeg; 
//             stParamFile.enPixelType =  buffItem[i].first.enPixelType; 
//             stParamFile.nWidth = buffItem[i].first.nWidth;       
//             stParamFile.nHeight = buffItem[i].first.nHeight;       
//             stParamFile.nDataLen = buffItem[i].first.nFrameLen;
//             stParamFile.pData = buffItem[i].second.get();
//              char filepath[256];
            
        

//             uint64_t timeStamp = (((uint64_t)  buffItem[i].first.nDevTimeStampHigh) << 32) +  buffItem[i].first.nDevTimeStampLow;

//             #ifdef _MSC_VER 
//             sprintf_s(filepath, sizeof(filepath), "data1/ts_%03d_%s_w%d_h%d.jpg", buffItem[i].first.nFrameNum, m_mapSerials[i].c_str(), stParamFile.nWidth, stParamFile.nHeight );
//             //printf ("%d. Image: %s \n", i, filepath);
        
//             #else
//             sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[i].c_str(), stParamFile.nWidth, stParamFile.nHeight,  buffItem[i].first.nFrameNum);
//             FILE* fp = fopen(filepath, "wb");
//             #endif

//             stParamFile.pcImagePath = filepath ;
//             stParamFile.nQuality = 99;
//             // std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();
//             int nRet =  m_pcMyCameras[i]->SaveImage2File(&stParamFile);
//             // std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
//             // printf("Duration in Save Image DevIndex[%d]= %ld[ms]\n", i, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );

//             if(nRet != MV_OK)
//             {
//                 printf("Failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
//             // std::this_thread::sleep_for(std::chrono::milliseconds(5));
//                 continue;
//             }
        
//         }
//         printf("Buffer size:%d\n",m_buf.getSize());

       
//     }



   
        
    
//     delete pDataForSaveImage;


//     return 0;


// }


// int HikMultipleCameras::ThreadWrite2DiskFun( ImageBuffer<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>> & buff, int nCurCameraIndex )
// {
//     unsigned char * pDataForSaveImage = NULL;
    
//     while(true)
//     {    
//         if (m_bExit) break;
//         if (nCurCameraIndex == 0) m_buffItem = std::make_unique<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>>(buff.getFront());
//         barrier2.wait();
//         MV_SAVE_IMAGE_PARAM_EX stParam;
//         memset(&stParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));

//         if ( !pDataForSaveImage) 
//             pDataForSaveImage = (unsigned char*)malloc(m_buffItem->at(nCurCameraIndex).first.nWidth * m_buffItem->at(nCurCameraIndex).first.nHeight * 4 + 2048);


//         stParam.enImageType = MV_Image_Jpeg; 
//         stParam.enPixelType =  m_buffItem->at(nCurCameraIndex).first.enPixelType; 
//         stParam.nWidth = m_buffItem->at(nCurCameraIndex).first.nWidth;       
//         stParam.nHeight = m_buffItem->at(nCurCameraIndex).first.nHeight;       
//         stParam.nDataLen = m_buffItem->at(nCurCameraIndex).first.nFrameLen;
//         stParam.pData = m_buffItem->at(nCurCameraIndex).second.get();
//         stParam.pImageBuffer =  pDataForSaveImage;
//         stParam.nBufferSize = m_buffItem->at(nCurCameraIndex).first.nWidth * m_buffItem->at(nCurCameraIndex).first.nHeight * 4 + 2048;;  
//         stParam.nJpgQuality = 99;  

//         int nRet =  m_pcMyCameras[nCurCameraIndex]->SaveImage(&stParam);

//         if(nRet != MV_OK)
//         {
//             printf("Failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
//         // std::this_thread::sleep_for(std::chrono::milliseconds(5));
//             return -1;
//         }
//         char filepath[256];
        
    

//         uint64_t timeStamp = (((uint64_t)  m_buffItem->at(nCurCameraIndex).first.nDevTimeStampHigh) << 32) +  m_buffItem->at(nCurCameraIndex).first.nDevTimeStampLow;

//         #ifdef _MSC_VER 
//         sprintf_s(filepath, sizeof(filepath), "ts_%03d_%s_w%d_h%d.jpg", m_buffItem->at(nCurCameraIndex).first.nFrameNum, m_mapSerials[nCurCameraIndex].c_str(), stParam.nWidth, stParam.nHeight );
//         //printf ("%d. Image: %s \n", i, filepath);
//         FILE* fp;
//         fopen_s(&fp, filepath, "wb");
//         #else
//         sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[nCurCameraIndex].c_str(), stParam.nWidth, stParam.nHeight,  m_buffItem->at(nCurCameraIndex).first.nFrameNum);
//         FILE* fp = fopen(filepath, "wb");
//         #endif
    
//         if (fp == NULL)
//         {
//             printf("fopen failed\n");
//             return -1;
//         }
//         fwrite(pDataForSaveImage, 1, stParam.nImageLen, fp);
//         fclose(fp);
//        // m_bImagesReady[nCurCameraIndex]= true;
//     // DEBUG_PRINT("%d. Camera, Save image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%ld ms], SystemTimeDiff[%.3f ms]\n", i,m_stImagesInfo[i].nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000);
//     }




//     while(!m_buf.isEmpty())
//     {    
//         if (nCurCameraIndex == 0) m_buffItem = std::make_unique<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>>(buff.getFront());
//         barrier2.wait();
//         MV_SAVE_IMAGE_PARAM_EX stParam;
//         memset(&stParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));

//         if ( !pDataForSaveImage) 
//             pDataForSaveImage = (unsigned char*)malloc(m_buffItem->at(nCurCameraIndex).first.nWidth * m_buffItem->at(nCurCameraIndex).first.nHeight * 4 + 2048);


//         stParam.enImageType = MV_Image_Jpeg; 
//         stParam.enPixelType =  m_buffItem->at(nCurCameraIndex).first.enPixelType; 
//         stParam.nWidth = m_buffItem->at(nCurCameraIndex).first.nWidth;       
//         stParam.nHeight = m_buffItem->at(nCurCameraIndex).first.nHeight;       
//         stParam.nDataLen = m_buffItem->at(nCurCameraIndex).first.nFrameLen;
//         stParam.pData = m_buffItem->at(nCurCameraIndex).second.get();
//         stParam.pImageBuffer =  pDataForSaveImage;
//         stParam.nBufferSize = m_buffItem->at(nCurCameraIndex).first.nWidth * m_buffItem->at(nCurCameraIndex).first.nHeight * 4 + 2048;;  
//         stParam.nJpgQuality = 99;  

//         int nRet =  m_pcMyCameras[nCurCameraIndex]->SaveImage(&stParam);

//         if(nRet != MV_OK)
//         {
//             printf("Failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
//         // std::this_thread::sleep_for(std::chrono::milliseconds(5));
//             return -1;
//         }
//         char filepath[256];
        
    

//         uint64_t timeStamp = (((uint64_t)  m_buffItem->at(nCurCameraIndex).first.nDevTimeStampHigh) << 32) +  m_buffItem->at(nCurCameraIndex).first.nDevTimeStampLow;

//         #ifdef _MSC_VER 
//         sprintf_s(filepath, sizeof(filepath), "ts_%03d_%s_w%d_h%d.jpg", m_buffItem->at(nCurCameraIndex).first.nFrameNum, m_mapSerials[nCurCameraIndex].c_str(), stParam.nWidth, stParam.nHeight );
//         //printf ("%d. Image: %s \n", i, filepath);
//         FILE* fp;
//         fopen_s(&fp, filepath, "wb");
//         #else
//         sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[nCurCameraIndex].c_str(), stParam.nWidth, stParam.nHeight,  m_buffItem->at(nCurCameraIndex).first.nFrameNum);
//         FILE* fp = fopen(filepath, "wb");
//         #endif
    
//         if (fp == NULL)
//         {
//             printf("fopen failed\n");
//             return -1;
//         }
//         fwrite(pDataForSaveImage, 1, stParam.nImageLen, fp);
//         fclose(fp);
//         printf("Buffer size:%d\n",m_buf.getSize());
//        // m_bImagesReady[nCurCameraIndex]= true;
//     // DEBUG_PRINT("%d. Camera, Save image succeeded, nFrameNum[%d], DeviceTimeStamp[%.3f ms], TimeDiff[%.3f ms], SystemTimeStamp[%ld ms], SystemTimeDiff[%.3f ms]\n", i,m_stImagesInfo[i].nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)), double(systemTimeDiff)/1000);
//     }



        
    
//     delete pDataForSaveImage;


//     return 0;


// }



int HikMultipleCameras::ThreadCheck4H264Fun( ) {
    while(true)
    {
        if (m_bExit) break;

        {
            std::unique_lock<std::mutex> lk(m_mWriteMp4Mutex);
            m_cDataReadyWriMP4Con.wait(lk, [this] {
            // for (int i = 0 ; i < converter->getResults().size(); i++)
                // printf("Inside wait - converter results: %d, %d \n",i, converter->getResults()[i] );

            // return std::all_of(converter->getResults().begin(), converter->getResults().end(), [](bool v) { return v; });
            bool sum=true;
            int i = 0;
            for (; i < converter->getResults().size(); i++){
                bool res = std::all_of(converter->getResults()[i].begin(), converter->getResults()[i].end(), [](bool v) { return v; });
                sum = sum && res;

            }
            std::cout<<"sum:"<<sum<<std::endl;
            if (i == 0 ) return false;
            else return sum;
            

            });
        }
        // std::fill(converter->getResults().begin(), converter->getResults().end(), false);
        auto framNums = converter->getFrameNums();

        std::vector<int> index(framNums.size(), 0);
        for (int i = 0 ; i != index.size() ; i++) {
            index[i] = i;
        }
        std::sort(index.begin(), index.end(),
            [&](const int& a, const int& b) {
                return (framNums[a] < framNums[b]);
            }
        );
        for (int i = 0 ; i != index.size() ; i++) {
            std::cout << index[i] << ":"<< framNums[index[i]]<<std::endl;
        }
        // std::vector<int> y(framNums.size());
        // std::size_t n(0);
        // std::generate(std::begin(y), std::end(y), [&]{ return n++; });

        // std::sort(  std::begin(y), 
        //             std::end(y),
        //             [&](int i1, int i2) { return framNums[i1] < framNums[i2]; } );
        // auto idxs = tag_sort(framNums);
        // for (auto && elem : idxs)
        // std::cout << elem << " : " << framNums[elem] << std::endl;
        for (unsigned int i = 0 ; i < m_nWriteThreads; i++)
            converter->writeAllFrames2MP42(index[i]);

      
       
        
     


    }
   
    return MV_OK;

}


// int HikMultipleCameras::ThreadWrite2MP4Fun2( )
// {
    
//     while(true)
//     {    
//         if (m_bExit) break;
//         const auto& buffItem = m_buf.getFront();
        
//         for (unsigned int i = 0 ; i < buffItem.size(); i++)
//         {
//             FrameFeatures frameFeat = {buffItem[i].first.enPixelType, buffItem[i].first.nWidth, buffItem[i].first.nHeight, buffItem[i].first.nFrameLen, buffItem[i].first.nFrameNum};
            
//             strcpy_s(frameFeat.serialNum, buffItem[i].first.nSerialNum);
//             //printf("%d. Camera, array size: %d\n", i, frameFeat.frameLen);
//             {
//                 std::lock_guard<std::mutex> lk(m_mMp4WriteMutex);
//                 if (!m_Containers[i].writeImageToContainer((char*) buffItem[i].second.get(), frameFeat, m_its[i]*100000, STREAM_INDEX_IMG)) 
//                 {
//                     printf("Cannot write texture to container\n");
//                     return -1 ;
//                 }
//                 m_its[i]++;
//             }

//         }
//     }
    
//     while(!m_buf.isEmpty())
//     {    
        
//         const auto& buffItem = m_buf.getFront();
        
//         for (unsigned int i = 0 ; i < buffItem.size(); i++)
//         {
//             // unsigned int frameLen = buffItem[i].first.nFrameLen;
//             // unsigned int frameNum = buffItem[i].first.nFrameNum;
//             //char serialNum[128];
//             //strcpy_s(serialNum, buffItem[i].first.nSerialNum );
//             FrameFeatures frameFeat = {buffItem[i].first.enPixelType, buffItem[i].first.nWidth, buffItem[i].first.nHeight, buffItem[i].first.nFrameLen, buffItem[i].first.nFrameNum};
//             // frameFeat.frameLen = frameLen;
//             // frameFeat.frameNum = frameNum;
//             strcpy_s(frameFeat.serialNum, buffItem[i].first.nSerialNum);
//              {
//                 std::lock_guard<std::mutex> lk(m_mMp4WriteMutex);
//                 if (!m_Containers[i].writeImageToContainer((char*) buffItem[i].second.get(), frameFeat, m_its[i]*100000, STREAM_INDEX_IMG)) 
//                 {
//                     printf("Cannot write texture to container\n");
//                     return -1 ;
//                 }
//                 m_its[i]++;
//              }

//         }
//         if (m_buf.getSize()==0) return 0;
//         printf("Buffer size: %d\n", m_buf.getSize());
//     }




//     return 0;
// }


void HikMultipleCameras::Write2MP4( )
{
   converter->writeAllFrames2MP43();

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
    printf("Number of Devices: %d\n", m_nDeviceNum);


    for (unsigned int i = 0; i < m_nDeviceNum; i++)
    {
        
        if (m_pcMyCameras[i])
        {
            memset(&(m_stImagesInfo[i]), 0, sizeof(MV_FRAME_OUT_INFO_EX));
            
            
            nRet = m_pcMyCameras[i]->StartGrabbing();
            if (MV_OK != nRet)
            {
                printf("Start grabbing fail! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }
            m_bStartGrabbing = true;

            memset(&m_params[i], 0, sizeof(MVCC_INTVALUE));
            nRet =  m_pcMyCameras[i]->GetIntValue("PayloadSize", &m_params[i]);
            if (nRet != MV_OK) {
                printf("Get PayloadSize failed! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }

           
            m_tGrabThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadGrabWithGetImageBufferFun, this, i)));
            if (i == 0 )
                printf("Grabbing just started!\n");
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
        
        for (unsigned int  i = 0; i < m_nDeviceNum; i++)
        {
                if (m_pcMyCameras[i])
                {
                    m_pcMyCameras[i]->CommandExecute("TriggerSoftware");
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
            if (m_bExit) break;
        
            nRet = HikCamera::GIGEIssueActionCommand(&m_actionCMDInfo, &m_actionCMDResList);
            if (MV_OK != nRet)
            {
                printf("Issue Action Command fail! nRet [0x%x]\n", nRet);
                continue;
            }
            //printf("NumResults = %d\r\n",m_actionCMDResList.nNumResults);

            MV_ACTION_CMD_RESULT* pResults = m_actionCMDResList.pResults;
            for (unsigned int i = 0;i < m_actionCMDResList.nNumResults;i++)
            {
                //Print the device infomation
                DEBUG_PRINT("Ip == %s, Status == 0x%x\r\n",pResults->strDeviceAddress,pResults->nStatus);
                pResults++;
            }
       
        
       
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

   
    

    int nRet = -1, nRetOne;
    bool bRet = false;
  

   
    
    if (m_nTriggerMode == MV_TRIGGER_MODE_ON)
        m_tTriggerThread->join();



    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    for (unsigned int i = 0; i < m_nWriteThreads; i++)  
        m_tSaveAsMP4Threads[i]->join();
    

    // m_tWrite2MP4Thread->join();



    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            // m_tSaveAsMP4Threads[i]->join();

            m_tGrabThreads[i]->join();
      
        }
        
    }
    m_tCheckBuffThread->join();



    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        
        
        nRet = m_pcMyCameras[i]->StopGrabbing();
        if (MV_OK != nRet)
        {
            printf("Stop grabbing fail! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
            bRet = true;
            nRetOne = nRet;
        } else {

            printf("Stop grabbing success! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
        }
    }

    


    if (nRet == -1) 
    {
        printf("There is something wrong with the number of opened devices in StopGrabbing! \n");
        return nRet;
    }
    if (bRet)
    {
        printf("Cannot stop grabbing for at least one camera\n");
        return nRetOne;

    }


   

   

   

    return MV_OK;
   

}

int HikMultipleCameras::ReadMp4Write2DiskAsJpgInThreads()
{
    
    for (unsigned int i = 0 ; i < m_nDeviceNum ; i++)
        m_Containers[i].openForRead((char*)m_Containers[i].getFileName().c_str());
    for (unsigned int i = 0 ; i < m_nDeviceNum ; i++)
    {
        m_tReadMp4WriteThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadReadMp4Write2DiskAsJpgFun, this, i)));

    }
    return MV_OK;

}

int HikMultipleCameras::JoinReadMp4Write2DiskAsJpgInThreads()
{
    for (unsigned int i = 0 ; i < m_nDeviceNum ; i++)
    {
        m_tReadMp4WriteThreads[i]->join() ;
        m_Containers[i].close();
    }
    
    return MV_OK;

}

int HikMultipleCameras::ThreadReadMp4Write2DiskAsJpgFun(int nCurCameraIndex ) 
{
    int length;
    char *data;
    unsigned char * pDataForSaveImage = NULL;

    while (true) 
    {

        //m_Containers[nCurCameraIndex].setWriteMode(false);
        int streamIndex = m_Containers[nCurCameraIndex].read(data, length);
        // for (unsigned int i = 0 ; i < m_nDeviceNum ; i++)
        // {
          printf("length: %d\n", length);



        // }
        if (streamIndex == -1) 
        {
            printf("Cannot read data from Mp4 Or End of file, DevIndex[%d]\n", nCurCameraIndex);
            break;
        }
        FrameFeatures tmpFrameFeat;

        memcpy(&tmpFrameFeat, data, sizeof(FrameFeatures));
        printf("TmpFrameFeatures Serial Number: %s\n", tmpFrameFeat.serialNum);
        char* newData = new  char[length - sizeof(FrameFeatures)];
        memcpy(newData, (char*)data +sizeof(FrameFeatures), length - sizeof(FrameFeatures) );
       // printf("TmpFrameFeatures Serial Number: %s\n", tmpFrameFeat.serialNum);
        if ( !pDataForSaveImage) 
                pDataForSaveImage = (unsigned char*)malloc(tmpFrameFeat.frameWidth *tmpFrameFeat.frameHeight * 4 + 2048);
        MV_SAVE_IMAGE_PARAM_EX stParam;
        memset(&stParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));

        stParam.enImageType = MV_Image_Jpeg; 
        stParam.enPixelType =  tmpFrameFeat.framePixelType; 
        stParam.nWidth = tmpFrameFeat.frameWidth;       
        stParam.nHeight = tmpFrameFeat.frameHeight;       
        stParam.nDataLen = length;
        stParam.pData = (unsigned char*)newData;
        stParam.pImageBuffer =  pDataForSaveImage;
        stParam.nBufferSize = tmpFrameFeat.frameWidth * tmpFrameFeat.frameHeight * 4 + 2048;;  
        stParam.nJpgQuality = 99;  
        int nRet =  m_pcMyCameras[nCurCameraIndex]->SaveImage(&stParam);

        if(nRet != MV_OK)
        {
            printf("Failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
            // std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        char filepath[256];
        
        


        #ifdef _MSC_VER 
        sprintf_s(filepath, sizeof(filepath), "ts_%03d_%s_w%d_h%d.jpg", tmpFrameFeat.frameHeight, m_mapSerials[nCurCameraIndex].c_str(), stParam.nWidth, stParam.nHeight );
       // sprintf_s(filepath, sizeof(filepath), "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[nCurCameraIndex].c_str(), stParam.nWidth, stParam.nHeight,  tmpFrameFeat.frameNum);
        FILE* fp;
        fopen_s(&fp, filepath, "wb");
        #else
        sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[nCurCameraIndex].c_str(), stParam.nWidth, stParam.nHeight,  tmpFrameFeat.frameNum);
        FILE* fp = fopen(filepath, "wb");
        #endif
        
        if (fp == NULL)
        {
            printf("fopen failed\n");
            break;
        }
        fwrite(pDataForSaveImage, 1, stParam.nImageLen, fp);
        fclose(fp);
        delete [] newData;

    }   


    return 0;

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
    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            nRet = m_pcMyCameras[i]->SetEnumValueByString("TriggerSource", m_sTriggerSource.c_str());
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
    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            nRet = m_pcMyCameras[i]->SetEnumValueByString("TriggerSource", m_sTriggerSource.c_str());
            if (nRet != MV_OK)
            {
                printf("Cannot set  Trigger GigE Action1! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }
            nRet = m_pcMyCameras[i]->SetIntValue("ActionDeviceKey", m_nDeviceKey);
            if (nRet != MV_OK)
            {
                printf("Cannot set Action Device Key! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }            

            nRet = m_pcMyCameras[i]->SetIntValue("ActionGroupMask", m_nGroupMask);
            if (nRet != MV_OK)
            {
                printf("Cannot set Action Group Mask! DevIndex[%d], nRet[%#x]. Exiting...\r\n", i, nRet);
                return nRet;
            }  
            nRet = m_pcMyCameras[i]->SetIntValue("ActionGroupKey", n_nGroupKey);
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
    m_actionCMDInfo.pBroadcastAddress = m_pBroadcastAddress.c_str();
    m_actionCMDInfo.nTimeOut = m_nTriggerTimeInterval;
    printf("nTimeOut: %d\n",  m_actionCMDInfo.nTimeOut);

    m_actionCMDInfo.bActionTimeEnable = 0;


    return nRet;

}


// Save Images of Cameras in threads
int HikMultipleCameras::SaveImages2Disk()
{
    
    if (false == m_bStartGrabbing)
    {        
        printf("'m_bStartGrabbing' set to false! Cannot save images. \n");
        return -1;
    }

    
    for (unsigned int i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
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



