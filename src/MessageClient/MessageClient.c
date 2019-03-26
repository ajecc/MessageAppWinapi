#include "stdafx.h"

#include "MessageClient.h"
#include "FileTransfer.h"
#include "communication_api.h"
#include "communication_string_functions.h"

#include <windows.h>
#include <strsafe.h>
#include <process.h>

static CM_ERROR WINAPI MainClientSend(PCM_CLIENT Client);
static CM_ERROR WINAPI MainClientReceive(PCM_CLIENT Client);

static CM_ERROR DataBufferCat(CM_DATA_BUFFER* Destination, CM_DATA_BUFFER* Source);

int _tmain(void)
{
    EnableCommunicationModuleLogger();

    CM_ERROR error = InitCommunicationModule();
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: InitCommunicationModule failed with err-code=0x%X!\r\n"), error);
        UninitCommunicationModule();
        return EXIT_FAILURE;
    }

    CM_CLIENT* client = NULL;
    error = CreateClientConnectionToServer(&client);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Error: no running server found\r\n"));
        UninitCommunicationModule();
        return EXIT_FAILURE;
    }
    
    HANDLE clientThreads[CLIENT_THREADS_COUNT];
    clientThreads[0] = (HANDLE)_beginthreadex(NULL, 0, MainClientReceive, client, 0, NULL);
    clientThreads[1] = (HANDLE)_beginthreadex(NULL, 0, MainClientSend, client, 0, NULL);

    WaitForMultipleObjects(CLIENT_THREADS_COUNT, clientThreads, TRUE, INFINITE);
    CloseHandle(clientThreads[0]);
    CloseHandle(clientThreads[1]);
    DestroyClient(client);
    UninitCommunicationModule();
    return 0;
}

static CM_ERROR WINAPI MainClientSend(PCM_CLIENT Client)
{
    if (NULL == Client)
    {
        return CM_INVALID_PARAMETER;
    }
    CM_CLIENT* client = Client;

    CM_ERROR cmError = CM_SUCCESS;

    CM_DATA_BUFFER* dataBuffer = NULL;
    cmError = CreateDataBuffer(&dataBuffer, BUFFER_SIZE);
    if (CM_IS_ERROR(cmError))
    {
        DestroyDataBuffer(dataBuffer);
        return cmError;
    }

    LPTSTR stringToSend = (LPTSTR)malloc(BUFFER_SIZE);
    if (NULL == stringToSend)
    {
        DestroyDataBuffer(dataBuffer);
        return CM_NO_MEMORY;
    }
    
    while (TRUE)
    {
        stringToSend[0] = TEXT('$');
        stringToSend[1] = TEXT('\0');
        CM_SIZE stringSize = 0;
        for (;;)
        {
            TCHAR charRead = _gettchar();
            if (charRead == TEXT('\n'))
            {
                break;
            }
            stringToSend[stringSize++] = charRead;
            if(stringSize >= CHARS_COUNT - 1)
            {
                rewind(stdin);
                break;
            }
        }
        stringToSend[stringSize] = TEXT('\0');
        if(TEXT('$') == stringToSend[0])
        {
            _tprintf_s(TEXT("Error: Invalid command\r\n"));
            continue;
        }
        HRESULT hrError = StringCchCat(stringToSend, CHARS_COUNT, TEXT("\r\n"));
        if(FAILED(hrError))
        {
            _tprintf_s(TEXT("Unexpected error: command sent might be too long. Last error code is: 0x%X\r\n"), GetLastError());
            continue;
        }
        cmError = SendStringToServer(client, dataBuffer, stringToSend);
        if (CM_IS_ERROR(cmError))
        {
            _tprintf_s(TEXT("Unexpected error: Failed to send string to server with err-code=0x%X!\r\n"), cmError);
        }
        if(0 == _tcsncmp(stringToSend, TEXT("exit\r\n"), 7))
        {
            break;
        }
        Sleep(10); // prevent super mega spam that might overflow the socket buffer
    }

    DestroyDataBuffer(dataBuffer);
    if (NULL != stringToSend)
    {
        free(stringToSend);
        stringToSend = NULL;
    }
    return CM_SUCCESS;
}


