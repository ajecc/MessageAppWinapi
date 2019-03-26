#include "stdafx.h"
#include "FileManagement.h"
#include "UserManagement.h"
#include "UsersHashTable.h"
#include <strsafe.h>

static CM_ERROR GetMsgsFolderPath(LPTSTR MsgFolderPath, CM_SIZE CharsCount);
static CM_ERROR GetHistoryFolderPath(LPTSTR HistoryFolderPath, CM_SIZE CharsCount);
static CM_ERROR GetHistoryPath(LPTSTR LhsUsername, LPTSTR RhsUsername, LPTSTR HistoryPath, CM_SIZE CharsCount);
static BOOLEAN CheckFileExistance(LPTSTR FilePath);

CM_ERROR SetupFiles(void)
{
    HANDLE handleError = CreateFile(
        REGISTER_FILE_PATH, // file name
        GENERIC_READ | GENERIC_WRITE, // access mode
        FILE_SHARE_READ, // share mode
        NULL, // security attributes
        CREATE_NEW, // creation disposition
        FILE_ATTRIBUTE_NORMAL, // file attribute
        NULL);                            // template file
    if (INVALID_HANDLE_VALUE == handleError && ERROR_FILE_EXISTS != GetLastError())
    {
        _tprintf_s(TEXT("Unexpected error: creating the registration file failed with err-code=0x%X. Maybe missing admin rights?\r\n"), GetLastError());
        return CM_EXT_FUNC_FAILURE;
    }
    if(INVALID_HANDLE_VALUE != handleError)
    {
        CloseHandle(handleError);
    }
    LPTSTR msgFolderPath = (LPTSTR)malloc(BUFFER_SIZE);
    if (NULL == msgFolderPath)
    {
        return CM_NO_MEMORY;
    }
    CM_ERROR cmError = GetMsgsFolderPath(msgFolderPath, CHARS_COUNT);
    if (CM_IS_ERROR(cmError))
    {
        if (NULL != msgFolderPath)
        {
            free(msgFolderPath);
            msgFolderPath = NULL;
        }
        return cmError;
    }
    BOOL bError = CreateDirectory(msgFolderPath, NULL);
    if (NULL != msgFolderPath)
    {
        free(msgFolderPath);
        msgFolderPath = NULL;
    }
    if (0 == bError && ERROR_ALREADY_EXISTS != GetLastError())
    {
        _tprintf_s(TEXT("Unexpected error: creating the message folder failed with err-code=0x%X. Maybe missing admin rights?\r\n"), GetLastError());
        return CM_EXT_FUNC_FAILURE;
    }
    LPTSTR historyFolderPath = (LPTSTR)malloc(BUFFER_SIZE);
    if(NULL == historyFolderPath)
    {
        return CM_NO_MEMORY;
    }
    cmError = GetHistoryFolderPath(historyFolderPath, CHARS_COUNT);
    if(CM_IS_ERROR(cmError))
    {
        if(NULL != historyFolderPath)
        {
            free(historyFolderPath);
            historyFolderPath = NULL;
        }
        return cmError;
    }
    bError = CreateDirectory(historyFolderPath, NULL);
    if(NULL != historyFolderPath)
    {
        free(historyFolderPath);
        historyFolderPath = NULL;
    }
    if (0 == bError && ERROR_ALREADY_EXISTS != GetLastError())
    {
        _tprintf_s(TEXT("Unexpected error: creating the history folder failed with err-code=0x%X. Maybe missing admin rights?\r\n"), GetLastError());
        return CM_EXT_FUNC_FAILURE;
    }
    return CM_SUCCESS;
}

