#include "stdafx.h"
#include "communication_api.h"

#include "communication_logging.h"

#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>

CM_ERROR InitCommunicationModule()
{
    const BYTE majorWSALibVersion = 2;
    const BYTE minorWSALibVersion = 2;

    setbuf(stdout, NULL);

    WORD wVersionRequested = MAKEWORD(majorWSALibVersion, minorWSALibVersion); // WinSock version 2.2
    WSADATA wsaData;

    ZeroMemory(&wsaData, sizeof(WSADATA));

    if (WSAStartup(wVersionRequested, &wsaData) != 0)
    {
        return CM_LIB_INIT_FAILED;
    }

    if (LOBYTE(wsaData.wVersion) != minorWSALibVersion || HIBYTE(wsaData.wVersion) != majorWSALibVersion)
    {
        WSACleanup();
        return CM_LIB_INIT_FAILED;
    }

    return CM_SUCCESS;
}

void UninitCommunicationModule()
{
    WSACleanup();
}

void EnableCommunicationModuleLogger()
{
    EnableLogging();
}

void DisableCommunicationModuleLogger()
{
    DisableLogging();
}
