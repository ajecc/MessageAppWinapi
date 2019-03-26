#include "stdafx.h"
#include "DoCommand.h"
#include "FileManagement.h"
#include "UsersHashTable.h"
#include "UserManagement.h"
#include "CommandErrors.h"
#include "CheckCommand.h"
#include <strsafe.h>

static CM_ERROR DoCommandMsgLoggedIn(USER* Receiver, LPTSTR StringToSend);
static CM_ERROR DoCommandMsgLoggedOut(USER* Receiver, LPTSTR StringToSend);
static CM_ERROR AddStringToHistoryFile(LPTSTR SenderUsername, LPTSTR ReceiverUsername, LPTSTR StringToAdd);

CM_ERROR DoCommandEcho(USER* User, LPTSTR StringToPrint)
{
    if (NULL == User || NULL == User->Client || NULL == StringToPrint)
    {
        return CM_INVALID_PARAMETER;
    }
    
    CM_ERROR cmError = CM_SUCCESS;

    _tprintf_s(TEXT("%s"), StringToPrint);

    cmError = SendStringToUser(User, StringToPrint, BUFFER_SIZE);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    
    return CM_SUCCESS;
}

CM_ERROR DoCommandRegister(LPTSTR Username, LPTSTR Password)
{
    if (NULL == Username || NULL == Password)
    {
        return CM_INVALID_PARAMETER;    
    }

    LPTSTR stringToAppend = (LPTSTR)malloc(BUFFER_SIZE);
    if (NULL == stringToAppend)
    {
        return CM_NO_MEMORY;
    }
    
    HRESULT hrError;
    BOOL bError;
    CM_ERROR cmError = CM_SUCCESS;

    HANDLE registerFile = INVALID_HANDLE_VALUE;
    HANDLE pendingMsgFile = INVALID_HANDLE_VALUE;
    __try
    {
        hrError = StringCbCopy(stringToAppend, BUFFER_SIZE, Username);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(stringToAppend, BUFFER_SIZE, TEXT(","));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(stringToAppend, BUFFER_SIZE, Password);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        registerFile = CreateFile(
                                REGISTER_FILE_PATH,                // file name
                                GENERIC_READ | GENERIC_WRITE,    // access mode
                                FILE_SHARE_READ | FILE_SHARE_WRITE,                // share mode
                                NULL,                            // security attributes
                                OPEN_EXISTING,                    // creation disposition
                                FILE_ATTRIBUTE_NORMAL,            // file attribute
                                NULL);                            // template file
        if (INVALID_HANDLE_VALUE == registerFile)
        {
            cmError = CM_EXT_FUNC_FAILURE;
            __leave;
        }
        cmError = AppendStringToFile(registerFile, stringToAppend);
        if (CM_IS_ERROR(cmError))
        {
            __leave;
        }
        cmError = CreatePendingMsgsFile(&pendingMsgFile, Username, CREATE_ALWAYS);
        if (CM_IS_ERROR(cmError))
        {
            __leave;
        }
    }
    __finally
    {
        if (NULL != stringToAppend)
        {
            free(stringToAppend);
            stringToAppend = NULL;
        }
        if (INVALID_HANDLE_VALUE != registerFile)
        {
            bError = CloseHandle(registerFile);
            if (FALSE == bError)
            {
                cmError = CM_EXT_FUNC_FAILURE;
            }
        }
        if (INVALID_HANDLE_VALUE != pendingMsgFile)
        {
            bError = CloseHandle(pendingMsgFile);
            if (FALSE == bError)
            {
                cmError = CM_EXT_FUNC_FAILURE;
            }
        }
    }
    return cmError;
}