CM_ERROR ReadFileLines(HANDLE File, TOKEN_STRINGS* Lines, CM_SIZE BufferSize, BOOLEAN* ReachedEof)
{
    if (NULL == File || NULL == Lines || NULL == ReachedEof)
    {
        return CM_INVALID_PARAMETER;
    }

    HANDLE file = File;
    TOKEN_STRINGS* lines = Lines;
    const CM_SIZE bufferSize = BufferSize;

    CM_DATA_BUFFER* buffer = NULL;
    CM_ERROR cmError = CreateDataBuffer(&buffer, bufferSize);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    EnterCriticalSection(&gFileCritSection);
    BOOL bError = ReadFile(file, buffer->DataBuffer, BufferSize,
                           (LPDWORD)(&buffer->UsedBufferSize), NULL);
    if (FALSE == bError)
    {
        LeaveCriticalSection(&gFileCritSection);
        DestroyDataBuffer(buffer);
        return CM_EXT_FUNC_FAILURE;
    }
    DWORD usedBufferSize = (DWORD)buffer->UsedBufferSize;
    if (0 == usedBufferSize)
    {
        LeaveCriticalSection(&gFileCritSection);
        DestroyDataBuffer(buffer);
        lines->TokenCount = 0;
        *ReachedEof = TRUE;
        return CM_SUCCESS;
    }
    LPTSTR auxString = (LPTSTR)malloc(usedBufferSize + sizeof(TCHAR));
    if (NULL == auxString)
    {
        LeaveCriticalSection(&gFileCritSection);
        DestroyDataBuffer(buffer);
        return CM_NO_MEMORY;
    }
    CopyMemory(auxString, buffer->DataBuffer, usedBufferSize);
    auxString[usedBufferSize / sizeof(TCHAR)] = TEXT('\0');
    cmError = TokenizeString(lines, auxString, TEXT('\n'), CHARS_COUNT, usedBufferSize);
    if (NULL != auxString)
    {
        free(auxString);
        auxString = NULL;
    }
    DestroyDataBuffer(buffer);
    if (CM_IS_ERROR(cmError))
    {
        LeaveCriticalSection(&gFileCritSection);
        return cmError;
    }
    if (usedBufferSize < BufferSize)
    {
        LeaveCriticalSection(&gFileCritSection);
        *ReachedEof = TRUE;
        return CM_SUCCESS;
    }

    CM_SIZE lastTokenByteCount = 0;
    HRESULT hrError = StringCbLength(lines->TokenStrings[lines->TokenCount - 1], bufferSize, &lastTokenByteCount);
    if (FAILED(hrError))
    {
        LeaveCriticalSection(&gFileCritSection);
        return CM_INSUFFICIENT_BUFFER;
    }

    LARGE_INTEGER largeLastTokenByteCount;
    largeLastTokenByteCount.u.LowPart = ~lastTokenByteCount;
    largeLastTokenByteCount.u.HighPart = 0;
    largeLastTokenByteCount.u.HighPart = ~largeLastTokenByteCount.u.HighPart;
    largeLastTokenByteCount.u.LowPart += 1;
    if(0 == largeLastTokenByteCount.u.LowPart)
    {
        largeLastTokenByteCount.u.HighPart += 1;
    }
    bError = SetFilePointerEx(file, largeLastTokenByteCount, NULL, FILE_CURRENT);
    LeaveCriticalSection(&gFileCritSection);
    if (FALSE == bError)
    {
        return CM_EXT_FUNC_FAILURE;
    }
    lines->TokenCount--;

    *ReachedEof = FALSE;
    return CM_SUCCESS;
}

CM_ERROR AppendStringToFile(HANDLE File, LPTSTR String)
{
    if (INVALID_HANDLE_VALUE == File || NULL == String)
    {
        return CM_INVALID_PARAMETER;
    }

    LARGE_INTEGER dontMove;
    dontMove.u.HighPart = dontMove.u.LowPart = 0;
    EnterCriticalSection(&gFileCritSection);
    BOOL bError = SetFilePointerEx(File, dontMove, NULL, FILE_END);  
    if (FALSE == bError)
    {
        LeaveCriticalSection(&gFileCritSection);
        return CM_EXT_FUNC_FAILURE;
    }
    if (INVALID_HANDLE_VALUE == File)
    {
        LeaveCriticalSection(&gFileCritSection);
        return CM_EXT_FUNC_FAILURE;
    }

    CM_SIZE bytesCnt = 0;
    HRESULT hrError = StringCbLength(String, BUFFER_SIZE, &bytesCnt);
    if (FAILED(hrError))
    {
        LeaveCriticalSection(&gFileCritSection);
        return CM_INSUFFICIENT_BUFFER;
    }
    DWORD bytesWritten = 0;
    bError = WriteFile(File, String, bytesCnt, &bytesWritten, NULL);
    LeaveCriticalSection(&gFileCritSection);
    if (FALSE == bError || bytesWritten != bytesCnt)
    {
        return CM_EXT_FUNC_FAILURE;
    }
    return CM_SUCCESS;
}

