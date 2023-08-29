// header file

#pragma once
#include <stdio.h>
#include <thread>
#include <condition_variable>
#include <memory>
#include <cstdint>
#include <map>
#include "HikCamera.h"
#include "ImageBuffer.h"
#include "Container.h"



class HikMultipleCameras 
{
// Construction
public:
    using thread = std::unique_ptr<std::thread>;
    using threadVector = std::vector<std::unique_ptr<std::thread>>;
    using timePoint = std::chrono::system_clock::time_point;
    using byteArrayVector = std::vector<std::shared_ptr<uint8_t[]>> ;
    using frameVector = std::vector<MV_FRAME_OUT_INFO_EX>;
    using mvccIntVector = std::vector<MVCC_INTVALUE>;
    using condVector = std::vector<std::condition_variable>;
    using hikCamVector = std::vector<std::unique_ptr<HikCamera>>;
    static bool m_bExit;

	HikMultipleCameras(ImageBuffer<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>> &, std::chrono::system_clock::time_point, const std::string&);	      
   
private:
    MV_CC_DEVICE_INFO_LIST  m_stDevList;
    MV_ACTION_CMD_INFO      m_actionCMDInfo;
    MV_ACTION_CMD_RESULT_LIST m_actionCMDResList;
    unsigned int            m_nDeviceNum;
    unsigned int            m_nDeviceKey ;
    unsigned int            n_nGroupKey ;
    unsigned int            m_nGroupMask;
    std::string             m_pBroadcastAddress;
    std::string             m_sTriggerSource;
    const std::string&      m_sCameraSettingsFile;
    bool                    m_bOpenDevice;
    bool                    m_bStartGrabbing;
    bool                    m_bStartConsuming;
    bool                    m_entered;
    std::chrono::system_clock::time_point m_timePoint;
    unsigned int            m_nTriggerMode;
    unsigned int            m_nTriggerSource;
    unsigned int            m_nTriggerTimeInterval;
    unsigned int            m_it = 0;
    std::vector<unsigned int> m_its;
    std::vector<bool>       m_bImagesOk;
    std::vector<bool>       m_bImagesCheck;
    std::vector<bool>       m_bImagesReady;
    threadVector            m_tGrabThreads;
    threadVector            m_tConsumeThreads;
    thread                  m_tTriggerThread;
    thread                  m_tCheckBuffThread;
    threadVector            m_tSaveBufThreads;
    thread                  m_tSaveDiskThread;
    threadVector            m_tOpenDevicesThreads;
    threadVector            m_tCloseDevicesThreads;
    threadVector            m_tResetTimestampThreads;
    threadVector            m_tWriteMP4Threads;
    threadVector            m_tReadMp4Threads;
    Container               m_Container;
    std::vector<Container>  m_Containers;

    byteArrayVector         m_pSaveImagesBuf;
    byteArrayVector         m_pDataForSaveImages;
    std::vector<unsigned int> m_nSaveImagesBufSize;
    frameVector             m_stImagesInfo;
    mvccIntVector           m_params;
    ImageBuffer<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>> &m_buf;
    std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> m_pairImagesInfo_Buff;
    const std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> *m_currentPairImagesInfo_Buff;
    std::mutex              m_mGrabMutex;
    std::mutex              m_mSaveMutex;
    std::mutex              m_mOpenDevMutex;
   // std::vector<std::mutex> m_mProduceMutexes;
    std::vector<std::mutex> m_mProduceMutexes;
    condVector              m_cDataReadyCon;
    std::condition_variable m_cDataReadySingleCon1;
    std::condition_variable m_cDataReadySingleCon2;

    std::map<int, std::string> m_mapSerials; 
    std::map<int, std::string> m_mapModels; 
                  

public:
    hikCamVector             m_pcMyCameras;          
   

public:
    
    void EnumDevices();
    void EnumDevicesByIPAddress();
    void OpenDevices();
    int StartGrabbing();
    void CloseDevices();
    int StopGrabbing();
    int SetTriggerModeOnOff(int triggerMode);
    int ConfigureCameraSettings();
    
    int SaveImages2Disk();
    int  Save2BufferThenDisk();
    void JoinOpenDevicesInThreads();
    void OpenDevicesInThreads();
    void JoinCloseDevicesInThreads();
    void CloseDevicesInThreads();
    void JoinThreadsTimeStampControlReset();
    void OpenThreadsTimeStampControlReset();
    void TimeStampControlReset();
    
    int ThreadWrite2MP4Fun(int nCurCameraIndex);
    int ThreadSave2BufferFun(int nCurCameraIndex);
    int ThreadSave2DiskFun();
    int ThreadCheckBufferFun();
    int ThreadGrabWithGetImageBufferFun(int nCurCameraIndex);
    int ThreadGrabWithGetImageBufferFun2(int nCurCameraIndex);
    int ThreadGrabWithGetOneFrameFun(int nCurCameraIndex);
    int ThreadConsumeFun(int nCurCameraIndex);
    int ThreadSoftwareTriggerFun();
    int ThreadTriggerGigActionCommandFun();
    int ThreadTimeStampControlResetFun(int nCurCameraIndex);
    int ThreadOpenDevicesFun(int nCurCameraIndex);
    int ThreadCloseDevicesFun(int nCurCameraIndex);
    int ThreadReadMp4Fun(int nCurCameraIndex);

private:
   
    int SetTriggerMode(void);
    int SetTriggerSoftwareMode();
    int SetTriggerGigEAction();
    void Write2Disk(const std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]>>>&);
    void Write2MP4(const std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]>>>&);

};