CM_ERROR DoCommandLogin(USER* User, LPTSTR Username)
{
    if (NULL == User || NULL == Username)
    {
        return CM_INVALID_PARAMETER;
    }

    USER* user = User;
    LPTSTR username = Username;

    HRESULT hrError;
    CM_ERROR cmError = CM_SUCCESS;

    HANDLE pendingMsgsFile = INVALID_HANDLE_VALUE;
    TOKEN_STRINGS* tokenStrings = NULL;
    LPTSTR stringsPending = NULL;
    LPTSTR userPendingMsgFilePath = NULL;
    __try
    {
        CM_SIZE usernameLenBytes = 0;
        hrError = StringCbLength(username, BUFFER_SIZE, &usernameLenBytes);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        usernameLenBytes += sizeof(TCHAR);
        user->Username = (LPTSTR)malloc(usernameLenBytes);
        if (NULL == user->Username)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        hrError = StringCbCopy(user->Username, usernameLenBytes, username);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        cmError = UsersHtRemoveKey(gUsers, user->Id);
        if (CM_IS_ERROR(cmError))
        {
            __leave;
        }
        cmError = UsersHtSetKeyValue(gUsers, username, user);
        if (CM_IS_ERROR(cmError))
        {
            __leave;
        }
        EnterCriticalSection(&gFileCritSection);
        cmError = CreatePendingMsgsFile(&pendingMsgsFile, username, OPEN_EXISTING);
        if(CM_IS_ERROR(cmError) && ERROR_FILE_NOT_FOUND == GetLastError())
        {
            SetLastError(0);
            cmError = CM_SUCCESS;
            __leave;
        }
        if (CM_IS_ERROR(cmError))
        {
            __leave;
        }
        cmError = CreateTokenStrings(&tokenStrings, CHARS_COUNT); 
        if (CM_IS_ERROR(cmError))
        {
            __leave;
        }
        stringsPending = (LPTSTR)malloc(BUFFER_SIZE);
        if (NULL == stringsPending)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        BOOLEAN reachedEOF = FALSE;
        while (!reachedEOF)
        {
            stringsPending[0] = TEXT('\0');
            cmError = ReadFileLines(pendingMsgsFile, tokenStrings, BUFFER_SIZE / 2, &reachedEOF);
            if (CM_IS_ERROR(cmError))
            {
                __leave;
            }
            for (CM_SIZE i = 0; i < tokenStrings->TokenCount; ++i)
            {
                hrError = StringCchCat(stringsPending, BUFFER_SIZE, tokenStrings->TokenStrings[i]);
                if (FAILED(hrError))
                {
                    cmError = CM_INSUFFICIENT_BUFFER;
                    __leave;
                }
                hrError = StringCchCat(stringsPending, BUFFER_SIZE, TEXT("\r\n"));
                if (FAILED(hrError))
                {
                    cmError = CM_INSUFFICIENT_BUFFER;
                    __leave;
                }
            }
            if(tokenStrings->TokenCount > 0)
            {
                cmError = SendStringToUser(User, stringsPending, BUFFER_SIZE);
                if(CM_IS_ERROR(cmError))
                {
                    __leave;
                }
            }
        }
        userPendingMsgFilePath = (LPTSTR)malloc(BUFFER_SIZE);
        if(NULL == userPendingMsgFilePath)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        cmError = GetPendingMsgsPath(username, userPendingMsgFilePath, CHARS_COUNT);
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }
        if (INVALID_HANDLE_VALUE != pendingMsgsFile)
        {
            CloseHandle(pendingMsgsFile);
            pendingMsgsFile = INVALID_HANDLE_VALUE;
        }
        BOOL bError = DeleteFile(userPendingMsgFilePath);
        if (0 == bError)
        {
            cmError = CM_EXT_FUNC_FAILURE;
            __leave;
        }
    }
    __finally
    {
        LeaveCriticalSection(&gFileCritSection);
        if (INVALID_HANDLE_VALUE != pendingMsgsFile)
        {
            CloseHandle(pendingMsgsFile);
            pendingMsgsFile = INVALID_HANDLE_VALUE;
        }
        if (NULL != stringsPending)
        {
            free(stringsPending);
            stringsPending = NULL;
        }
        if(NULL != userPendingMsgFilePath)
        {
            free(userPendingMsgFilePath);
            userPendingMsgFilePath = NULL;
        }
        DestroyTokenStrings(tokenStrings);
    }
    return cmError;
}

