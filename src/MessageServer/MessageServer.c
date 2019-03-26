#include "stdafx.h"

#include "MessageServer.h"
#include "ManageCommand.h"
#include "UsersHashTable.h"
#include "FileManagement.h"
#include "UserManagement.h"
#include "CommandErrors.h"
#include "communication_api.h"

#include <strsafe.h>
#include <process.h>

static volatile LONG gConnectionsCount = 0;
static CM_ERROR CALLBACK MainClientFunction(PTP_CALLBACK_INSTANCE Instance, PUSER User, PTP_WORK Work);
static CM_ERROR GetMaxClients(int Argc, LPTSTR Argv[], LONG* MaxClients);

int _tmain(int Argc, LPTSTR Argv[])
{
    CM_SERVER* server = NULL;
    gUsers = NULL;
    TP_CALLBACK_ENVIRON callbackEnviron;
    InitializeThreadpoolEnvironment(&callbackEnviron);
    InitializeCriticalSection(&gFileCritSection);
    LPTSTR lastId = NULL;

    SetupFiles(); 
    LONG maxClients = 0;
    CM_ERROR cmError = GetMaxClients(Argc, Argv, &maxClients);
    if (CM_IS_ERROR(cmError))
    {
        _tprintf_s(TEXT("Error: invalid maximum number of connections\r\n")); 
        return EXIT_FAILURE;
    }

    EnableCommunicationModuleLogger();
    cmError = InitCommunicationModule();
    if (CM_IS_ERROR(cmError))
    {
        _tprintf_s(TEXT("Unexpected error: InitCommunicationModule failed with err-code=0x%X!\r\n"), cmError); /// recheck this before shipping
        goto cleanup;
    }

    cmError = CreateServer(&server);
    if (CM_IS_ERROR(cmError))
    {
        _tprintf_s(TEXT("Unexpected error: CreateServer failed with err-code=0x%X!\r\n"), cmError); /// recheck this before shipping
        UninitCommunicationModule();
    }

    cmError = UsersHtCreate(&gUsers, CC_HASH_TABLE_MAX_SIZE < maxClients ? 
                                    CC_HASH_TABLE_MAX_SIZE : maxClients);
    if (CM_IS_ERROR(cmError))
    {
        UsersHtDestroy(&gUsers);
        return EXIT_FAILURE;
    }

    lastId = (LPTSTR)malloc(ID_BUFFER_SIZE);
    if(NULL == lastId)
    {
        cmError = CM_NO_MEMORY;
        goto cleanup;
    }
    CM_SIZE lastIdLen = ID_BUFFER_SIZE / sizeof(TCHAR) - 1;
    lastId[lastIdLen] = TEXT('\0');
    lastId[0] = TEXT('#');
    for (CM_SIZE i = 1; i < lastIdLen; ++i)
    {
        lastId[i] = (TCHAR)1;
    }
    CM_SIZE currentIdIndex = 1;

    _tprintf_s(TEXT("Success\r\n")); 
    while (TRUE)
    {
        CM_SERVER_CLIENT* newClient = NULL;
        cmError = AwaitNewClient(server, &newClient);
        if (CM_IS_ERROR(cmError))
        {
            cmError = CM_SUCCESS;
            _tprintf_s(TEXT("Unexpected error: AwaitNewClient failed with err-code=x%X\r\n"), cmError);
            continue;
        }
        if (lastId[currentIdIndex] == (TCHAR)255)
        {
            currentIdIndex++;
        }
        lastId[currentIdIndex]++;
        PUSER user = NULL;
        cmError = CreateUser(&user, newClient, lastId);
        if (CM_IS_ERROR(cmError))
        {
            DestroyUser(user);
            continue;
        }
        cmError = UsersHtSetKeyValue(gUsers, user->Id, user);
        if (CM_IS_ERROR(cmError))
        {
            DestroyUser(user);
            continue;
        }

        if(gConnectionsCount >= maxClients)
        {
            CM_SIZE tries = 0;
            do
            {
                cmError = SendStringToUser(user, TEXT("Error: maximum concurrent connection count reached\r\n"), BUFFER_SIZE);
                tries++;
            } while (CM_IS_ERROR(cmError) && tries < 10); 
            continue;
        }


        PTP_WORK workObject = CreateThreadpoolWork((PTP_WORK_CALLBACK) MainClientFunction, user, &callbackEnviron);
        SubmitThreadpoolWork(workObject);
        InterlockedIncrement(&gConnectionsCount);
    }

cleanup:
    if(NULL != lastId)
    {
        free(lastId);
        lastId = NULL;
    }
    DestroyServer(server);
    DeleteCriticalSection(&gFileCritSection);
    UninitCommunicationModule();
    UsersHtDestroy(&gUsers);

    return cmError;
}

