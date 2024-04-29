// header file

#pragma once
#include <stdio.h>
#include <thread>
#include <condition_variable>
#include <memory>
#include <cstdint>
#include <map>
#include <cstdlib>
#include "HikCamera.h"
// #include "Bayer2H264Converter.h"
#include "Bayer2H264Converter2.h"


struct FrameFeatures {
      enum MvGvspPixelType framePixelType;
      unsigned int frameWidth;
      unsigned int frameHeight;
      unsigned int frameLen;
      unsigned int frameNum;
      char serialNum[128];
} ;
// #include "HikCamera.h"
#include "ImageBuffer.h"
#include "Container.h"
#include "SafeVector.h"
#include "Barrier.h"




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
    // using converterVector = std::vector<std::unique_ptr<BayerToH264Converter>>;
    static bool m_bExit;

	HikMultipleCameras(ImageBuffer<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>> &,  ImageBuffer<std::vector<AVPacket*>> &, std::chrono::system_clock::time_point, const std::string&);	      
   
private:
    MV_CC_DEVICE_INFO_LIST  m_stDevList;
    MV_CC_DEVICE_INFO_LIST   m_stDevListCorr;
    MV_ACTION_CMD_INFO      m_actionCMDInfo;
    MV_ACTION_CMD_RESULT_LIST m_actionCMDResList;
    unsigned int            m_nDeviceNum;
    unsigned int            m_nDeviceNumDouble;
    unsigned int            m_nDeviceKey ;
    unsigned int            n_nGroupKey ;
    unsigned int            m_nGroupMask;
    unsigned int            m_nWriteThreads;
    std::string             m_pBroadcastAddress;
    std::string             m_sTriggerSource;
    const std::string&      m_sCameraSettingsFile;
    bool                    m_bOpenDevice;
    bool                    m_bStartGrabbing;
    bool                    m_bStartConsuming;
    std::vector<bool>       m_bDataReady;
    std::vector<bool>       bDataReady;
    bool                    m_entered;
    std::chrono::system_clock::time_point m_timePoint;
    unsigned int            m_nTriggerMode;
    unsigned int            m_nTriggerSource;
    unsigned int            m_nTriggerTimeInterval;
    unsigned int            m_it = 0;
    int                     m_iRandomNumber;
    SafeVector<unsigned int> m_cnt;
    //std::vector<atomwrapper<int>> m_cnt;
    //std::vector<std::unique_ptr<std::atomic<int>>> m_cnt;
    std::vector<unsigned int> m_its;
    std::vector<bool>       m_bImagesOk;
    std::vector<bool>       m_bImagesCheck;
    std::vector<bool>       m_bImagesReady;
    threadVector            m_tGrabThreads;
    threadVector            m_tConsumeThreads;
    thread                  m_tTriggerThread;
    thread                  m_tCheckBuffThread;
    thread                  m_tCheck4H264Thread;
    thread                  m_tWrite2MP4Thread;
    threadVector            m_tSaveBufThreads;
    thread                  m_tSaveDiskThread;
    threadVector            m_tOpenDevicesThreads;
    threadVector            m_tCloseDevicesThreads;
    threadVector            m_tResetTimestampThreads;
    threadVector            m_tWriteMP4Threads;
    threadVector            m_tSaveAsMP4Threads;
    threadVector            m_tWrite2DiskThreads;
    threadVector            m_tReadMp4WriteThreads;
    threadVector            m_tStopGrabbingThreads;
    // converterVector         converters;
    std::unique_ptr<BayerToH264Converter2> converter;

    
    Container               m_Container;
    std::vector<Container>  m_Containers;
    my_barrier              barrier0;
    my_barrier              barrier1;
    my_barrier              barrier2;
    // Barrier                 barr1;
    //boost::barrier          barrboost;
    byteArrayVector         m_pSaveImagesBuf;
    byteArrayVector         m_pDataForSaveImages;
    std::vector<unsigned int> m_nSaveImagesBufSize;
    frameVector             m_stImagesInfo;
    mvccIntVector           m_params;
    SafeVector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> m_pairImagesInfo_Buff;
    SafeVector<AVPacket *> m_vectorAvPacketBuff;

    // SafeVector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> m_pairImagesInfo_Buff_Prev;
    // SafeVector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> m_pairImagesInfo_Buff_New;

    //ImageBuffer<SafeVector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>> &m_buf;
    ImageBuffer<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>> &m_buf;
    ImageBuffer<std::vector<AVPacket* >> &m_h264Buff;
    std::unique_ptr<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>> m_buffItem;
    std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> buff_item;

   // std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> m_pairImagesInfo_Buff;
    //std::vector<std::pair<std::shared_ptr<std::atomic<MV_FRAME_OUT_INFO_EX>>, std::shared_ptr<std::atomic<uint8_t[]>> >> m_pairImagesInfo_Buff;
    std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> m_currentPairImagesInfo_Buff;
    std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >> m_currentPairImagesInfo_Buff_Prev;

    std::mutex              m_mGrabMutex;
    std::mutex              m_mWriteMp4Mutex;
    std::mutex              m_mSaveMutex;
    std::mutex              m_mOpenDevMutex;
    std::mutex              m_mIoMutex;
    std::mutex              m_mMp4WriteMutex;
   // std::vector<std::mutex> m_mProduceMutexes;
    std::vector<std::mutex> m_mProduceMutexes;
    std::vector<std::mutex> m_mCheckMutexes;
    std::vector<std::mutex> codecMutexes_;

    condVector              m_cDataReadyCon;
    std::condition_variable m_cdataCheckCon;
    std::condition_variable m_cDataReadySingleCon1;
    std::condition_variable m_cDataReadyWriMP4Con;
    std::condition_variable m_cDataReadySingleCon2;

    std::map<int, std::string> m_mapSerials; 
    std::map<int, std::string> m_mapModels; 
    unsigned int                barr_cnt =0;
    std::atomic<int>            counter_at{0};
  
                  