CM_ERROR DoCommandLogout(USER* User)
{
    if (NULL == User)
    {
        return CM_INVALID_PARAMETER;
    }
    CM_ERROR cmError = CM_SUCCESS;

    if (IS_USER_LOGGED_IN(User))
    {
        cmError = UsersHtRemoveKey(gUsers, User->Username);
        free(User->Username);
        User->Username = NULL;
        if (CM_IS_ERROR(cmError))
        {
            return cmError;
        }
        cmError = UsersHtSetKeyValue(gUsers, User->Id, User);
        return cmError;
    }
    return CM_INVALID_COMMAND;
}

CM_ERROR DoCommandMsg(USER* Sender, LPTSTR ReceiverUsername, LPTSTR MessageToSendNoUsername)
{
    if (NULL == Sender || NULL == Sender->Client || NULL == Sender->Username
        || NULL == MessageToSendNoUsername)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR receiverUsername = ReceiverUsername;
    LPTSTR messageToSendNoUsername = MessageToSendNoUsername;

    CM_ERROR cmError = CM_SUCCESS;
    HRESULT hrError;

    BOOLEAN receiverLoggedIn = FALSE;
    cmError = IsUserLoggedInByUsername(receiverUsername, &receiverLoggedIn);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }

    USER* receiver = NULL;
    LPTSTR messageToSend = NULL;
    __try
    {
        receiver = (USER*)malloc(sizeof(USER));
        if (NULL == receiver)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        messageToSend = (LPTSTR)malloc(BUFFER_SIZE);
        if (NULL == messageToSend)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        
        // build the message to send to the receiver
        hrError = StringCbCopy(messageToSend, BUFFER_SIZE, TEXT("Message from "));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(messageToSend, BUFFER_SIZE, Sender->Username);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(messageToSend, BUFFER_SIZE, TEXT(": "));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(messageToSend, BUFFER_SIZE, messageToSendNoUsername);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        if (receiverLoggedIn)
        {
            cmError = UsersHtGetKeyValue(gUsers, receiverUsername, receiver);
            if (CM_IS_ERROR(cmError))
            {
                __leave;
            }
            cmError = DoCommandMsgLoggedIn(receiver, messageToSend);
            if (CM_IS_ERROR(cmError))
            {
                __leave;
            }
        }
        else
        {
            receiver->Username = receiverUsername;
            cmError = DoCommandMsgLoggedOut(receiver, messageToSend);
            if (CM_IS_ERROR(cmError))
            {
                __leave;
            }
        }
        CM_SIZE messageToSendLen = 0;
        hrError = StringCchLength(messageToSend, CHARS_COUNT, &messageToSendLen);
        if(FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        for(CM_SIZE i = 8; i <= messageToSendLen; i++)
        {
            messageToSend[i - 8] = messageToSend[i];
        }
        messageToSend[0] = TEXT('F');
        cmError = AddStringToHistoryFile(Sender->Username, ReceiverUsername, messageToSend);
    }
    __finally
    {
        if (NULL != receiver)
        {
            free(receiver);
            receiver = NULL;
        }
        if (NULL != messageToSend)
        {
            free(messageToSend);
            messageToSend = NULL;
        }
    }
    return cmError;
}

CM_ERROR DoCommandBroadcast(USER* Sender, LPTSTR MessageToSendNoUsername)
{
    if (NULL == Sender || NULL == Sender->Username || NULL == MessageToSendNoUsername)
    {
        return CM_INVALID_PARAMETER;
    }
    
    CM_ERROR cmError = CM_SUCCESS;
    HRESULT hrError;
    
    LPTSTR messageToSend = NULL;
    __try
    {
        messageToSend = (LPTSTR)malloc(BUFFER_SIZE);
        if (NULL == messageToSend)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        hrError = StringCbCopy(messageToSend, BUFFER_SIZE, TEXT("Broadcast from "));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(messageToSend, BUFFER_SIZE, Sender->Username);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(messageToSend, BUFFER_SIZE, TEXT(": "));
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        hrError = StringCbCat(messageToSend, BUFFER_SIZE, MessageToSendNoUsername);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }

        USERS_LIST_NODE* currentNode = NULL;
        AcquireSRWLockShared(&gUsers->SRWLock);
        for (;;)
        {
            cmError = UsersHtParse(gUsers, &currentNode);
            if (CM_IS_ERROR(cmError))
            {
                __leave;
            }
            if (NULL == currentNode)
            {
                break;
            }
            if (0 != _tcscmp(currentNode->Key, Sender->Username))
            {
                cmError = SendStringToUser(currentNode->User, messageToSend, BUFFER_SIZE);
                if (CM_IS_ERROR(cmError))
                {
                    __leave;
                }
            }
        }
    }
    __finally
    {
        ReleaseSRWLockShared(&gUsers->SRWLock);
        if (NULL != messageToSend)
        {
            free(messageToSend);
            messageToSend = NULL;
        }
    }
    return cmError;
}

