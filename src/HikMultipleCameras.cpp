
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
#include <Windows.h>
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

thread_local unsigned count_buf = 0;
thread_local unsigned counter = 0;

thread_local double last_buf = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();

#define FPS_CALC_THREAD_BUF(_WHAT_, buff, ncurrCameraIndex) \
do \
{ \
    double now_buf = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); \
    ++count_buf; \
    ++counter; \
    if (now_buf - last_buf >= 5.0) \
    { \
      std::cerr <<  ncurrCameraIndex<< ". Camera,"<<" Average framerate("<< _WHAT_ << "): " << double(count_buf)/double(now_buf - last_buf) << " Hz. Queue size: " << buff[ncurrCameraIndex].size () << " Frame Number: "<<counter <<"\n"; \
      count_buf = 0; \
      last_buf = now_buf; \
    } \
}while(false)


// #define FPS_CALC_BUF(_WHAT_, buff) \
// do \
// { \
//     static unsigned count_buf = 0;\
//     static unsigned counter = 0; \
//     static double last_buf = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();\
//     double now_buf = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); \
//     ++count_buf; \
//     ++counter; \
//     if (now_buf - last_buf >= 5.0) \
//     { \
//       std::cerr << "Average framerate("<< _WHAT_ << "): " << double(count_buf)/double(now_buf - last_buf) << " Hz. Queue size: " << buff.size () << " Frame Number: "<<counter <<"\n"; \
//       count_buf = 0; \
//       last_buf = now_buf; \
//     } \
// }while(false)


// HikMultipleCameras dialog
HikMultipleCameras::HikMultipleCameras( std::chrono::system_clock::time_point timePoint, const std::string& cameraSettingsFile):
      m_nDeviceNum(0)
    , m_nDeviceNumDouble(0)
    , m_nWriteThreads(6)
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
   
    
    EnumDevicesAvoidDublication();
   // EnumDevicesByIPAddress();
