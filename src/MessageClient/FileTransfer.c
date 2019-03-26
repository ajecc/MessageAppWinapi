#include "stdafx.h"
#include "FileTransfer.h"
#include "MessageClient.h"
#include "communication_api.h"
#include <strsafe.h>

BOOLEAN IsReceiveSignal(LPTSTR String)
{
    if(NULL == String)
    {
        return FALSE;
    }

    TOKEN_STRINGS* tokenStrings = NULL;
    CM_ERROR cmError = CreateTokenStrings(&tokenStrings, 2);
    if(CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(tokenStrings);
        return FALSE;
    }
    cmError = TokenizeString(tokenStrings, String, TEXT(' '), 2, BUFFER_SIZE);
    if(CM_IS_ERROR(cmError) || 2 != tokenStrings->TokenCount)
    {
        DestroyTokenStrings(tokenStrings);
        return FALSE;
    }
    BOOLEAN isReceiveSignal = 0 == _tcscmp(tokenStrings->TokenStrings[0], FILE_RECEIVE_SIGNAL);
    DestroyTokenStrings(tokenStrings);
    return isReceiveSignal;
}

CM_ERROR InitWritingToReceivedFile(HANDLE* ReceivedFile, CM_SIZE* WriteByteCount, LPTSTR ReceiveSignalString)
{
    if(NULL == ReceivedFile || NULL == WriteByteCount || NULL == ReceiveSignalString)
    {
        return CM_INVALID_PARAMETER;
    }

    TOKEN_STRINGS* receiveSignalStringTokenized = NULL;
    LPTSTR filePath = NULL;
    HANDLE receivedFile = INVALID_HANDLE_VALUE;
    DWORD writeByteCount = 0;
    CM_ERROR cmError = CM_SUCCESS;
    __try
    {
        cmError = CreateTokenStrings(&receiveSignalStringTokenized, 3);
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }
        cmError = TokenizeString(receiveSignalStringTokenized, ReceiveSignalString, TEXT(' '), 3, BUFFER_SIZE);
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }

        LPTSTR byteToWriteCountString = receiveSignalStringTokenized->TokenStrings[1];
        cmError = ConvertStringToDword(&writeByteCount, byteToWriteCountString);

        filePath = (LPTSTR)malloc(BUFFER_SIZE);
        if(NULL == filePath)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        DWORD dError = GetCurrentDirectory(CHARS_COUNT, filePath);
        if(0 == dError)
        {
            cmError = CM_EXT_FUNC_FAILURE;
            __leave;
        }
        LPTSTR fileName = receiveSignalStringTokenized->TokenStrings[2];
        CM_SIZE fileNameSize = 0;
        HRESULT hrError = StringCchLength(fileName, CHARS_COUNT, &fileNameSize);
        if(FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        fileName[fileNameSize - 2] = TEXT('\0');
        hrError = StringCbCat(filePath, BUFFER_SIZE, TEXT("\\"));
        if(FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(filePath, BUFFER_SIZE, fileName);
        if(FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        receivedFile = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
            NULL, OPEN_ALWAYS, 0, NULL);
        if(INVALID_HANDLE_VALUE == receivedFile)
        {
            cmError = CM_EXT_FUNC_FAILURE;
            __leave;
        }

    }
    __finally
    {
        DestroyTokenStrings(receiveSignalStringTokenized);
        if(NULL != filePath)
        {
            free(filePath);
            filePath = NULL;
        }
    }

    if(CM_IS_SUCCESS(cmError))
    {
        *ReceivedFile = receivedFile;
        *WriteByteCount = (CM_SIZE)writeByteCount;
    }
    return cmError;
}