CM_ERROR DoCommandList(USER* User)
{
    if (NULL == User)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_ERROR cmError = CM_SUCCESS;
    HRESULT hrError = S_OK;

    USERS_LIST_NODE* currentNode = NULL;
    LPTSTR stringToSend = (LPTSTR)malloc(BUFFER_SIZE); 
    if(NULL == stringToSend)
    {
        cmError = CM_NO_MEMORY;
        goto cleanup;
    }
    stringToSend[0] = TEXT('\0');
    AcquireSRWLockShared(&gUsers->SRWLock);
    for (;;)
    {
        cmError = UsersHtParse(gUsers, &currentNode);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        if (NULL == currentNode)
        {
            if (TEXT('\0') != stringToSend[0])
            {
                cmError = SendStringToUser(User, stringToSend, BUFFER_SIZE);
                if(CM_IS_ERROR(cmError))
                {
                    goto cleanup;
                }
            }
            goto cleanup;
        }
        if (IS_USER_LOGGED_IN(currentNode->User))
        {
            CM_SIZE stringToSendSize = 0;
            hrError = StringCbLength(stringToSend, BUFFER_SIZE, &stringToSendSize);
            if(FAILED(hrError))
            {
                cmError = CM_INSUFFICIENT_BUFFER;
                goto cleanup;
            }
            CM_SIZE usernameSize = 0;
            hrError = StringCbLength(currentNode->User->Username, BUFFER_SIZE, &usernameSize);
            if(FAILED(hrError))
            {
                cmError = CM_INSUFFICIENT_BUFFER;
                goto cleanup;
            }
            if (stringToSendSize + usernameSize + 2 * sizeof(TCHAR) >= BUFFER_SIZE - 1)
            {
                cmError = SendStringToUser(User, stringToSend, BUFFER_SIZE);
                if(CM_IS_ERROR(cmError))
                {
                    goto cleanup;
                }
                hrError = StringCbCopy(stringToSend, BUFFER_SIZE, currentNode->User->Username);
                if(FAILED(hrError))
                {
                    cmError = CM_INSUFFICIENT_BUFFER;
                    goto cleanup;
                }
                stringToSend[0] = TEXT('\0');
            } 
            else
            {
                hrError = StringCbCat(stringToSend, BUFFER_SIZE, currentNode->User->Username);
                if(FAILED(hrError))
                {
                    cmError = CM_INSUFFICIENT_BUFFER;
                    goto cleanup;
                }
                hrError = StringCbCat(stringToSend, BUFFER_SIZE, TEXT("\r\n"));
                if(FAILED(hrError))
                {
                    cmError = CM_INSUFFICIENT_BUFFER;
                    goto cleanup;
                }
            }
        }
    }
cleanup:
    ReleaseSRWLockShared(&gUsers->SRWLock);
    if(NULL != stringToSend)
    {
        free(stringToSend);
        stringToSend = NULL;
    }
    return cmError;
}