static CM_ERROR WINAPI MainClientReceive(PCM_CLIENT Client)
{
    if (NULL == Client)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_CLIENT* client = Client;

    CM_ERROR cmError = CM_SUCCESS;
    BOOL bError = TRUE;

    CM_DATA_BUFFER* dataBuffer = NULL;
    CM_DATA_BUFFER* dataBufferCat = NULL;
    LPTSTR stringsReceivedCated = NULL;
    LPTSTR stringWithCrlf = NULL;

    cmError = CreateDataBuffer(&dataBuffer, BUFFER_SIZE);
    if (CM_IS_ERROR(cmError))
    {
        goto cleanup;
    }
    cmError = CreateDataBuffer(&dataBufferCat, CAT_BUFFER_SIZE);
    if (CM_IS_ERROR(cmError))
    {
        goto cleanup;
    }
    stringsReceivedCated = (LPTSTR)malloc(CAT_BUFFER_SIZE + sizeof(TCHAR));
    if(NULL == stringsReceivedCated)
    {
        goto cleanup;
    }
    stringsReceivedCated[0] = TEXT('\0');
    stringWithCrlf = (LPTSTR)malloc(BUFFER_SIZE);
    if(NULL == stringWithCrlf)
    {
        cmError = CM_NO_MEMORY;
        goto cleanup;
    }
    HANDLE receivedFile = INVALID_HANDLE_VALUE;
    CM_SIZE writeToFileByteCount = 0;

    CRITICAL_SECTION sendFileCritSection;
    InitializeCriticalSection(&sendFileCritSection);

    while (TRUE)
    {
        CM_SIZE receivedBytes = 0;
        if(0 == writeToFileByteCount || dataBufferCat->UsedBufferSize < writeToFileByteCount)
        {
            cmError = ReceiveDataFormServer(client, dataBuffer, &receivedBytes);
            if (CM_IS_ERROR(cmError))
            {
                _tprintf_s(TEXT("Unexpected error: ReceivedDataFromServer failed with err-code=0x%X!\r\n"), cmError);
                continue;
            }
        }
        cmError = DataBufferCat(dataBufferCat, dataBuffer);
        if(CM_IS_ERROR(cmError))
        {
            continue;
        }
        dataBuffer->UsedBufferSize = 0;
        if(0 != writeToFileByteCount)
        {
            cmError = WriteToReceivedFile(&receivedFile, &writeToFileByteCount, dataBufferCat);
            if(0 != writeToFileByteCount || CM_IS_ERROR(cmError))
            {
                continue;
            }
        }
        CopyMemory(stringsReceivedCated, dataBufferCat->DataBuffer, dataBufferCat->UsedBufferSize 
            - dataBufferCat->UsedBufferSize % sizeof(TCHAR));
        stringsReceivedCated[dataBufferCat->UsedBufferSize / sizeof(TCHAR)] = TEXT('\0');
        for(;;)
        {
            cmError = ExtractStringWithCrlf(stringWithCrlf, CHARS_COUNT,stringsReceivedCated, CAT_CHARS_COUNT);
            if(CM_IS_ERROR(cmError))
            {
                break;
            }
            CM_SIZE stringWithCrlfLen = 0;
            HRESULT hrError = StringCbLength(stringWithCrlf, BUFFER_SIZE, &stringWithCrlfLen);
            if(FAILED(hrError))
            {
                break;
            }
            for(CM_SIZE i = 0; i < dataBufferCat->UsedBufferSize - stringWithCrlfLen; i++)
            {
                dataBufferCat->DataBuffer[i] = dataBufferCat->DataBuffer[i + stringWithCrlfLen];
            }
            dataBufferCat->UsedBufferSize -= stringWithCrlfLen;

            if(0 == stringWithCrlfLen || 0 == _tcscmp(stringWithCrlf, EXIT_SIGNAL))
            {
                break;
            }
            if(IsReceiveSignal(stringWithCrlf))
            {
                cmError = InitWritingToReceivedFile(&receivedFile, &writeToFileByteCount, stringWithCrlf);
                break;
            }
            dataBuffer->UsedBufferSize = 0;
            if(IsSendSignal(stringWithCrlf))
            {
                P_SEND_FILE_ARGS sendFileArgs = NULL;
                cmError = CreateSendFileArgs(&sendFileArgs, client, stringWithCrlf);
                if(CM_IS_ERROR(cmError))
                {
                    DestroySendFileArgs(sendFileArgs);
                    break;
                }
                sendFileArgs->CritSection = &sendFileCritSection;
                _tprintf_s(TEXT("Success\r\n"));
                _beginthreadex(NULL, 0, SendFile, sendFileArgs, 0, NULL);
                break;
            }
            _tprintf_s(TEXT("%s"), stringWithCrlf);
        }
        if(0 == _tcscmp(stringWithCrlf, EXIT_SIGNAL))
        {
            break;
        }
    }

cleanup:
    DestroyDataBuffer(dataBuffer);
    DestroyDataBuffer(dataBufferCat);
    if(NULL != stringsReceivedCated)
    {
        free(stringsReceivedCated);
    }
    if(NULL != stringWithCrlf)
    {
        free(stringWithCrlf);
    }
    return cmError;
}

CM_ERROR SendStringToServer(CM_CLIENT* Client, CM_DATA_BUFFER* DataBuffer, LPTSTR String)
{
    if (NULL == Client || NULL == DataBuffer || NULL == String)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_CLIENT* client = Client;
    CM_DATA_BUFFER* dataBuffer = DataBuffer;
    LPTSTR string = String;

    CM_ERROR cmError = CM_SUCCESS;

    cmError = ConvertStringToDataBuffer(dataBuffer, string, BUFFER_SIZE);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    CM_SIZE sentBytes = 0;
    cmError = SendDataToServer(client, dataBuffer, &sentBytes);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }

    return CM_SUCCESS;
}

static CM_ERROR DataBufferCat(CM_DATA_BUFFER* Destination, CM_DATA_BUFFER* Source)
{
    for(CM_SIZE i = 0; i < Source->UsedBufferSize; i++)
    {
        Destination->DataBuffer[Destination->UsedBufferSize++] = Source->DataBuffer[i];
    }
    return CM_SUCCESS;
}