CM_ERROR CreatePendingMsgsFile(HANDLE* PendingMsgsFile, LPTSTR Username, DWORD CreationDisposition)
{
    if (NULL == PendingMsgsFile || NULL == Username)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_ERROR cmError = CM_SUCCESS;

    LPTSTR pendingMsgsPath = NULL;
    HANDLE pendingMsgsFile = INVALID_HANDLE_VALUE;
    __try
    {
        pendingMsgsPath = (LPTSTR)malloc(BUFFER_SIZE);
        if (NULL == pendingMsgsPath)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        cmError = GetPendingMsgsPath(Username, pendingMsgsPath, CHARS_COUNT);
        if (CM_IS_ERROR(cmError))
        {
            __leave;
        }
        EnterCriticalSection(&gFileCritSection);
        pendingMsgsFile = CreateFile(pendingMsgsPath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CreationDisposition,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        LeaveCriticalSection(&gFileCritSection);
        if (INVALID_HANDLE_VALUE == pendingMsgsFile)
        {
            cmError = CM_EXT_FUNC_FAILURE;
            __leave;
        }
    }
    __finally
    {
        if (NULL != pendingMsgsPath)
        {
            free(pendingMsgsPath);
            pendingMsgsPath = NULL;
        }
        *PendingMsgsFile = pendingMsgsFile;
    }
    return cmError;
}

CM_ERROR ClearFile(HANDLE File)
{
    if (INVALID_HANDLE_VALUE == File)
    {
        return CM_INVALID_PARAMETER;
    }

    LARGE_INTEGER dontMove;
    dontMove.u.HighPart = dontMove.u.LowPart = 0;
    EnterCriticalSection(&gFileCritSection);
    BOOL bError = SetFilePointerEx(File, dontMove, NULL, FILE_BEGIN);
    if (0 == bError)
    {
        LeaveCriticalSection(&gFileCritSection);
        return CM_EXT_FUNC_FAILURE;
    }
    bError = SetEndOfFile(File);
    LeaveCriticalSection(&gFileCritSection);
    if (0 == bError)
    {
        return CM_EXT_FUNC_FAILURE;
    }
    return CM_SUCCESS;
}

CM_ERROR CountLinesOfFile(HANDLE File, CM_SIZE* LinesCount)
{
    if(INVALID_HANDLE_VALUE == File || NULL == LinesCount)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_SIZE linesCount = 0;

    TOKEN_STRINGS* lines = NULL;
    CM_ERROR cmError = CreateTokenStrings(&lines, CHARS_COUNT);
    if(CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(lines);
        return cmError;
    }

    BOOLEAN reachedEof = FALSE;
    while(!reachedEof)
    {
        cmError = ReadFileLines(File, lines, BUFFER_SIZE - 5 * sizeof(TCHAR), &reachedEof);
        if(CM_IS_ERROR(cmError))
        {
            DestroyTokenStrings(lines);
            return cmError;
        }
        linesCount += lines->TokenCount;
    }
    DestroyTokenStrings(lines);
    *LinesCount = linesCount;
    return CM_SUCCESS;    
}

CM_ERROR CreateHistoryFile(HANDLE* HistoryFile, LPTSTR LhsUsername, LPTSTR RhsUsername, DWORD CreationDisposition)
{
    if(NULL == HistoryFile || NULL == LhsUsername || 
        NULL == RhsUsername)
    {
        return CM_INVALID_PARAMETER;
    }

    HANDLE historyFile = INVALID_HANDLE_VALUE;
    CM_ERROR cmError = CM_SUCCESS;
    LPTSTR historyFilePath = NULL;
    __try
    {
        historyFilePath = (LPTSTR)malloc(BUFFER_SIZE);
        if(NULL == historyFilePath)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        cmError = GetHistoryPath(LhsUsername, RhsUsername, historyFilePath, CHARS_COUNT);
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }
        historyFile = CreateFile(historyFilePath, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ, NULL, CreationDisposition, 0, NULL);
        if(INVALID_HANDLE_VALUE == historyFile)
        {
            cmError = CM_EXT_FUNC_FAILURE;
        }
    }
    __finally
    {
        *HistoryFile = historyFile;
        if(NULL != historyFilePath)
        {
            free(historyFilePath);
            historyFilePath = NULL;
        }
    }
    return cmError;
}

