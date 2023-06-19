// header file

#pragma once
#include <stdio.h>
#include <thread>
#include <memory>
#include <map>
#include "HikCamera.h"
#include "ImageBuffer.h"

#define MAX_DEVICE_NUM          5


class HikMultipleCameras 
{
// Construction
public:
	HikMultipleCameras(ImageBuffer &buf);	      
    static bool m_bExit;
private:
    MV_CC_DEVICE_INFO_LIST m_stDevList;
    unsigned int    m_nValidCamNum;
    bool            m_bOpenDevice;
    bool            m_bStartGrabbing;
    bool            m_bStartConsuming;
    int             m_nTriggerMode;
    int             m_nTriggerSource;
    bool            m_imageOk[MAX_DEVICE_NUM];
    std::thread* m_hGrabThread[MAX_DEVICE_NUM];
    std::thread* m_hConsumeThread[MAX_DEVICE_NUM];


    unsigned char*          m_pSaveImageBuf[MAX_DEVICE_NUM];
    unsigned char *         m_pDataForSaveImage[MAX_DEVICE_NUM] ;
    unsigned int            m_nSaveImageBufSize[MAX_DEVICE_NUM];
    MV_FRAME_OUT_INFO_EX    m_stImageInfo[MAX_DEVICE_NUM];
    ImageBuffer             &m_buf;
    std::vector<std::vector<uint8_t>> m_images;
    std::mutex              m_grabMutex;
    std::mutex              m_consumeMutexes[MAX_DEVICE_NUM];
    std::condition_variable m_dataReadyCon[MAX_DEVICE_NUM];
    std::shared_ptr<std::thread> m_grabThread;

    int             m_nZoomInIndex;  

    std::map<std::string, int> m_mapSerials  ;                  

public:
    HikCamera*      m_pcMyCamera[MAX_DEVICE_NUM];          
   

public:
    
    void EnumDevices();
    void OpenDevices();
    void StartGrabbing();
    void CloseDevices();
    void StopGrabbing();
    void SetTriggerModeOnOff(int triggerMode);
    void SetTriggerSoftwareMode();
    void SetSoftwareOnce();
    void SaveImages();
    void SaveToBuffer();

    int ThreadGrabFun(int nCurCameraIndex);
    int ThreadConsumeFun(int nCurCameraIndex);

private:
   
     void DoSoftwareOnce();
     void SetTriggerMode(void);
     void SetTriggerSource(void);
    

};
