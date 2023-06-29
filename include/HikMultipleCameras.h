// header file

#pragma once
#include <stdio.h>
#include <thread>
#include <memory>
#include <map>
#include "HikCamera.h"
#include "ImageBuffer.h"



class HikMultipleCameras 
{
// Construction
public:
    using thread = std::unique_ptr<std::thread>;
    using threadVector = std::vector<std::unique_ptr<std::thread>>;
    using timePoint = std::chrono::system_clock::time_point;
    using byteArrayVector = std::vector<std::unique_ptr<uint8_t[]>> ;
    using frameVector = std::vector<MV_FRAME_OUT_INFO_EX>;
    using mvccIntVector = std::vector<MVCC_INTVALUE>;
    using condVector = std::vector<std::condition_variable>;
    using hikCamVector = std::vector<std::unique_ptr<HikCamera>>;
    static bool m_bExit;

	HikMultipleCameras(ImageBuffer &buf, std::chrono::system_clock::time_point postime);	      
   
private:
    MV_CC_DEVICE_INFO_LIST  m_stDevList;
    uint                    m_nDeviceNum;
    bool                    m_bOpenDevice;
    timePoint               m_timePoint;
    bool                    m_bStartGrabbing;
    bool                    m_bStartConsuming;
    std::vector<bool>       m_triggeredEvent;
    int                     m_nTriggerMode;
    int                     m_nTriggerSource;
    std::vector<bool>       m_imageOk;
    threadVector            m_grabThreads;
    threadVector            m_consumeThreads;
    thread                  m_triggerThread;
    threadVector            m_openDevicesThread;
    threadVector            m_resetTimestampThreads;


    byteArrayVector         m_pSaveImageBuf;
    byteArrayVector         m_pDataForSaveImage;
    std::vector<uint>       m_nSaveImageBufSize;
    frameVector             m_stImageInfo;
    mvccIntVector           m_params;
    ImageBuffer             &m_buf;
   // byteArrayVector       m_images;
    std::mutex              m_grabMutex;
    std::mutex              m_ioMutex;
    std::mutex              m_triggerMutex;
    std::vector<std::mutex> m_consumeMutexes;
    condVector              m_dataReadyCon;
    std::condition_variable m_triggerCon;

    int                     m_nZoomInIndex;  

    std::map<int, std::string> m_mapSerials; 
    std::map<int, std::string> m_mapModels;               

public:
    hikCamVector             m_pcMyCamera;          
   

public:
    
    void EnumDevices();
    void OpenDevices();
    void StartGrabbing();
    void CloseDevices();
    void StopGrabbing();
    void SetHeightWidth(int height, int width);
    void SetTriggerModeOnOff(int triggerMode);
    void SetTriggerSoftwareMode();
    void SetSoftwareOnce();
    void SaveImages();
    void SaveToBuffer();
    void JoinOpenThreads();
    void JoinThreadsTimeStampControlReset();
    void TimeStampControlReset();
    void OpenThreadsTimeStampControlReset();

    int ThreadGrabWithGetImageBufferFun(int nCurCameraIndex);
    int ThreadGrabWithGetOneFrameFun(int nCurCameraIndex);
    int ThreadConsumeFun(int nCurCameraIndex);
    int ThreadTriggerFun();
    int ThreadTriggerWithMutexFun();
    int ThreadTimeStampControlResetFun(int nCurCameraIndex);

private:
   
     void DoSoftwareOnce();
     void SetTriggerMode(void);
     void SetTriggerSource(void);
 
};