CM_ERROR InitFileTransfer(LPTSTR ReceiverUsername, LPTSTR FileToSendName, CM_SIZE* BytesToSendCount,
    LPTSTR FileSendSignalString, LPTSTR SenderUsername)
{
    if(NULL == ReceiverUsername || NULL == FileToSendName || NULL == BytesToSendCount || 
        NULL == FileSendSignalString || NULL == SenderUsername)
    {
        return CM_INVALID_PARAMETER;
    }

    TOKEN_STRINGS* fileSendSignalStringTokenized = NULL;
    CM_ERROR cmError = CreateTokenStrings(&fileSendSignalStringTokenized, 4);
    if(CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(fileSendSignalStringTokenized);
        return cmError;
    }
    cmError = TokenizeString(fileSendSignalStringTokenized, FileSendSignalString, TEXT(' '), 4, BUFFER_SIZE);
    if(CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(fileSendSignalStringTokenized);
        return cmError;
    }

    HRESULT hrError = StringCbCopy(ReceiverUsername, BUFFER_SIZE, fileSendSignalStringTokenized->TokenStrings[1]);
    if(FAILED(hrError))
    {
        DestroyTokenStrings(fileSendSignalStringTokenized);
        return CM_INSUFFICIENT_BUFFER;
    }

    LPTSTR byteCountString = fileSendSignalStringTokenized->TokenStrings[2];
    DWORD bytesToSendCount = 0;
    cmError = ConvertStringToDword(&bytesToSendCount, byteCountString);

    hrError = StringCbCopy(FileToSendName, BUFFER_SIZE, fileSendSignalStringTokenized->TokenStrings[3]);
    if(FAILED(hrError))
    {
        DestroyTokenStrings(fileSendSignalStringTokenized);
        return CM_INSUFFICIENT_BUFFER;
    }
    CM_SIZE fileToSendNameLen = 0;
    hrError = StringCchLength(FileToSendName, CHARS_COUNT, &fileToSendNameLen);
    if(FAILED(hrError))
    {
        DestroyTokenStrings(fileSendSignalStringTokenized);
        return CM_INSUFFICIENT_BUFFER;
    }
    FileToSendName[fileToSendNameLen - 2] = TEXT('\0');
    DestroyTokenStrings(fileSendSignalStringTokenized);

    if(0 == bytesToSendCount)
    {
        USER* receiver = (USER*)malloc(sizeof(USER));
        if(NULL == receiver)
        {
            return CM_NO_MEMORY;
        }
        cmError = UsersHtGetKeyValue(gUsers, ReceiverUsername, receiver);
        if(CM_IS_ERROR(cmError))
        {
            if(NULL != receiver)
            {
                free(receiver);
                receiver = NULL;
            }
            return cmError;
        }
        LPTSTR receivedMessage = (LPTSTR)malloc(BUFFER_SIZE);
        if(NULL == receivedMessage)
        {
            if(NULL != receiver)
            {
                free(receiver);
                receiver = NULL;
            }
            return CM_NO_MEMORY;
        }
        hrError = StringCbCopy(receivedMessage, BUFFER_SIZE, TEXT("Received file: "));
        if(FAILED(hrError))
        {
            if(NULL != receiver)
            {
                free(receiver);
                receiver = NULL;
            }
            if(NULL != receivedMessage)
            {
                free(receivedMessage);
                receivedMessage = NULL;
            }
        }
        hrError = StringCbCat(receivedMessage, BUFFER_SIZE, FileToSendName);
        if(FAILED(hrError))
        {
            if(NULL != receiver)
            {
                free(receiver);
                receiver = NULL;
            }
            if(NULL != receivedMessage)
            {
                free(receivedMessage);
                receivedMessage = NULL;
            }
        }
        hrError = StringCbCat(receivedMessage, BUFFER_SIZE, TEXT(" from "));
        if(FAILED(hrError))
        {
            if(NULL != receiver)
            {
                free(receiver);
                receiver = NULL;
            }
            if(NULL != receivedMessage)
            {
                free(receivedMessage);
                receivedMessage = NULL;
            }
        }
        hrError = StringCbCat(receivedMessage, BUFFER_SIZE, SenderUsername);
        if(FAILED(hrError))
        {
            if(NULL != receiver)
            {
                free(receiver);
                receiver = NULL;
            }
            if(NULL != receivedMessage)
            {
                free(receivedMessage);
                receivedMessage = NULL;
            }
        }
        hrError = StringCbCat(receivedMessage, BUFFER_SIZE, TEXT("\r\n"));
        if(FAILED(hrError))
        {
            if(NULL != receiver)
            {
                free(receiver);
                receiver = NULL;
            }
            if(NULL != receivedMessage)
            {
                free(receivedMessage);
                receivedMessage = NULL;
            }
        }
        cmError = SendStringToUser(receiver, receivedMessage, BUFFER_SIZE);
        if(NULL != receiver)
        {
            free(receiver);
            receiver = NULL;
        }
        if(NULL != receivedMessage)
        {
            free(receivedMessage);
            receivedMessage = NULL;
        }
    }

    *BytesToSendCount = (CM_SIZE)bytesToSendCount;
    return cmError;
}