CM_ERROR WriteToReceivedFile(HANDLE* ReceivedFile, CM_SIZE* WriteToFileByteCount, CM_DATA_BUFFER* DataBuffer)
{
    if(NULL == ReceivedFile || INVALID_HANDLE_VALUE == *ReceivedFile
        || NULL == WriteToFileByteCount || 0 == *WriteToFileByteCount
        || NULL == DataBuffer)
    {
        return CM_INVALID_PARAMETER;
    }

    HANDLE receivedFile = *ReceivedFile;
    CM_SIZE writeToFileByteCount = *WriteToFileByteCount;
    if(DataBuffer->UsedBufferSize < writeToFileByteCount)
    {
        writeToFileByteCount = DataBuffer->UsedBufferSize;
    }
    LARGE_INTEGER dontMove;
    dontMove.HighPart = dontMove.LowPart = 0;
    BOOL bError = SetFilePointerEx(receivedFile, dontMove, NULL, FILE_END);
    if(FALSE == bError)
    {
        return CM_EXT_FUNC_FAILURE;
    }
    DWORD numberOfBytesWrittenDword = 0;
    bError = WriteFile(receivedFile, DataBuffer->DataBuffer, 
        writeToFileByteCount, &numberOfBytesWrittenDword, NULL);
    if(FALSE == bError)
    {
        return CM_EXT_FUNC_FAILURE;
    }
    CM_SIZE numberOfBytesWritten = (CM_SIZE)numberOfBytesWrittenDword;
    for(CM_SIZE i = numberOfBytesWritten; i < DataBuffer->UsedBufferSize; i++)
    {
        DataBuffer->DataBuffer[i - numberOfBytesWritten] = DataBuffer->DataBuffer[i];
    }
    DataBuffer->UsedBufferSize -= numberOfBytesWritten;
    *WriteToFileByteCount = *WriteToFileByteCount - numberOfBytesWritten;
    if(0 == *WriteToFileByteCount)
    {
        CloseHandle(receivedFile);
        *ReceivedFile = INVALID_HANDLE_VALUE;
    }
    return CM_SUCCESS;
}

CM_ERROR CreateSendFileArgs(SEND_FILE_ARGS** SendFileArgs, CM_CLIENT* Client, LPTSTR SendFileSignal)
{
    if(NULL == SendFileArgs || NULL != *SendFileArgs 
        || NULL == Client || NULL == SendFileSignal)
    {
        return CM_INVALID_PARAMETER;
    }

    SEND_FILE_ARGS* sendFileArgs = NULL;
    TOKEN_STRINGS* sendFileSignalTokenized = NULL;

    CM_ERROR cmError = CM_SUCCESS;

    sendFileArgs = (SEND_FILE_ARGS*)malloc(sizeof(SEND_FILE_ARGS));
    if(NULL == sendFileArgs)
    {
        cmError = CM_NO_MEMORY;
        goto cleanup;
    }
    sendFileArgs->FilePath = NULL;
    sendFileArgs->ReceiverUsername = NULL;
    sendFileArgs->Client = Client;

    sendFileArgs->FilePath = (LPTSTR)malloc(BUFFER_SIZE);
    if(NULL == sendFileArgs->FilePath)
    {
        cmError = CM_NO_MEMORY;
        goto cleanup;
    }
    sendFileArgs->ReceiverUsername = (LPTSTR)malloc(BUFFER_SIZE);
    if(NULL == sendFileArgs->ReceiverUsername)
    {
        cmError = CM_NO_MEMORY;
        goto cleanup;
    }

    cmError = CreateTokenStrings(&sendFileSignalTokenized, 3);
    if(CM_IS_ERROR(cmError))
    {
        goto cleanup;
    }
    cmError = TokenizeString(sendFileSignalTokenized, SendFileSignal, TEXT(' '), 3, BUFFER_SIZE);
    if(CM_IS_ERROR(cmError))
    {
        goto cleanup;
    }
    HRESULT hrError = StringCbCopy(sendFileArgs->ReceiverUsername, BUFFER_SIZE, sendFileSignalTokenized->TokenStrings[1]);
    if(FAILED(hrError))
    {
        cmError = CM_INSUFFICIENT_BUFFER;
        goto cleanup;
    }
    hrError = StringCbCopy(sendFileArgs->FilePath, BUFFER_SIZE, sendFileSignalTokenized->TokenStrings[2]);
    if(FAILED(hrError))
    {
        cmError = CM_INSUFFICIENT_BUFFER;
        goto cleanup;
    }
    CM_SIZE filePathLen = 0;
    hrError = StringCchLength(sendFileArgs->FilePath, BUFFER_SIZE, &filePathLen);
    if(FAILED(hrError))
    {
        cmError = CM_INSUFFICIENT_BUFFER;
        goto cleanup;
    }
    sendFileArgs->FilePath[filePathLen - 2] = TEXT('\0');
    DWORD attributes = GetFileAttributes(sendFileArgs->FilePath);
    if(INVALID_FILE_ATTRIBUTES == attributes ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        _tprintf_s(TEXT("Error: File not found\r\n"));
        return CM_EXT_FUNC_FAILURE;
    }
    hrError = StringCbCopy(sendFileArgs->FilePath, BUFFER_SIZE, sendFileSignalTokenized->TokenStrings[2]);
    if(FAILED(hrError))
    {
        cmError = CM_INSUFFICIENT_BUFFER;
    }


cleanup:
    DestroyTokenStrings(sendFileSignalTokenized);
    if(CM_IS_ERROR(cmError))
    {
        if(NULL != sendFileArgs && NULL != sendFileArgs->FilePath)
        {
            free(sendFileArgs->FilePath);
            sendFileArgs->FilePath = NULL;
        }
        if(NULL != sendFileArgs && NULL != sendFileArgs->ReceiverUsername)
        {
            free(sendFileArgs->ReceiverUsername);
            sendFileArgs->ReceiverUsername = NULL;
        }
        if(NULL != sendFileArgs)
        {
            sendFileArgs->Client = NULL;
            free(sendFileArgs);
            sendFileArgs = NULL;
        }
    }
    *SendFileArgs = sendFileArgs;
    return cmError;
}