public:
    hikCamVector             m_pcMyCameras;          
   

public:
    
    void EnumDevices();
    void EnumDevicesAvoidDublication();
    void EnumDevicesByIPAddress();
    void OpenDevices();
    int StartGrabbing();
    void CloseDevices();
    int StopGrabbing();
    int SetTriggerModeOnOff(int );
    int ConfigureCameraSettings();
    
    int SaveImages2Disk();
    int  Save2BufferThenDisk();
    int ReadMp4Write2DiskAsJpgInThreads();
    int JoinReadMp4Write2DiskAsJpgInThreads();
    void JoinOpenDevicesInThreads();
    void OpenDevicesInThreads();
    void JoinCloseDevicesInThreads();
    void CloseDevicesInThreads();
    void JoinThreadsTimeStampControlReset();
    void OpenThreadsTimeStampControlReset();
    void TimeStampControlReset();

    int ThreadCheck4H264Fun( );
    int ThreadWrite2MP4Fun2();
    int ThreadWrite2DiskFunEx2();

    int ThreadSave2BufferFun(int );
    int ThreadSave2DiskFun();
    int ThreadCheckBufferFun();
    int ThreadGrabWithGetImageBufferFun(int );
    int ThreadGrabWithGetImageBufferFun2(int );
    int ThreadGrabWithGetOneFrameFun(int );
    int ThreadConsumeFun(int );
    int ThreadSoftwareTriggerFun();
    int ThreadTriggerGigActionCommandFun();
    int ThreadTimeStampControlResetFun(int );
    int ThreadOpenDevicesFun(int );
    int ThreadCloseDevicesFun(int );
    int ThreadReadMp4Write2DiskAsJpgFun(int );
    int ThreadWrite2DiskFun(ImageBuffer<std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]> >>> &, int  );
    int ThreadWrite2DiskFun2( );

private:
    int SetTriggerMode(void);
    int SetTriggerSoftwareMode();
    int SetTriggerGigEAction();
    void Write2Disk(const std::vector<std::pair<MV_FRAME_OUT_INFO_EX, std::shared_ptr<uint8_t[]>>>&);
    void Write2MP4();

    void Write2MP4FromBayer( int nCurrCamera);
    void Write2H264FromBayer2( int nCurrCamera);
    void Write2H264FromBayer3(int numWriteThreads);

    void Write2H264FromBayer4(int nCurrWriteThread);
};
