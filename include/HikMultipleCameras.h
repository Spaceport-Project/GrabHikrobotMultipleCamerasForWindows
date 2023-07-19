// header file

#pragma once
#include <stdio.h>
#include <thread>
#include <condition_variable>
#include <memory>
#include <map>
#include "HikCamera.h"

//#include "ImageBuffer.h"



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

	HikMultipleCameras(std::chrono::system_clock::time_point timePoint);	      
   
private:
    MV_CC_DEVICE_INFO_LIST  m_stDevList;
    MV_ACTION_CMD_INFO      m_actionCMDInfo;
    MV_ACTION_CMD_RESULT_LIST m_actionCMDResList;
    uint                    m_nDeviceNum;
    uint                    m_nDeviceKey ;
    uint                    n_nGroupKey ;
    uint                    m_nGroupMask;
    std::string             m_sTriggerSource;
    bool                    m_bOpenDevice;
    bool                    m_bStartGrabbing;
    bool                    m_bStartConsuming;
    bool                    m_entered;
    std::chrono::system_clock::time_point m_timePoint;
    int                     m_nTriggerMode;
    int                     m_nTriggerSource;
    int                     m_nTriggerTimeInterval;
    std::vector<bool>       m_bImagesOk;
    threadVector            m_tGrabThreads;
    threadVector            m_tConsumeThreads;
    thread                  m_tTriggerThread;
    threadVector            m_tOpenDevicesThread;
    threadVector            m_tResetTimestampThreads;


    byteArrayVector         m_pSaveImagesBuf;
    byteArrayVector         m_pDataForSaveImages;
    std::vector<uint>       m_nSaveImagesBufSize;
    frameVector             m_stImagesInfo;
    mvccIntVector           m_params;
    //ImageBuffer             &m_buf;
    std::mutex              m_mGrabMutex;
    std::vector<std::mutex> m_mConsumeMutexes;
    condVector              m_cDataReadyCon;
   
    int                     m_nZoomInIndex;  

    std::map<int, std::string> m_mapSerials; 
    std::map<int, std::string> m_mapModels; 
                  

public:
    hikCamVector             m_pcMyCamera;          
   

public:
    
    void EnumDevices();
    void OpenDevices();
    int StartGrabbing();
    void CloseDevices();
    int StopGrabbing();
    int SetTriggerModeOnOff(int triggerMode);
    int ConfigureCameraSettings();
    
    int SaveImages();
    void SaveToBuffer();
    void JoinOpenThreads();
    void JoinThreadsTimeStampControlReset();
    void TimeStampControlReset();
    void OpenThreadsTimeStampControlReset();

    int ThreadGrabWithGetImageBufferFun(int nCurCameraIndex);
    int ThreadGrabWithGetOneFrameFun(int nCurCameraIndex);
    int ThreadConsumeFun(int nCurCameraIndex);
    int ThreadSoftwareTriggerFun();
    int ThreadTriggerGigActionCommandFun();
    int ThreadTimeStampControlResetFun(int nCurCameraIndex);


private:
   
     int  SetTriggerMode(void);
     int SetTriggerSoftwareMode();
     int SetTriggerGigEAction();
 
};