static CM_ERROR CALLBACK MainClientFunction(PTP_CALLBACK_INSTANCE Instance, PUSER User, PTP_WORK Work)
{
    (void)Instance;
    (void)Work;
    if (NULL == User)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_ERROR cmError = CM_INTERNAL_ERROR;
    for(CM_SIZE sendTries = 0; sendTries < 10 && CM_IS_ERROR(cmError); sendTries++)
    {
        cmError = SendStringToUser(User, TEXT("Successful connection\r\n"), BUFFER_SIZE);
    }
    if(CM_IS_ERROR(cmError))
    {
        return cmError;
    }

    PUSER user = User;
    CM_SERVER_CLIENT* client = User->Client;

    CM_DATA_BUFFER* receivedDataFromClient = NULL;
    CM_DATA_BUFFER* dataBufferCat = NULL;
    LPTSTR commandStringCat = NULL;
    LPTSTR commandString = NULL;
    LPTSTR fileReceiverUsername = NULL;
    LPTSTR fileToSendName = NULL;
    CM_SIZE fileBytesToSend = 0;

    cmError = CreateDataBuffer(&receivedDataFromClient,  BUFFER_SIZE);
    if (CM_IS_ERROR(cmError))
    {
        goto cleanup;
    }
    cmError = CreateDataBuffer(&dataBufferCat, CAT_BUFFER_SIZE);
    if (CM_IS_ERROR(cmError))
    {
        goto cleanup;
    }
    commandStringCat = (LPTSTR)malloc(CAT_BUFFER_SIZE);
    if (NULL == commandStringCat)
    {
        cmError =  CM_NO_MEMORY;
        goto cleanup;
    }
    commandStringCat[0] = TEXT('\0');
    commandString = (LPTSTR)malloc(BUFFER_SIZE);
    if(NULL == commandString)
    {
        cmError = CM_NO_MEMORY;
        goto cleanup;
    }
    fileReceiverUsername = (LPTSTR)malloc(BUFFER_SIZE);
    if(NULL == fileReceiverUsername)
    {
        cmError = CM_NO_MEMORY;
        goto cleanup;
    }
    fileToSendName = (LPTSTR)malloc(BUFFER_SIZE);
    if(NULL == fileToSendName)
    {
        cmError = CM_NO_MEMORY;
        goto cleanup;
    }

    while (TRUE)
    {
        CM_SIZE receivedByteCountFromClient = 0;
        if(fileBytesToSend == 0 || dataBufferCat->UsedBufferSize < fileBytesToSend)
        {
            cmError = ReceiveDataFromClient(client,
                receivedDataFromClient, &receivedByteCountFromClient);
            if (CM_IS_ERROR(cmError) || 0 == receivedByteCountFromClient)
            {
                break;
            }
        }
        for(CM_SIZE i = 0; i < receivedDataFromClient->UsedBufferSize; i++)
        {
            dataBufferCat->DataBuffer[dataBufferCat->UsedBufferSize++] = receivedDataFromClient->DataBuffer[i];
        }
        receivedDataFromClient->UsedBufferSize = 0;
        if(0 != fileBytesToSend)
        {
            if(dataBufferCat->UsedBufferSize < fileBytesToSend)
            {
                continue;
            }
            cmError = SendFileToClient(fileReceiverUsername, fileToSendName, &fileBytesToSend, dataBufferCat);
            if(CM_IS_ERROR(cmError))
            {
                continue;
            }
        }
        CopyMemory(commandStringCat, dataBufferCat->DataBuffer, dataBufferCat->UsedBufferSize
            - dataBufferCat->UsedBufferSize % sizeof(TCHAR));
        commandStringCat[dataBufferCat->UsedBufferSize / sizeof(TCHAR)] = TEXT('\0');
        cmError = ExtractStringWithCrlf(commandString, CHARS_COUNT, commandStringCat, CAT_CHARS_COUNT);
        if(CM_IS_ERROR(cmError))
        {
            continue;
        }
        CM_SIZE commandStringLen = 0;
        StringCbLength(commandString, BUFFER_SIZE, &commandStringLen);
        for(CM_SIZE i = 0; i < dataBufferCat->UsedBufferSize - commandStringLen; i++)
        {
            dataBufferCat->DataBuffer[i] = dataBufferCat->DataBuffer[i + commandStringLen];
        }
        dataBufferCat->UsedBufferSize -= commandStringLen;

        if(IsSendSignal(commandString))
        {
            InitFileTransfer(fileReceiverUsername, fileToSendName, &fileBytesToSend, commandString, user->Username);
            continue;
        }
        if(TEXT('\0') == commandString[0])
        {
            continue;
        }

        cmError = ManageUserCommand(user, commandString);
        if(0 == _tcscmp(commandString, TEXT("exit\r\n")))
        {
            goto cleanup;
        }
        if (CM_IS_ERROR(cmError) && !CM_IS_COMMAND_ERROR(cmError))
        {
            SendStringToUser(user, TEXT("Unexpected error: maybe your command is wrong...\r\n"), BUFFER_SIZE);
            continue;
        }
        ManageError(user, cmError);
    }
    UsersHtRemoveKey(gUsers, User->Username);
    UsersHtRemoveKey(gUsers, User->Id);
    DestroyUser(User);
cleanup:
    if (NULL != commandStringCat)
    {
        free(commandStringCat);
        commandStringCat = NULL;
    }
    if(NULL != commandString)
    {
        free(commandString);
        commandString = NULL;
    }
    if(NULL != fileReceiverUsername)
    {
        free(fileReceiverUsername);
        fileReceiverUsername = NULL;
    }
    if(NULL != fileToSendName)
    {
        free(fileToSendName);
        fileToSendName = NULL;
    }
    DestroyDataBuffer(receivedDataFromClient);
    DestroyDataBuffer(dataBufferCat);
    if (CM_IS_ERROR(cmError))
    {
        //_tprintf_s(TEXT("Unexpected error: Something failed. The CM_ERROR err-code=0x=%X. GetLastError err-code=0x%X\n"), 
        //    cmError, GetLastError());
    }
    InterlockedDecrement(&gConnectionsCount);
    return cmError;
}

static CM_ERROR GetMaxClients(int Argc, LPTSTR Argv[], LONG* MaxClients)
{
    if (2 != Argc)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR maxClientsString = Argv[1];
    DWORD maxClientsValue = 0;

    CM_ERROR cmError = CM_SUCCESS;

    cmError = ConvertStringToDword(&maxClientsValue, maxClientsString);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }

    if((LONG)maxClientsValue < 0)
    {
        return CM_INTERNAL_ERROR;
    }
    *MaxClients = (LONG)maxClientsValue;
    return CM_SUCCESS;
}