CM_ERROR DoCommandExit(USER* User)
{
    if(NULL == User)
    {
        return CM_INVALID_PARAMETER;
    }
    CM_ERROR cmError = SendStringToUser(User, EXIT_SIGNAL, BUFFER_SIZE);
    if(CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    cmError = CheckCommandLogout(User);
    if(CM_IS_COMMAND_ERROR(cmError))
    {
        return CM_SUCCESS;
    }
    if(CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    return DoCommandLogout(User);
}

CM_ERROR DoCommandSendfile(USER* User, LPTSTR Username, LPTSTR FilePath)
{
    if(NULL == User || NULL == Username || NULL == FilePath)
    {
        return CM_INVALID_PARAMETER;
    }
    LPTSTR sendSignal = (LPTSTR)malloc(BUFFER_SIZE);
    if(NULL == sendSignal)
    {
        return CM_NO_MEMORY;
    }
    HRESULT hrError = StringCbCopy(sendSignal, BUFFER_SIZE, FILE_SEND_SIGNAL);
    if(FAILED(hrError))
    {
        if(NULL != sendSignal)
        {
            free(sendSignal);
            sendSignal = NULL;
        }
        return CM_INSUFFICIENT_BUFFER;
    }
    hrError = StringCbCat(sendSignal, BUFFER_SIZE, TEXT(" "));
    if(FAILED(hrError))
    {
        if(NULL != sendSignal)
        {
            free(sendSignal);
            sendSignal = NULL;
        }
        return CM_INSUFFICIENT_BUFFER;
    }
    hrError = StringCbCat(sendSignal, BUFFER_SIZE, Username);
    if(FAILED(hrError))
    {
        if(NULL != sendSignal)
        {
            free(sendSignal);
            sendSignal = NULL;
        }
        return CM_INSUFFICIENT_BUFFER;
    }
    hrError = StringCbCat(sendSignal, BUFFER_SIZE, TEXT(" "));
    if(FAILED(hrError))
    {
        if(NULL != sendSignal)
        {
            free(sendSignal);
            sendSignal = NULL;
        }
        return CM_INSUFFICIENT_BUFFER;
    }
    hrError = StringCbCat(sendSignal, BUFFER_SIZE, FilePath); 
    if(FAILED(hrError))
    {
        if(NULL != sendSignal)
        {
            free(sendSignal);
            sendSignal = NULL;
        }
        return CM_INSUFFICIENT_BUFFER;
    }
    CM_ERROR cmError = SendStringToUser(User, sendSignal, BUFFER_SIZE);
    if(NULL != sendSignal)
    {
        free(sendSignal);
        sendSignal = NULL;
    }
    return cmError;
}

CM_ERROR DoCommandHistory(USER* User, LPTSTR OtherUsername, CM_SIZE HistoryCount)
{
    if(NULL == User || NULL == OtherUsername)
    {
        return CM_INVALID_PARAMETER;
    }

    HANDLE historyFile = INVALID_HANDLE_VALUE;
    EnterCriticalSection(&gFileCritSection);
    CM_ERROR cmError = CreateHistoryFile(&historyFile, User->Username, OtherUsername, OPEN_EXISTING);
    if(CM_IS_ERROR(cmError) && ERROR_FILE_NOT_FOUND == GetLastError())
    {
        SetLastError(0);
        CloseHandle(historyFile);
        LeaveCriticalSection(&gFileCritSection);
        return SendStringToUser(User, TEXT("\r\n"), BUFFER_SIZE);
    }
    if(CM_IS_ERROR(cmError))
    {
        CloseHandle(historyFile);
        LeaveCriticalSection(&gFileCritSection);
        return cmError;
    }

    TOKEN_STRINGS* lines = NULL;
    LPTSTR stringsPending = NULL;
    __try
    {
        CM_SIZE linesCount = 0;
        cmError = CountLinesOfFile(historyFile, &linesCount);
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }

        LARGE_INTEGER dontMove;
        dontMove.u.HighPart = dontMove.u.LowPart = 0;
        BOOL bError = SetFilePointerEx(historyFile, dontMove, NULL, FILE_BEGIN);
        if(0 == bError)
        {
            cmError = CM_EXT_FUNC_FAILURE;
            __leave;
        }
        cmError = CreateTokenStrings(&lines, CHARS_COUNT);
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }

        stringsPending = (LPTSTR)malloc(BUFFER_SIZE);
        if(NULL == stringsPending)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        BOOLEAN reachedEof = FALSE;
        while(!reachedEof)
        {
            stringsPending[0] = TEXT('\0');
            cmError = ReadFileLines(historyFile, lines, BUFFER_SIZE / 2, &reachedEof);
            if(CM_IS_ERROR(cmError))
            {
                __leave;
            }
            for(CM_SIZE i = 0; i < lines->TokenCount; i++)
            {
                if(linesCount <= HistoryCount)
                {
                    HRESULT hrError = StringCbCat(stringsPending, BUFFER_SIZE, lines->TokenStrings[i]);
                    if(FAILED(hrError))
                    {
                        cmError = CM_INSUFFICIENT_BUFFER;
                        __leave;
                    }
                    hrError = StringCbCat(stringsPending, BUFFER_SIZE, TEXT("\r\n"));
                    if(FAILED(hrError))
                    {
                        cmError = CM_INSUFFICIENT_BUFFER;
                        __leave;
                    }
                }
                linesCount--;
            }
            if(lines->TokenCount > 0)
            {
                cmError = SendStringToUser(User, stringsPending, BUFFER_SIZE);
            }
        }
    }
    __finally
    {
        CloseHandle(historyFile);
        DestroyTokenStrings(lines);
        LeaveCriticalSection(&gFileCritSection);
        if(NULL != stringsPending)
        {
            free(stringsPending);
            stringsPending = NULL;
        }
    }
    return cmError;
}

