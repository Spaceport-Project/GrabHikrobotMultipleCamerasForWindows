#include <stdio.h>
#include <string.h>
#include <io.h>
#include <stdlib.h>
//#include <pthread.h>
#include <vector>
#include <memory>
#include <Windows.h>
#include <crtdefs.h>
#include <process.h>

#include "HikCamera.h"
#include "MvCameraControl.h"
// Wait for the user to press Enter to stop grabbing or end the program
void PressEnterToExit(void)
{
    // unsigned int c;
    fprintf( stderr, "\nPress enter to exit.\n");
    while( getchar() != '\n');
}
bool ConvertToHexIp(unsigned int *nHexIP, unsigned int *nDecIP, char c = '\n')
{
    if ( nDecIP[0] < 0 || nDecIP[0] > 255
        || nDecIP[1] < 0 || nDecIP[1] > 255
        || nDecIP[2] < 0 || nDecIP[2] > 255
        || nDecIP[3] < 0 || nDecIP[3] > 255
        || c != '\n')
    {
        return false;
    }
    *nHexIP = (nDecIP[0] << 24) + (nDecIP[1] << 16) + (nDecIP[2] << 8) + nDecIP[3];
    return true;
}
bool PrintDeviceInfo(MV_CC_DEVICE_INFO* pstMVDevInfo)
{
    if (NULL == pstMVDevInfo)
    {
        printf("The Pointer of pstMVDevInfo is NULL!\n");
        return false;
    }
    if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE)
    {
        int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
        int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
        int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
        int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);
        // Print the IP address and user defined name of the current camera
        printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chModelName);
        printf("CurrentIp: %d.%d.%d.%d\n" , nIp1, nIp2, nIp3, nIp4);
        printf("SerialNumber: %s\n\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chSerialNumber);
      //  printf("UserDefinedName: %s\n\n" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    }
    else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
    {
        printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chModelName);
        printf("UserDefinedName: %s\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
    }
    else
    {
        printf("Not support.\n");
    }
    return true;
}
int main()
{
    int nRet = MV_OK;
    void* handle = NULL;
    unsigned int nIP[4] = {0};
    unsigned int nIPCams[4] = {0};
    char c = '\0';
    unsigned int nIpAddr = 0, nNetWorkMask = 0, nDefaultGateway = 0;
    std::vector<std::unique_ptr<HikCamera>> m_pcMyCameras;
    m_pcMyCameras.push_back(std::make_unique<HikCamera>());

    do 
    {
        MV_CC_DEVICE_INFO_LIST stDeviceList;
        memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
        // Enumerate devices
        nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE, &stDeviceList);
        if (MV_OK != nRet)
        {
            printf("MV_CC_EnumDevices fail! nRet [%x]\n", nRet);
            break;
        }
        if (stDeviceList.nDeviceNum > 0)
        {
            for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++)
            {
                printf("[device %d]:\n", i);
                MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
                if (NULL == pDeviceInfo)
                {
                    break;
                } 
                PrintDeviceInfo(pDeviceInfo);            
            }  
        } 
        else
        {
            printf("Find No Devices!\n");
            break;
        }
        for (unsigned int i = 0 ; i < stDeviceList.nDeviceNum ; ++i) 
        {
            m_pcMyCameras.push_back(std::make_unique<HikCamera>());
            m_pcMyCameras[i]->CreateHandle(stDeviceList.pDeviceInfo[i]);
        }
        // printf("Please Intput camera index: ");
        // unsigned int nIndex = 0;
        // scanf("%d", &nIndex);
        // if (nIndex >= stDeviceList.nDeviceNum)
        // {
        //     printf("Intput error!\n");
        //     break;
        // }
        // // Create a handle for the selected device
        // nRet = MV_CC_CreateHandle(&handle, stDeviceList.pDeviceInfo[nIndex]);
        // if (MV_OK != nRet)
        // {
        //     printf("MV_CC_CreateHandle fail! nRet [%x]\n", nRet);
        //     break;
        // }
        // Enter the IP address, subnet mask, and default gateway
        printf("Please input starting ip address of first device, example: 192.168.1.100\n");
        // int ch;
        if ( 5 != scanf("%d.%d.%d.%d%c", &nIPCams[0], &nIPCams[1], &nIPCams[2], &nIPCams[3], &c) )
        {
            printf("input count error\n");
            (handle);
            for (unsigned int i = 0 ; i < stDeviceList.nDeviceNum ; ++i) 
                m_pcMyCameras[i]->Close();
            break;
        }
        if (!ConvertToHexIp(&nIpAddr, nIPCams, c))
        {
            printf("input IpAddr format is not correct\n");
            for (unsigned int i = 0 ; i < stDeviceList.nDeviceNum ; ++i) 
                m_pcMyCameras[i]->Close();
            break;
        }
        printf("Please input NetMask, example: 255.255.255.0\n");
        if ( 5 != scanf("%d.%d.%d.%d%c", &nIP[0], &nIP[1], &nIP[2], &nIP[3], &c) )
        {
            printf("input count error\n");
            for (unsigned int i = 0 ; i < stDeviceList.nDeviceNum ; ++i) 
                m_pcMyCameras[i]->Close();
            break;
        }
        if (!ConvertToHexIp(&nNetWorkMask, nIP, c))
        {
            printf("input NetMask format is not correct\n");
            for (unsigned int i = 0 ; i < stDeviceList.nDeviceNum ; ++i) 
                    m_pcMyCameras[i]->Close();
            break;
        }
        printf("Please input DefaultWay, example: 192.168.1.1\n");
        if ( 5 != scanf("%d.%d.%d.%d%c", &nIP[0], &nIP[1], &nIP[2], &nIP[3], &c) )
        {
            printf("input count error\n");
            for (unsigned int i = 0 ; i < stDeviceList.nDeviceNum ; ++i) 
                m_pcMyCameras[i]->Close();
            break;
        }
        if (!ConvertToHexIp(&nDefaultGateway, nIP, c))
        {
            printf("input DefaultWay format is not correct\n");
            for (unsigned int i = 0 ; i < stDeviceList.nDeviceNum ; ++i) 
                    m_pcMyCameras[i]->Close();

            break;
        }
        // Set the ForceIP
        for (unsigned int i = 0 ; i < stDeviceList.nDeviceNum ; ++i) 
        {
            nRet = m_pcMyCameras[i]->ForceIp(nIpAddr, nNetWorkMask, nDefaultGateway);
            if (MV_OK != nRet)
            {
                  printf("MV_GIGE_ForceIpEx fail! DevIndex[%d] nRet [%x]\n",i, nRet);
                  break;
            }
            nIPCams[3]++;
            if (!ConvertToHexIp(&nIpAddr, nIPCams))
            {
                printf("input IpAddr format is not correct\n");
                for (unsigned int j = 0 ; j < stDeviceList.nDeviceNum ; ++j) 
                    m_pcMyCameras[j]->Close();
                break;
            }


           
            
        }

        

        
        printf("Set IPs succeed\n");
        PressEnterToExit();
        // Destroy the handle
        for (unsigned int i = 0 ; i < stDeviceList.nDeviceNum ; ++i) 
                    m_pcMyCameras[i]->Close();
      
    } while (0);
   
    printf("exit\n");
    return 0;
}