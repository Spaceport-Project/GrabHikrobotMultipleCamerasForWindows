
// MultipleCameraDlg.cpp : implementation file
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <functional>   
#include "HikMultipleCameras.h"


#define FPS_CALC(_WHAT_) \
do \
{ \
    static unsigned count = 0;\
    static double last = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();\
    double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); \
    ++count; \
    if (now - last >= 2.0) \
    { \
      std::cerr << "\033[1;31m";\
      std::cerr << "Average framerate("<< _WHAT_ << "): " << double(count)/double(now - last) << " fbs." <<  "\n"; \
      std::cerr << "\033[0m";\
      count = 0; \
      last = now; \
    } \
}while(false)


// HikMultipleCameras dialog
HikMultipleCameras::HikMultipleCameras(ImageBuffer &buf):
    m_buf(buf)
    , m_nValidCamNum(0)
    , m_bOpenDevice(false)
    , m_bStartGrabbing(false)
    , m_bStartConsuming(false)
    , m_nTriggerMode(MV_TRIGGER_MODE_OFF)
   
{
	
    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    for (int i = 0; i < MAX_DEVICE_NUM; i++)
    {
        m_imageOk[i] = false;
        m_pcMyCamera[i] = NULL;
        m_hGrabThread[i] = NULL;
        m_hConsumeThread[i] = NULL;
        m_pSaveImageBuf[i] = NULL;
        m_nSaveImageBufSize[i] = 0;
        memset(&(m_stImageInfo[i]), 0, sizeof(MV_FRAME_OUT_INFO_EX));
    }
}


