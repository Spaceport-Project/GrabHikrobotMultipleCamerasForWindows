// header file

#pragma once
#include <stdio.h>
#include <thread>
#include <memory>
#include <map>
#include "HikCamera.h"
#include "ImageBuffer.h"

#define MAX_DEVICE_NUM          3


class HikMultipleCameras 
{
// Construction
public:
	HikMultipleCameras(ImageBuffer &buf, std::chrono::system_clock::time_point postime);	      
    static bool m_bExit;
private:
    MV_CC_DEVICE_INFO_LIST m_stDevList;
    unsigned int    m_nValidCamNum;
    bool            m_bOpenDevice;
    std::chrono::system_clock::time_point m_timePoint;
    bool            m_bStartGrabbing;
    bool            m_bStartConsuming;
    bool            m_triggeredEvent[MAX_DEVICE_NUM];
    int             m_nTriggerMode;
    int             m_nTriggerSource;
    bool            m_imageOk[MAX_DEVICE_NUM];
    std::thread*    m_hGrabThread[MAX_DEVICE_NUM];
    std::thread*    m_hConsumeThread[MAX_DEVICE_NUM];
    std::thread*    m_triggerThread;
    std::thread*    m_openDevicesThread[MAX_DEVICE_NUM];
    std::thread*    m_resetTimestamp[MAX_DEVICE_NUM];
    std::thread*    m_grabThread;


    unsigned char*          m_pSaveImageBuf[MAX_DEVICE_NUM];
    unsigned char *         m_pDataForSaveImage[MAX_DEVICE_NUM] ;
    unsigned int            m_nSaveImageBufSize[MAX_DEVICE_NUM];
    MV_FRAME_OUT_INFO_EX    m_stImageInfo[MAX_DEVICE_NUM];
    MVCC_INTVALUE           m_params[MAX_DEVICE_NUM];
    ImageBuffer             &m_buf;
    std::vector<std::vector<uint8_t>> m_images;
    std::mutex              m_grabMutex;
    std::mutex              m_ioMutex;
    std::mutex              m_triggerMutex;
    std::mutex              m_consumeMutexes[MAX_DEVICE_NUM];
    std::condition_variable m_dataReadyCon[MAX_DEVICE_NUM];
    std::condition_variable m_triggerCon;

    int             m_nZoomInIndex;  

    std::map<int, std::string> m_mapSerials; 
    std::map<int, std::string> m_mapModels;               

public:
    HikCamera*      m_pcMyCamera[MAX_DEVICE_NUM];          
   

public:
    
    void EnumDevices();
    void EnumDevicesAndOpenInThreads();
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
    int ThreadOpenDevicesFun(int nCurCameraIndex);
    int ThreadTimeStampControlResetFun(int nCurCameraIndex);

private:
   
     void DoSoftwareOnce();
     void SetTriggerMode(void);
     void SetTriggerSource(void);
    

};