CM_ERROR SendFileToClient(LPTSTR ReceiverUsername, LPTSTR FileToSendName, CM_SIZE* BytesToSendCount,
    CM_DATA_BUFFER* DataBuffer)
{
    if(NULL == ReceiverUsername || NULL == FileToSendName || NULL == BytesToSendCount
        || NULL == DataBuffer)
    {
        return CM_INVALID_CONNECTION;
    }

    USER* receiverUser = NULL;  
    LPTSTR receiveSignalString = NULL; 
    LPTSTR auxString = NULL;
    CM_DATA_BUFFER* dataBufferToSend = NULL;
    CM_SIZE sentBytesCount = 0;
    CM_ERROR cmError = CM_SUCCESS;
    __try
    {
        receiverUser = (USER*)malloc(sizeof(USER));
        if(NULL == receiverUser)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        cmError = UsersHtGetKeyValue(gUsers, ReceiverUsername, receiverUser);
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }
        receiveSignalString = (LPTSTR)malloc(BUFFER_SIZE);
        if(NULL == receiveSignalString)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        auxString = (LPTSTR)malloc(BUFFER_SIZE);
        if(NULL == auxString)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }

        CM_SIZE bytesToSendCount = *BytesToSendCount;
        if(bytesToSendCount > DataBuffer->DataBufferSize)
        {
            bytesToSendCount = DataBuffer->DataBufferSize;
        }
        HRESULT hrError = StringCbCopy(receiveSignalString, BUFFER_SIZE, FILE_RECEIVE_SIGNAL);
        if(FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbPrintf(auxString, BUFFER_SIZE, TEXT("%s %d "), receiveSignalString, bytesToSendCount);
        if(FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCopy(receiveSignalString, BUFFER_SIZE, auxString);
        if(FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(receiveSignalString, BUFFER_SIZE, FileToSendName);
        if(FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(receiveSignalString, BUFFER_SIZE, TEXT("\r\n"));
        if(FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }

        cmError = CreateDataBuffer(&dataBufferToSend, BUFFER_SIZE);
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }
        cmError = CopyDataIntoBuffer(dataBufferToSend, DataBuffer->DataBuffer, bytesToSendCount); 
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }

        EnterCriticalSection(&receiverUser->CritSection);
        cmError = SendStringToUser(receiverUser, receiveSignalString, BUFFER_SIZE);
        if(CM_IS_ERROR(cmError))
        {
            LeaveCriticalSection(&receiverUser->CritSection);
            __leave;
        }
        cmError = SendDataToClient(receiverUser->Client, dataBufferToSend, &sentBytesCount);
        LeaveCriticalSection(&receiverUser->CritSection);
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }

        for(CM_SIZE i = sentBytesCount; i < DataBuffer->UsedBufferSize; i++) 
        {
            DataBuffer->DataBuffer[i - sentBytesCount] = DataBuffer->DataBuffer[i];
        }
        DataBuffer->UsedBufferSize -= sentBytesCount;
    }
    __finally
    {
        if(NULL != receiverUser)
        {
            free(receiverUser);
            receiverUser = NULL;
        }
        if(NULL != receiveSignalString)
        {
            free(receiveSignalString);
            receiveSignalString = NULL;
        }
        if(NULL != auxString)
        {
            free(auxString);
            auxString = NULL;
        }
        DestroyDataBuffer(dataBufferToSend);
    }

    *BytesToSendCount = *BytesToSendCount - sentBytesCount;
    return cmError;
}

CM_ERROR GetPendingMsgsPath(LPTSTR Username, LPTSTR PendingMsgPath, CM_SIZE CharsCount)
{
    if (NULL == Username || NULL == PendingMsgPath)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_ERROR cmError = GetMsgsFolderPath(PendingMsgPath, CharsCount);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    HRESULT hrError = StringCchCat(PendingMsgPath, CharsCount, TEXT("\\"));
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    hrError = StringCchCat(PendingMsgPath, CharsCount, Username);
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    hrError = StringCchCat(PendingMsgPath, CharsCount, TEXT(".txt"));
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    return CM_SUCCESS;
}