static CM_ERROR DoCommandMsgLoggedIn(USER* Receiver, LPTSTR StringToSend)
{
    if (NULL == Receiver || NULL == Receiver->Client || NULL == StringToSend)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_ERROR cmError = CM_SUCCESS;
    HRESULT hrError = S_OK;


    CM_SIZE stringToSendByteLen = 0;
    hrError = StringCbLength(StringToSend, BUFFER_SIZE, &stringToSendByteLen);
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    cmError = SendStringToUser(Receiver, StringToSend, BUFFER_SIZE);
    return cmError;
}

static CM_ERROR DoCommandMsgLoggedOut(USER* Receiver, LPTSTR StringToSend)
{
    if (NULL == Receiver || NULL == StringToSend)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR receiverUsername = Receiver->Username;

    CM_ERROR cmError = CM_SUCCESS;

    HANDLE pendingMsgFile = INVALID_HANDLE_VALUE;
    __try
    {
        cmError = CreatePendingMsgsFile(&pendingMsgFile, receiverUsername, CREATE_NEW);
        if (CM_IS_ERROR(cmError) && ERROR_FILE_EXISTS == GetLastError())
        {
            SetLastError(0);
            cmError = CreatePendingMsgsFile(&pendingMsgFile, receiverUsername, OPEN_EXISTING);
        }
        if(CM_IS_ERROR(cmError))
        {
            __leave;
        }
        cmError = AppendStringToFile(pendingMsgFile, StringToSend);
        if (CM_IS_ERROR(cmError))
        {
            __leave;
        }
    }
    __finally
    {
        if (INVALID_HANDLE_VALUE != pendingMsgFile)
        {
            CloseHandle(pendingMsgFile);
        }
    }
    return cmError;
}

static CM_ERROR AddStringToHistoryFile(LPTSTR SenderUsername, LPTSTR ReceiverUsername, LPTSTR StringToAdd)
{
    if(NULL == SenderUsername || NULL == ReceiverUsername ||
        NULL == StringToAdd)
    {
        return CM_INVALID_PARAMETER;
    }
    CM_ERROR cmError = CM_SUCCESS;
    HANDLE historyFile = INVALID_HANDLE_VALUE;
    EnterCriticalSection(&gFileCritSection);
    cmError = CreateHistoryFile(&historyFile, SenderUsername, ReceiverUsername, OPEN_EXISTING);
    if(CM_IS_ERROR(cmError) && ERROR_FILE_NOT_FOUND == GetLastError())
    {
        SetLastError(0);
        cmError = CreateHistoryFile(&historyFile, SenderUsername, ReceiverUsername, CREATE_NEW);
        if(CM_IS_ERROR(cmError))
        {
            LeaveCriticalSection(&gFileCritSection);
            return cmError;
        }
    }
    cmError = AppendStringToFile(historyFile, StringToAdd);
    if(INVALID_HANDLE_VALUE != historyFile)
    {
        CloseHandle(historyFile);
    }
    LeaveCriticalSection(&gFileCritSection);
    return cmError;
}