//    EnumDevices();

    if (m_nDeviceNum > 0)
    {
        
        converter = std::make_unique<BayerToH264Converter>(m_nDeviceNum, 1920, 1200);
        converter->initializeContexts("AllCameras", m_mapSerials);
        m_bImagesOk.resize(m_nDeviceNum, false);
        m_bImagesCheck.resize(m_nDeviceNum, false);
        m_bImagesReady.resize(m_nDeviceNum, false);
        m_bDataReady.resize(m_nDeviceNum, false);
        bDataReady.resize(m_nDeviceNum, true);
        m_params.resize(m_nDeviceNum, {0});
        m_stImagesInfo.resize(m_nDeviceNum, {0});
        m_nSaveImagesBufSize.resize(m_nDeviceNum, 0);
        m_Containers.resize(m_nDeviceNum, Container());
      
        m_pairImagesInfo_Buff.resize(m_nDeviceNum);
        m_vectorAvPacketBuff.resize(m_nDeviceNum, nullptr);
      
      
        //m_pairImagesInfo_Buff.resize(m_nDeviceNum, std::make_pair(MV_FRAME_OUT_INFO_EX{0}, nullptr));
      
        m_mProduceMutexes = std::vector<std::mutex>(m_nDeviceNum);
        m_mProduceMutexes = std::vector<std::mutex>(m_nDeviceNum);
        m_mCheckMutexes = std::vector<std::mutex>(m_nDeviceNum);
        codecMutexes_ =  std::vector<std::mutex>(m_nDeviceNum);
        m_cDataReadyCon = condVector(m_nDeviceNum);
        m_queue_vecs.resize(m_nDeviceNum);

       // m_cdataCheckCon = condVector(m_nDeviceNum);
        
         // if (!container.open((char*)"test_hikrobot_jpgs.mp4", true)){
        //     exit(0);
        // };


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
            // std::string fileNameTmp = "hikrobot_" + m_mapSerials[i] + "_" + std::to_string(sec.count()) + ".mp4";
            // if (!m_Containers[i].open((char*)fileNameTmp.c_str(), true))
            //     exit(0);
            // m_queue_vecs.emplace_back();

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
int HikMultipleCameras::ThreadConsumeAnWrite2DiskAsMp4Fun(int nCurCameraIndex)
{
    
        
         while(true) {

                const auto buff_item = m_queue_vecs[nCurCameraIndex].front();
                m_queue_vecs[nCurCameraIndex].pop_front();

                // std::cout<<nCurCameraIndex<<" .cam, FrameNum:"  << " "<<buff_item.second.get()[234]<<std::endl;
                bool res = converter->convertAndEncodeBayerToH264(buff_item.second.get(), nCurCameraIndex, buff_item.first.nHostTimeStamp,  buff_item.first.nFrameNum);
                if (res  )
                        // converter->push
                    converter->writeSingleFrame2MP4(nCurCameraIndex);
                FPS_CALC_THREAD_BUF ("Consuming from Buffer callback", m_queue_vecs, nCurCameraIndex);


                if (m_bExit) break; 
         }

         while (m_queue_vecs[nCurCameraIndex].size() != 0){

                const auto buff_item = m_queue_vecs[nCurCameraIndex].front();
                // printf("%d. Cam, Left Buffer Size: %d", nCurCameraIndex, m_queue_vecs[nCurCameraIndex].size());
                m_queue_vecs[nCurCameraIndex].pop_front();
                bool res = converter->convertAndEncodeBayerToH264(buff_item.second.get(), nCurCameraIndex, buff_item.first.nHostTimeStamp,  buff_item.first.nFrameNum);
                if (res )
                    converter->writeSingleFrame2MP4(nCurCameraIndex);
                FPS_CALC_THREAD_BUF ("Consuming from Buffer callback", m_queue_vecs, nCurCameraIndex);
                // printf_s("%d. Cam, Left Buffer Size: %zd\n", nCurCameraIndex, m_queue_vecs[nCurCameraIndex].size());


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
            
                // std::chrono::system_clock::time_point begin = std::chrono::system_clock::now();      
                 
                nRet = m_pcMyCameras[nCurCameraIndex]->GetImageBuffer(&stImageOut, 1000);
           


                MV_FRAME_OUT_INFO_EX tmpFrame={0};
                memset(&tmpFrame, 0, sizeof(MV_FRAME_OUT_INFO_EX));

                // std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
                #ifdef _MSC_VER 
                // DEBUG_PRINT("Grabbing duration in DevIndex[%d]= %lld[ms]\n", nCurCameraIndex, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );
                #else
                DEBUG_PRINT("Grabbing duration in DevIndex[%d]= %ld[ms]\n", nCurCameraIndex, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );
                #endif  
            //    printf("Grabbing duration in DevIndex[%d]= %lld[ms]\n", nCurCameraIndex, std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() );
                
                if (nRet == MV_OK)
                {
                    
                    {
                        // std::unique_lock<std::mutex> lk(m_mProduceMutexes[nCurCameraIndex]);
                        // m_cDataReadySingleCon2.wait(lk, [this, nCurCameraIndex]{

                        //     return bDataReady[nCurCameraIndex];

                        // });
                      
                        
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
                        // printf_s("DevIndex[%d], nFrameNum[%d], DeviceTimeStamp[%.3f ms],  TimeDiff[%.3f ms], SystemTimeStamp[%lld ms], HostTimeStamp[%lld ms]\n", nCurCameraIndex,  stImageOut.stFrameInfo.nFrameNum, double(timeStamp)/1000000, float(timeDif)/1000000,  uint64_t(round(double(microseconds)/1000)),  hostTimeStamp);



                        if (stImageOut.pBufAddr != NULL)
                        {   
        
                            std::shared_ptr<uint8_t[]>  tmpSharedptr (new uint8_t[stImageOut.stFrameInfo.nFrameLen]);
                            memcpy(tmpSharedptr.get(), stImageOut.pBufAddr, stImageOut.stFrameInfo.nFrameLen);
                          

                            memcpy(&tmpFrame, &(stImageOut.stFrameInfo), sizeof(MV_FRAME_OUT_INFO_EX)); 

                            tmpFrame.nHostTimeStamp = double(timeStamp)/1000000;

                            strcpy_s(tmpFrame.nSerialNum, m_mapSerials[nCurCameraIndex].c_str());
                            // printf("tmpFrame.nHostTimeStamp:%I64d\n",tmpFrame.nHostTimeStamp);
    
                            // m_pairImagesInfo_Buff.set(nCurCameraIndex, std::make_pair(tmpFrame, tmpSharedptr));
                            
                            m_queue_vecs[nCurCameraIndex].push_back(std::make_pair(tmpFrame, tmpSharedptr));
                            
                         
                           
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

            if (nCurCameraIndex ==0 && ++m_cnt % 20 == 0 )
            {
                
                    MEMORYSTATUSEX memStatus;
                    memStatus.dwLength = sizeof(MEMORYSTATUSEX);

                    if (GlobalMemoryStatusEx(&memStatus)) {
                        int total_mem =  (memStatus.ullTotalPhys / (1024 * 1024));
                        int left_mem = (memStatus.ullAvailPhys / (1024 * 1024));
                        double ratio = double(left_mem)/double(total_mem);
                        // std::cout<<"ratio:"<<ratio<<std::endl;
                        if (ratio < 0.06) {
                            std::cout << "Memory is getting full! Exiting..."<<std::endl;
                            std::cout << "Total Physical Memory: " << (memStatus.ullTotalPhys / (1024 * 1024)) << " MB" << std::endl;
                            std::cout << "Available Physical Memory: " << (memStatus.ullAvailPhys / (1024 * 1024)) << " MB" << std::endl;
                            HikMultipleCameras::m_bExit = true;
                        }
                    } else {
                        std::cerr << "Error getting memory status." << std::endl;
                    }
                
            
             }


            if (m_bExit) m_bStartGrabbing = false;
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



  //  m_tSaveBufThread = std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadSave2BufferFun, this));
   // m_tSaveDiskThread =  std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadSave2DiskFun, this));
   
    // m_tCheckBuffThread = std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadCheckBufferFun, this));
    // m_tCheck4H264Thread = std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadCheck4H264Fun, this));

    for (unsigned int i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {

            // m_bStartConsuming = true;
            m_tConsumeThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadConsumeAnWrite2DiskAsMp4Fun, this, i)));
            if (m_tConsumeThreads[i] == nullptr)
            {
                printf("Create consume thread fail! DevIndex[%d]\r\n", i);
                return -1;
            }

        }
    }

   

    // for (unsigned int i = 0; i < m_nWriteThreads ; i++)
    //     m_tWrite2DiskThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadWrite2DiskFunEx2, this)));
   
    // for (unsigned int i = 0; i < m_nDeviceNum; i++) 
    // {
    //     m_tSaveAsMP4Threads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::Write2H264FromBayer2, this, i)));
    // }
    
    // for (unsigned int i = 0; i < m_nWriteThreads ; i++)
    //     m_tSaveAsMP4Threads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::Write2H264FromBayer3, this, i)));


    return MV_OK;

}   


// int HikMultipleCameras::ThreadCheckBufferFun() 
// {
//     while(true)
//     {
//         if (m_bExit) {
//             // m_buf.pop_back();
//             break;
//         }
//         std::unique_lock<std::mutex> lk(m_mGrabMutex);
//         m_cDataReadySingleCon1.wait(lk, [this] {
//             bool sum=true;
//             int i = 0;
//             for (; i < m_bImagesCheck.size(); i++){
                
//                 sum = sum && m_bImagesCheck[i];

//             }
//             if (i == 0 ) return false;
//             else return sum;

//         });

//         for (int i =0 ; i < m_bImagesCheck.size(); i++)
//             m_bImagesCheck[i]= false;
       


//         // std::cout<<"Frame nums:"<<m_pairImagesInfo_Buff.data.at(0).first.nFrameNum<<" "<<m_pairImagesInfo_Buff.data.at(8).first.nFrameNum<<" "<<m_pairImagesInfo_Buff.data.at(15).first.nFrameNum<<" "<<m_pairImagesInfo_Buff.data.at(23).first.nFrameNum<<std::endl;
//         if (!m_buf.pushBack(m_pairImagesInfo_Buff.data) )
//         {
            
//             printf ("Warning! Buffer was full, overwriting data!\n");
            
//         }
            

        

//         // FPS_CALC_BUF ("Copying to Buffer callback", m_buf);

       
        
    
//     }
//     return 0;

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
//             sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[i].c_str(), stParam.nWidth, stParam.nHeight,  buffItem[i].first.nFrameNum);
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
//             sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", m_mapSerials[i].c_str(), stParam.nWidth, stParam.nHeight,  buffItem[i].first.nFrameNum);
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

    
    
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (unsigned int  i = 0; i < m_nDeviceNum; i++)
    {
        if (m_pcMyCameras[i])
        {
            // m_tSaveAsMP4Threads[i]->join();

            m_tGrabThreads[i]->join();
            m_tConsumeThreads[i]->join();
      
        }
        
    }

   
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
            m_tConsumeThreads.push_back(std::make_unique<std::thread>(std::bind(&HikMultipleCameras::ThreadConsumeAnWrite2DiskAsMp4Fun, this, i)));
            if (m_tConsumeThreads[i] == nullptr)
            {
                printf("Create consume thread fail! DevIndex[%d]\r\n", i);
                return -1;
            }

        }
    }
    return MV_OK;

}   



