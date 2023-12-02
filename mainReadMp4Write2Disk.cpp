#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <functional> 
#include <cmath>  
#include <memory>
#include <map>
#include <cstdint>
#include "HikCamera.h"
struct FrameFeatures {
      enum MvGvspPixelType framePixelType;
      unsigned int frameWidth;
      unsigned int frameHeight;
      unsigned int frameLen;
      unsigned int frameNum;
      char serialNum[128];
} ;
#include "Container.h"


unsigned int nDeviceNum;
MV_CC_DEVICE_INFO_LIST  stDevList;
MV_CC_DEVICE_INFO_LIST   stDevListCorr;
std::vector<std::unique_ptr<HikCamera>> pcMyCameras;
std::vector<std::unique_ptr<std::thread>> tOpenDevicesThreads;
std::vector<std::unique_ptr<std::thread>> tReadMp4WriteThreads;

std::map<int, std::string> mapSerials; 
std::vector<Container> Containers;

int readMp4FilesWrite2Disk(int i)
{
    int length;
    char *data;
    unsigned char * pDataForSaveImage = NULL;

    while (true) 
    {

        //m_Containers[nCurCameraIndex].setWriteMode(false);
        int streamIndex = Containers[i].read(data, length);
        // for (unsigned int i = 0 ; i < nDeviceNum ; i++)
        // {
        



        // }
        if (streamIndex == -1) 
        {
            printf("Cannot read data from Mp4 Or End of file, DevIndex[%d]\n", i);
            Containers[i].close();
            break;
        }
        FrameFeatures tmpFrameFeat;

        memcpy(&tmpFrameFeat, data, sizeof(FrameFeatures));
        //printf("TmpFrameFeatures Serial Number: %s\n", tmpFrameFeat.serialNum);
        char* newData = new  char[length - sizeof(FrameFeatures)];
        memcpy(newData, (char*)data +sizeof(FrameFeatures), length - sizeof(FrameFeatures) );
        if (i == 0) printf("Frame Num: %d\n", tmpFrameFeat.frameNum);
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
        int nRet =  pcMyCameras[i]->SaveImage(&stParam);

        if(nRet != MV_OK)
        {
            printf("Failed in MV_CC_SaveImage,nRet[%x]\n", nRet);
            // std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        char filepath[256];
        
        


        #ifdef _MSC_VER 
        sprintf_s(filepath, sizeof(filepath), "ts_%03d_%s_w%d_h%d.jpg", tmpFrameFeat.frameNum, mapSerials[i].c_str(), stParam.nWidth, stParam.nHeight );
       // sprintf_s(filepath, sizeof(filepath), "Image_%s_w%d_h%d_fn%03d.jpg", mapSerials[nCurCameraIndex].c_str(), stParam.nWidth, stParam.nHeight,  tmpFrameFeat.frameNum);
        FILE* fp;
        fopen_s(&fp, filepath, "wb");
        #else
        sprintf(filepath, "Image_%s_w%d_h%d_fn%03d.jpg", mapSerials[i].c_str(), stParam.nWidth, stParam.nHeight,  tmpFrameFeat.frameNum);
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
void EnumDevicesAvoidDublication()
{
   // memset(&stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    int nRet = HikCamera::EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDevList);
    if ( nRet != MV_OK || stDevList.nDeviceNum == 0)
    {
        printf("Find no device!\r\n");
        return;
    }
   // printf("Find %d devices!\r\n", stDevList.nDeviceNum);
    
    unsigned int nDeviceNumDouble = stDevList.nDeviceNum;
    std::vector<std::tuple<unsigned int, unsigned int, std::string>> vectorExportSerials1;
    
    unsigned int  l = 0;

    for (unsigned int i = 0; i < nDeviceNumDouble; i++) {
        MV_CC_DEVICE_INFO* pDeviceInfo = stDevList.pDeviceInfo[i];

       if (i < (unsigned int)std::ceil(nDeviceNumDouble/4.0f)) {

            stDevListCorr.pDeviceInfo[l] = new MV_CC_DEVICE_INFO(*pDeviceInfo);
            vectorExportSerials1.push_back(std::make_tuple (i, pDeviceInfo->SpecialInfo.stGigEInfo.nNetExport , (char *)pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber ));
            mapSerials.insert(std::make_pair(l , (char *)pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber));
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
                stDevListCorr.pDeviceInfo[l] = new MV_CC_DEVICE_INFO(*pDeviceInfo);
                mapSerials.insert(std::make_pair(l , (char *)pDeviceInfo->SpecialInfo.stGigEInfo.chSerialNumber));
                l++;
            }



       }
        
       
    }
    
 
    nDeviceNum = l;
    printf("Device Number: %d\n", l);
}
    
 std::vector<std::string> LoadMp4Files(const std::string &fileName){
 // Define the vector to store the file names
    std::vector<std::string> fileNames;

    // Specify the path to the file containing the list of file names
    std::string fileNameListPath = fileName;

    // Open the file
    std::ifstream file(fileNameListPath);

    // Check if the file is open
    if (!file.is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
        return std::vector<std::string>();
    }

    // Read file names line by line and add them to the vector
    std::string line;
    while (std::getline(file, line)) {
        // Remove any leading or trailing whitespace from the line
        line = line.substr(line.find_first_not_of(" \t\r\n"), line.find_last_not_of(" \t\r\n") + 1);

        // Add the file name to the vector
        fileNames.push_back(line);
    }

    // Close the file
    file.close();

    // Print the file names in the vector
    for (const std::string& fileName : fileNames) {
        std::cout << fileName << std::endl;
    }

    return fileNames;


}
void StartCamerasAndLoadMp4Files(const std::string &fileName){

    EnumDevicesAvoidDublication();
    auto fileNames = LoadMp4Files(fileName);
    if (fileNames.size() != nDeviceNum) 
    {
        printf("The number of mp4 files is not the same as number of devices! Exiting....\n");
        exit(0);
    }
    for (unsigned int i = 0 ; i < nDeviceNum ; i++)
    {
        pcMyCameras.push_back(std::make_unique<HikCamera>());
        printf("%d Camera serial number:%s\n", i, mapSerials[i].c_str());
        Containers.push_back(Container());
        //std::string fileNameTmp = "hikrobot_" + mapSerials[i] + "_" + std::to_string(sec.count()) + ".mp4";
        if (!Containers[i].openForRead((char*)fileNames[i].c_str()))
        {
            printf("Cannot open for read %s \n", fileNames[i].c_str());
        }
          

    }


}

//  Initialzation, include opening device
void OpenDevices()
{
  
    if ( nDeviceNum == 0)
    {
        printf("'nDeviceNum' set to 0! Exiting from OpenDevices... \n");
        return ;
    }
    
   
    
    for (unsigned int i = 0; i < nDeviceNum; i++)
    {
       

            int nIp1 = ((stDevListCorr.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
            int nIp2 = ((stDevListCorr.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
            int nIp3 = ((stDevListCorr.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
            int nIp4 = (stDevListCorr.pDeviceInfo[i]->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
            printf("%d Cam, CurrentIp: %d.%d.%d.%d\n" , i, nIp1, nIp2, nIp3, nIp4);

            int nRet = pcMyCameras[i]->Open(stDevListCorr.pDeviceInfo[i]);
            printf("%d. Camera, serial number: %s\n", i, (char*)stDevListCorr.pDeviceInfo[i]->SpecialInfo.stGigEInfo.chSerialNumber);

            if (nRet != MV_OK)
            {
                pcMyCameras[i].reset();
                printf("Open device failed! DevIndex[%d], nRet[%#x]\r\n", i, nRet);
                continue;
            }
            else
            {
                
                            
                // Detect the optimal packet size (it is valid for GigE cameras only)
                if (stDevListCorr.pDeviceInfo[i]->nTLayerType == MV_GIGE_DEVICE)
                {
                    unsigned int  nPacketSize = 0;
                    nRet = pcMyCameras[i]->GetOptimalPacketSize(&nPacketSize);
                    if (nPacketSize > 0)
                    {
                        nRet = pcMyCameras[i]->SetIntValue("GevSCPSPacketSize",nPacketSize);
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



int ThreadOpenDevicesFun(int nCurCameraIndex) 
{
    if ( nDeviceNum == 0)
    {
        printf("'nDeviceNum' set to 0! Exiting from OpenDevices... \n");
        return -1;
    }
   
   
    
    int nRet = pcMyCameras[nCurCameraIndex]->Open(stDevListCorr.pDeviceInfo[nCurCameraIndex]);
    if (nRet != MV_OK)
    {
        pcMyCameras[nCurCameraIndex].reset();
        printf("Open device failed! DevIndex[%d], nRet[%#x]\r\n", nCurCameraIndex , nRet);
        return -1;
    }
    else
    {
        
                    
        // Detect the optimal packet size (it is valid for GigE cameras only)
        if (stDevListCorr.pDeviceInfo[nCurCameraIndex]->nTLayerType == MV_GIGE_DEVICE)
        {
            unsigned int nPacketSize = 0;
            nRet = pcMyCameras[nCurCameraIndex]->GetOptimalPacketSize(&nPacketSize);
            if (nPacketSize > 0)
            {
                nRet = pcMyCameras[nCurCameraIndex]->SetIntValue("GevSCPSPacketSize",nPacketSize);
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
void OpenDevicesInThreads()
{

    for (unsigned int  i = 0; i < nDeviceNum; i++)
    {
        if (pcMyCameras[i])
            tOpenDevicesThreads.push_back(std::make_unique<std::thread>(ThreadOpenDevicesFun,  i));
       
    }

}

void JoinOpenDevicesInThreads() {

    for (unsigned int i = 0; i < nDeviceNum; i++)
    {
        if (pcMyCameras[i])
        {
            tOpenDevicesThreads[i]->join();
        }
    }


}


int main(int argc, char *argv[]) {

     if (argc != 2) {
        printf("Usage: %s <path/to/FileNames.txt>\n", argv[0]);
        return -1;
    } 
    memset(&stDevList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    memset(&stDevListCorr, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    StartCamerasAndLoadMp4Files(argv[1]);
    OpenDevicesInThreads();
    JoinOpenDevicesInThreads();
    for (unsigned int  i = 0 ; i< nDeviceNum; i++)
        tReadMp4WriteThreads.push_back(std::make_unique<std::thread>(readMp4FilesWrite2Disk,  i));
    for (unsigned int  i = 0 ; i< nDeviceNum; i++)
         tReadMp4WriteThreads[i]->join();
   

      


}