CM_ERROR DestroySendFileArgs(SEND_FILE_ARGS* SendFileArgs)
{
    if(NULL == SendFileArgs)
    {
        return CM_INVALID_PARAMETER;
    }
    if(NULL != SendFileArgs->ReceiverUsername)
    {
        free(SendFileArgs->ReceiverUsername);
        SendFileArgs->ReceiverUsername = NULL;
    }
    if(NULL != SendFileArgs->FilePath)
    {
        free(SendFileArgs->FilePath);
        SendFileArgs->FilePath = NULL;
    }
    SendFileArgs->Client = NULL;
    free(SendFileArgs);
    return CM_SUCCESS;
}

CM_ERROR WINAPI SendFile(P_SEND_FILE_ARGS SendFileArgs)
{
    if(NULL == SendFileArgs || NULL == SendFileArgs->ReceiverUsername || 
        NULL == SendFileArgs->FilePath || NULL == SendFileArgs->Client)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR receiverUsername = SendFileArgs->ReceiverUsername;
    LPTSTR filePath = SendFileArgs->FilePath;
    CM_CLIENT* client = SendFileArgs->Client;
    CRITICAL_SECTION* critSection = SendFileArgs->CritSection;

    CM_SIZE filePathLen = 0;
    HRESULT hrError = StringCchLength(filePath, CHARS_COUNT, &filePathLen);
    if(FAILED(hrError))
    {
        return CM_EXT_FUNC_FAILURE;
    }
    filePath[filePathLen - 2] = TEXT('\0');
    LPTSTR fileName = NULL;
    TOKEN_STRINGS* filePathTokenized = NULL;
    CreateTokenStrings(&filePathTokenized, CHARS_COUNT);
    TokenizeString(filePathTokenized, filePath, TEXT('\\'), CHARS_COUNT, BUFFER_SIZE);
    fileName = filePathTokenized->TokenStrings[filePathTokenized->TokenCount - 1];

    HANDLE file = INVALID_HANDLE_VALUE;
    CM_DATA_BUFFER* dataBuffer = NULL;
    CM_DATA_BUFFER* dataBufferToSend = NULL;
    LPTSTR fileBytesToString = NULL;
    LPTSTR fileBytesToStringCount = NULL;
    LPTSTR sendFileCommand = NULL;
    CM_ERROR cmError = CM_SUCCESS;

    EnterCriticalSection(critSection);
    __try
    {
        file = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, 0, NULL);
        if(INVALID_HANDLE_VALUE == file)
        {
            cmError = CM_EXT_FUNC_FAILURE;
            __leave;
        }
        cmError = CreateDataBuffer(&dataBuffer, BUFFER_SIZE); 
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }
        cmError = CreateDataBuffer(&dataBufferToSend, BUFFER_SIZE);
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }
        fileBytesToString = (LPTSTR)malloc(BUFFER_SIZE);
        if(NULL == fileBytesToString)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        fileBytesToStringCount = (LPTSTR)malloc(BUFFER_SIZE);
        if(NULL == fileBytesToStringCount)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        sendFileCommand = (LPTSTR)malloc(BUFFER_SIZE);
        if(NULL == sendFileCommand)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }

        for(;;)
        {
            DWORD usedBufferSize = 0;
            hrError = ReadFile(file, dataBuffer->DataBuffer, BUFFER_SIZE / 2, 
                &usedBufferSize, NULL);
            dataBuffer->UsedBufferSize = (CM_SIZE)usedBufferSize;
            if(FAILED(hrError))
            {
                cmError = CM_EXT_FUNC_FAILURE;
                __leave;
            }
            hrError = StringCbCopy(sendFileCommand, BUFFER_SIZE, FILE_SEND_SIGNAL);
            if(FAILED(hrError))
            {
                cmError = CM_EXT_FUNC_FAILURE;
                __leave;
            }
            hrError = StringCbCat(sendFileCommand, BUFFER_SIZE, TEXT(" "));
            if(FAILED(hrError))
            {
                cmError = CM_EXT_FUNC_FAILURE;
                __leave;
            }
            hrError = StringCbCat(sendFileCommand, BUFFER_SIZE, receiverUsername);
            if(FAILED(hrError))
            {
                cmError = CM_EXT_FUNC_FAILURE;
                __leave;
            }
            hrError = StringCbPrintf(fileBytesToStringCount, BUFFER_SIZE, TEXT(" %d "), dataBuffer->UsedBufferSize);
            if(FAILED(hrError))
            {
                cmError = CM_EXT_FUNC_FAILURE;
                __leave;
            }
            cmError = StringCbCat(sendFileCommand, BUFFER_SIZE, fileBytesToStringCount);
            if(FAILED(hrError))
            {
                cmError = CM_EXT_FUNC_FAILURE;
                __leave;
            }
            hrError = StringCbCat(sendFileCommand, BUFFER_SIZE, fileName);
            if(FAILED(hrError))
            {
                cmError = CM_EXT_FUNC_FAILURE;
                __leave;
            }
            hrError = StringCbCat(sendFileCommand, BUFFER_SIZE, TEXT("\r\n"));
            if(FAILED(hrError))
            {
                cmError = CM_EXT_FUNC_FAILURE;
                __leave;
            }
            ConvertStringToDataBuffer(dataBufferToSend, sendFileCommand, BUFFER_SIZE);
            for(CM_SIZE i = 0; i < dataBuffer->UsedBufferSize; i++)
            {
                dataBufferToSend->DataBuffer[dataBufferToSend->UsedBufferSize++] = dataBuffer->DataBuffer[i];
            }
            CM_SIZE dummy = 0;
            SendDataToServer(client, dataBufferToSend, &dummy);
            if(0 == usedBufferSize)
            {
                _tprintf_s(TEXT("Successfully sent file: %s\r\n"), filePath);
                break;
            }
            Sleep(20); // Sleep so the file transfer doesn't take all the bandwidth...
        }     
    }
    _finally
    {
        LeaveCriticalSection(critSection);
        if(INVALID_HANDLE_VALUE != file)
        {
            CloseHandle(file);
        }
        DestroyDataBuffer(dataBuffer);
        DestroyDataBuffer(dataBufferToSend);
        if(NULL != fileBytesToString)
        {
            free(fileBytesToString);
            fileBytesToString = NULL;
        }
        if(NULL != fileBytesToStringCount)
        {
            free(fileBytesToStringCount);
            fileBytesToStringCount = NULL;
        }
        if(NULL != sendFileCommand)
        {
            free(sendFileCommand);
            sendFileCommand = NULL;
        }
    }
    DestroyTokenStrings(filePathTokenized);
    DestroySendFileArgs(SendFileArgs);
    return cmError;
}