static CM_ERROR GetHistoryPath(LPTSTR LhsUsername, LPTSTR RhsUsername, LPTSTR HistoryPath, CM_SIZE CharsCount)
{
    if(NULL == LhsUsername || NULL == RhsUsername ||
        NULL == HistoryPath)
    {
        return CM_INVALID_PARAMETER;
    }
    LPTSTR historyFolderPath = NULL, historyFilePath = NULL;
    HANDLE historyFile = INVALID_HANDLE_VALUE;
    CM_ERROR cmError = CM_SUCCESS;
    __try
    {
        historyFolderPath = (LPTSTR)malloc(sizeof(TCHAR) * CharsCount);
        if (NULL == historyFolderPath)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        historyFilePath = (LPTSTR)malloc(sizeof(TCHAR) * CharsCount);
        if (NULL == historyFilePath)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }

        cmError = GetHistoryFolderPath(historyFolderPath, CharsCount);
        if (CM_IS_ERROR(cmError))
        {
            return cmError;
        }

        HRESULT hrError = StringCchCopy(historyFilePath, CharsCount, historyFolderPath);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCchCat(historyFilePath, CharsCount, TEXT("\\"));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCchCat(historyFilePath, CharsCount, LhsUsername);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCchCat(historyFilePath, CharsCount, TEXT("-"));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCchCat(historyFilePath, CharsCount, RhsUsername);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCchCat(historyFilePath, CharsCount, TEXT(".txt"));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        BOOLEAN fileExists = CheckFileExistance(historyFilePath);
        if(fileExists)
        {
            __leave;
        }

        hrError = StringCchCopy(historyFilePath, CharsCount, historyFolderPath);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCchCat(historyFilePath, CharsCount, TEXT("\\"));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCchCat(historyFilePath, CharsCount, RhsUsername);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCchCat(historyFilePath, CharsCount, TEXT("-"));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCchCat(historyFilePath, CharsCount, LhsUsername);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCchCat(historyFilePath, CharsCount, TEXT(".txt"));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
    }
    __finally
    {
        if(CM_IS_SUCCESS(cmError))
        {
            HRESULT hrError = StringCchCopy(HistoryPath, CharsCount, historyFilePath);
            if (FAILED(hrError))
            {
                cmError = CM_INSUFFICIENT_BUFFER;
            }
        }
        if(NULL != historyFolderPath)
        {
            free(historyFolderPath);
            historyFolderPath = NULL;
        }
        if(NULL != historyFilePath)
        {
            free(historyFilePath);
            historyFilePath = NULL;
        }
        if(INVALID_HANDLE_VALUE != historyFile)
        {
            CloseHandle(historyFile);
        }
    }
    return cmError;
}

static CM_ERROR GetMsgsFolderPath(LPTSTR MsgFolderPath, CM_SIZE CharsCount)
{
    if (NULL == MsgFolderPath)
    {
        return CM_INVALID_PARAMETER;
    }

    DWORD dError = GetCurrentDirectory(CharsCount, MsgFolderPath);
    if (0 == dError)
    {
        return CM_EXT_FUNC_FAILURE;
    }
    HRESULT hrError = StringCchCat(MsgFolderPath, CharsCount, TEXT("\\PendingMsgs"));
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    return CM_SUCCESS;
}

static CM_ERROR GetHistoryFolderPath(LPTSTR HistoryFolderPath, CM_SIZE CharsCount)
{
    if(NULL == HistoryFolderPath)
    {
        return CM_INVALID_PARAMETER;
    }


    DWORD dError = GetCurrentDirectory(CharsCount, HistoryFolderPath);
    if (0 == dError)
    {
        return CM_EXT_FUNC_FAILURE;
    }
    HRESULT hrError = StringCchCat(HistoryFolderPath, CharsCount, TEXT("\\History"));
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    return CM_SUCCESS;
}

static BOOLEAN CheckFileExistance(LPTSTR FilePath)
{
    if(NULL == FilePath)
    {
        return 0;
    }

    DWORD attributes = GetFileAttributes(FilePath);

    // not directory
     return attributes != INVALID_FILE_ATTRIBUTES && 
            !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}