//  Set trigger mode
void HikMultipleCameras::SetTriggerMode(void)
{
    int nRet = MV_OK;
    for (unsigned int i = 0; i < MAX_DEVICE_NUM; i++)
    {
        if (m_pcMyCamera[i])
        {
            std::cout<<"Trigger Mode nRet:"<<m_nTriggerMode<<std::endl;
            nRet = m_pcMyCamera[i]->SetEnumValue("TriggerMode", m_nTriggerMode);
            
            if (MV_OK != nRet)
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
    for (unsigned int i = 0; i < MAX_DEVICE_NUM; i++)
    {
        if (m_pcMyCamera[i])
        {
            nRet = m_pcMyCamera[i]->SetEnumValue("TriggerSource", m_nTriggerSource);
            if (MV_OK != nRet)
            {
                printf("Set Trigger source fail! DevIndex[%d], TriggerSource[%d], nRet[%#x]\r\n", i+1, m_nTriggerSource, nRet);
            }
            float triggerRate = 5.0f;
            float triggerInterval = 1.0f / triggerRate; 
            float triggerDelay = 100.0f;
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
    for (unsigned int i = 0; i < MAX_DEVICE_NUM; i++)
    {
        if (m_pcMyCamera[i])
        {
            nRet = m_pcMyCamera[i]->CommandExecute("TriggerSoftware");
            if (MV_OK != nRet)
            {
                printf("Soft trigger fail! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            }
        }
    }
}



int HikMultipleCameras::ThreadConsumeFun(int nCurCameraIndex)
{
    if (m_pcMyCamera[nCurCameraIndex])
    {
        MV_SAVE_IMAGE_PARAM_EX stParam;
        memset(&stParam, 0, sizeof(MV_SAVE_IMAGE_PARAM_EX));
        while(m_bStartConsuming)
        {
            if  (m_pSaveImageBuf[nCurCameraIndex]==NULL) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            m_pDataForSaveImage[nCurCameraIndex] = (unsigned char*)malloc(m_stImageInfo[nCurCameraIndex].nWidth * m_stImageInfo[nCurCameraIndex].nHeight * 4 + 2048);

            if (NULL ==  m_pDataForSaveImage[nCurCameraIndex])
            {
                break;
            }
           

                stParam.enImageType = MV_Image_Jpeg; // en:Image format to save
                stParam.enPixelType =  m_stImageInfo[nCurCameraIndex].enPixelType; 
                stParam.nWidth = m_stImageInfo[nCurCameraIndex].nWidth;       
                stParam.nHeight = m_stImageInfo[nCurCameraIndex].nHeight;       
                stParam.nDataLen = m_stImageInfo[nCurCameraIndex].nFrameLen;
               
                stParam.pImageBuffer =  m_pDataForSaveImage[nCurCameraIndex];
                stParam.nBufferSize = m_stImageInfo[nCurCameraIndex].nWidth * m_stImageInfo[nCurCameraIndex].nHeight * 4 + 2048;;  
                stParam.nJpgQuality = 80;  
                
             {
             
                std::unique_lock<std::mutex> lk(m_consumeMutexes[nCurCameraIndex]);

                m_dataReadyCon[nCurCameraIndex].wait(lk, [this, nCurCameraIndex] {
                    return m_imageOk[nCurCameraIndex];

                });

                m_imageOk[nCurCameraIndex] = false;
                stParam.pData = m_pSaveImageBuf[nCurCameraIndex];
                
                int nRet =  m_pcMyCamera[nCurCameraIndex]->SaveImage(&stParam);

                if(MV_OK != nRet)
                {
                    printf("failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;;
                }
                char filepath[256];
                // unsigned int timestampHigh = m_stImageInfo[nCurCameraIndex].nDevTimeStampHigh ;
                // unsigned int timestampLow = m_stImageInfo[nCurCameraIndex].nDevTimeStampLow ;
                // unsigned long  long timestamp = ((unsigned long long)m_stImageInfo[nCurCameraIndex].nDevTimeStampHigh << 32) + (unsigned long long)m_stImageInfo[nCurCameraIndex].nDevTimeStampLow;
                uint64_t timestamp = (((uint64_t)m_stImageInfo[nCurCameraIndex].nDevTimeStampHigh) << 32) + m_stImageInfo[nCurCameraIndex].nDevTimeStampLow;
                printf("nFrame:[%d], Timestamp:[%ld]\n", m_stImageInfo[nCurCameraIndex].nFrameNum, timestamp);

                sprintf(filepath, "Image_%d_w%d_h%d_fn%03d.jpg", nCurCameraIndex, stParam.nWidth, stParam.nHeight, m_stImageInfo[nCurCameraIndex].nFrameNum);
                FILE* fp = fopen(filepath, "wb");
                if (NULL == fp)
                {
                    printf("fopen failed\n");
                    break;
                }
                fwrite(m_pDataForSaveImage[nCurCameraIndex], 1, stParam.nImageLen, fp);
                fclose(fp);
                printf("save image succeed, frame number=%d\n", m_stImageInfo[nCurCameraIndex].nFrameNum);    
                
            }
            FPS_CALC ("Image Saving FPS:");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            if (m_bExit) m_bStartConsuming=false;

        }
    }
    return MV_OK;
}

int HikMultipleCameras::ThreadGrabFun(int nCurCameraIndex)
{
    if (m_pcMyCamera[nCurCameraIndex])
    {
        MV_FRAME_OUT stImageOut = {0};
        memset(&stImageOut, 0, sizeof(MV_FRAME_OUT));
        while(m_bStartGrabbing)
        {
            //std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

            if (m_nTriggerMode == MV_TRIGGER_MODE_ON) m_pcMyCamera[nCurCameraIndex]->CommandExecute("TriggerSoftware");
            //std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            //std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;

            int nRet = m_pcMyCamera[nCurCameraIndex]->GetImageBuffer(&stImageOut, 1000);
            if (nRet == MV_OK)
            {
                
                
                {
                    std::lock_guard<std::mutex> lk(m_consumeMutexes[nCurCameraIndex]);
                    
                    printf("Get One Frame: Width[%d], Height[%d], nFrameNum[%d]\n",
                    stImageOut.stFrameInfo.nWidth, stImageOut.stFrameInfo.nHeight, stImageOut.stFrameInfo.nFrameNum);

                    if (NULL == m_pSaveImageBuf[nCurCameraIndex] || stImageOut.stFrameInfo.nFrameLen > m_nSaveImageBufSize[nCurCameraIndex])
                    {
                        if (m_pSaveImageBuf[nCurCameraIndex])
                        {
                            free(m_pSaveImageBuf[nCurCameraIndex]);
                            m_pSaveImageBuf[nCurCameraIndex] = NULL;
                        }

                        m_pSaveImageBuf[nCurCameraIndex] = (unsigned char *)malloc(sizeof(unsigned char) * stImageOut.stFrameInfo.nFrameLen);
                        if (m_pSaveImageBuf[nCurCameraIndex] == NULL)
                        {
                            return 0;
                        }
                        m_nSaveImageBufSize[nCurCameraIndex] = stImageOut.stFrameInfo.nFrameLen;
                    }


                    if (NULL != stImageOut.pBufAddr)
                    {
                        
                    // m_images.at(nCurCameraIndex).resize(stImageOut.stFrameInfo.nFrameLen);
                    // memcpy(m_images.at(nCurCameraIndex).data(), stImageOut.pBufAddr, stImageOut.stFrameInfo.nFrameLen);
                            memcpy(m_pSaveImageBuf[nCurCameraIndex], stImageOut.pBufAddr, stImageOut.stFrameInfo.nFrameLen);
                            memcpy(&(m_stImageInfo[nCurCameraIndex]), &(stImageOut.stFrameInfo), sizeof(MV_FRAME_OUT_INFO_EX));
                            unsigned int timestampHigh = m_stImageInfo[nCurCameraIndex].nDevTimeStampHigh ;
                            unsigned int timestampLow = m_stImageInfo[nCurCameraIndex].nDevTimeStampLow ;
                            //unsigned long  long timestamp = ((unsigned long long)stImageOut.stFrameInfo.nDevTimeStampHigh << 32) | (unsigned long long)stImageOut.stFrameInfo.nDevTimeStampLow;
                        //     uint64_t timestamp = (((uint64_t)m_stImageInfo[nCurCameraIndex].nDevTimeStampHigh) << 32) + m_stImageInfo[nCurCameraIndex].nDevTimeStampLow;

                        //     //  int64_t nTimestamp = pEventInfo->nTimestampHigh;
                        //     //  nTimestamp = (nTimestamp << 32) + pEventInfo->nTimestampLow;
                        //    //  std::cout<<"timetamp:"<<timestampHigh<<" " <<timestampLow<<" "<<timestamp<<std::endl;
                        //     printf("Timestamp[%ld]\n", timestamp);
                        
                            m_imageOk[nCurCameraIndex] = true;
                            nRet = m_pcMyCamera[nCurCameraIndex]->FreeImageBuffer(&stImageOut);
                            if (MV_OK != nRet)
                            {
                                continue;
                            }
                        
                        


                    }
                }
                m_dataReadyCon[nCurCameraIndex].notify_one();
                FPS_CALC ("Image Grabbing FPS:");

                if (m_bExit) m_bStartGrabbing=false;

				std::this_thread::sleep_for(std::chrono::milliseconds(2));

             
            }
            else
            {
                if (MV_TRIGGER_MODE_ON ==  m_nTriggerMode)
                {
                   std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
				continue;
            }
        }
    }

    return MV_OK;
}

void HikMultipleCameras::EnumDevices()
{
    memset(&m_stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    int nRet = HikCamera::EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &m_stDevList);
    if (MV_OK != nRet || m_stDevList.nDeviceNum == 0)
    {
        printf("Find no device!\r\n");
        return;
    }
    printf("Find %d devices!\r\n", m_stDevList.nDeviceNum);

   // m_images.resize( m_stDevList.nDeviceNum);

    m_nValidCamNum = 0;

    for (unsigned int i = 0; i < MAX_DEVICE_NUM; i++)
    {
        // char chDeviceName[256] = {0};
        if (i < m_stDevList.nDeviceNum)
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
                printf("UserDefinedName: %s\n\n" , pDeviceInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
                printf("SerialNumber: %s\n", pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber);
                m_mapSerials.insert(std::make_pair((char *)pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber,i));
                
               
            }
            else if (pDeviceInfo->nTLayerType == MV_USB_DEVICE)
            {
                
                printf("Device Model Name: %s\n", pDeviceInfo->SpecialInfo.stUsb3VInfo.chModelName);
                printf("UserDefinedName: %s\n\n", pDeviceInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
                m_mapSerials.insert(std::make_pair((char *)pDeviceInfo->SpecialInfo.stUsb3VInfo.chSerialNumber,i));
            }
            else {
                printf("Not support.\n");
            }
            m_nValidCamNum++;
        }
       
        
       
    }
}

//  Initialzation, include opening device
void HikMultipleCameras::OpenDevices()
{
    if (true == m_bOpenDevice || m_nValidCamNum == 0)
    {
        return;
    }

    
    for (unsigned int i = 0; i < MAX_DEVICE_NUM; i++)
    {
        
        if (NULL == m_pcMyCamera[i])
        {
            m_pcMyCamera[i] = new HikCamera;
        }

        int nRet = m_pcMyCamera[i]->Open(m_stDevList.pDeviceInfo[i]);
        if (MV_OK != nRet)
        {
            delete(m_pcMyCamera[i]);
            m_pcMyCamera[i] = NULL;

            printf("Open device failed! DevIndex[%d], nRet[%#x]\r\n", i+1, nRet);
            continue;
        }
        else
        {
            // Detect the optimal packet size (it is valid for GigE cameras only)
            m_bOpenDevice = true;
            if (m_stDevList.pDeviceInfo[i]->nTLayerType == MV_GIGE_DEVICE)
            {
                unsigned int nPacketSize = 0;
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
        }
        
    }


   
}

// Close, include destroy handle
void HikMultipleCameras::CloseDevices()
{
	StopGrabbing();

	int nRet = MV_OK;
    for (unsigned int i = 0; i < MAX_DEVICE_NUM; i++)
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

            delete(m_pcMyCamera[i]);
            m_pcMyCamera[i] = NULL;
        }

		if (m_pSaveImageBuf[i])
		{
			free(m_pSaveImageBuf[i]);
			m_pSaveImageBuf[i] = NULL;
		}
		m_nSaveImageBufSize[i] = 0;

        

    }

    
}

void HikMultipleCameras::SaveToBuffer(){
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
    if (true == m_bStartGrabbing)
    {        
        return;
    }

    int nRet = MV_OK;

    for (int i = 0; i < MAX_DEVICE_NUM; i++)
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

            // std::this_thread::sleep_for(std::chrono::milliseconds(2));
            m_hGrabThread[i] = new std::thread(std::bind(&HikMultipleCameras::ThreadGrabFun, this, i));
           
            if (m_hGrabThread[i]==NULL)
            {
                printf("Create grab thread fail! DevIndex[%d]\r\n", i+1);
            }
        }
    }

// Consider including the line below incase of saving in buffer.
//m_grabThread.reset (new std::thread(std::bind(&HikMultipleCameras::SaveToBuffer, this)));

}



// Stop grabbing
void HikMultipleCameras::StopGrabbing()
{
    
    if (false == m_bOpenDevice || false == m_bStartGrabbing)
    {        
        return;
    }


    int nRet = MV_OK;
    for (unsigned int i = 0; i < MAX_DEVICE_NUM; i++)
    {
        if (m_pcMyCamera[i])
        {
            
            m_hGrabThread[i]->join();
            m_hConsumeThread[i]->join();
            delete m_hGrabThread[i];
            delete m_hConsumeThread[i];
            
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


void HikMultipleCameras::SetTriggerModeOnOff(int triggerMode)
{
    
   // m_nTriggerMode = MV_TRIGGER_MODE_ON;
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
    if (false == m_bStartGrabbing)
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

    for (int i = 0; i < MAX_DEVICE_NUM; i++)
    {
        if (m_pcMyCamera[i])
        {

            m_bStartConsuming = true;
            m_hConsumeThread[i] = new std::thread(std::bind(&HikMultipleCameras::ThreadConsumeFun, this, i));

          
        }
    }


}   

