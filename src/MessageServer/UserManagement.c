#include "stdafx.h"
#include "UserManagement.h"
#include "UsersHashTable.h"

#include <strsafe.h>

CM_ERROR CreateUser(USER** User, CM_SERVER_CLIENT* Client, LPTSTR Id)
{
    if (NULL == User || NULL != *User || NULL == Client || NULL == Id)
    {
        return CM_INVALID_PARAMETER;
    }

    USER* user = (USER*)malloc(sizeof(USER));
    if (NULL == user)
    {
        return CM_NO_MEMORY;
    }
    user->Id = (LPTSTR)malloc(ID_BUFFER_SIZE);
    if (NULL == user->Id)
    {
        return CM_NO_MEMORY;
    }
    HRESULT hrError = StringCbCopy(user->Id, ID_BUFFER_SIZE, Id);
    if (FAILED(hrError))
    {
        if (NULL != user->Id)
        {
            free(user->Id);
            user->Id = NULL;
        }
        if (NULL != user)
        {
            free(user);
            user = NULL;
        }
        return CM_INSUFFICIENT_BUFFER;
    }
    user->Client = Client;
    user->Username = NULL;
    InitializeCriticalSection(&user->CritSection);


    *User = user;
    return CM_SUCCESS;
}

CM_ERROR DestroyUser(USER* User)
{
    if (NULL == User)
    {
        return CM_INVALID_PARAMETER;
    }
    if (NULL != User->Username)
    {
        free(User->Username);
        User->Username = NULL;
    }    
    if (NULL != User->Id)
    {
        free(User->Id);
        User->Id = NULL;
    }
    DeleteCriticalSection(&User->CritSection);
    AbandonClient(User->Client);
    free(User);
    return CM_SUCCESS;
}

CM_ERROR SendStringToUser(USER* User, LPTSTR String, CM_SIZE BufferSize)
{
    if (NULL == User || NULL == User->Client || NULL == String)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_SERVER_CLIENT* client = User->Client;
    LPTSTR string = String;
    const CM_SIZE bufferSize = BufferSize;


    CM_DATA_BUFFER* dataBuffer = NULL;
    CM_ERROR cmError = CreateDataBuffer(&dataBuffer, bufferSize);
    if (CM_IS_ERROR(cmError))
    {
        DestroyDataBuffer(dataBuffer);
        return cmError;
    }
    cmError = ConvertStringToDataBuffer(dataBuffer, string, bufferSize);
    if (CM_IS_ERROR(cmError))
    {
        DestroyDataBuffer(dataBuffer);
        return cmError;
    }
    CM_SIZE sentBytes = 0;
    EnterCriticalSection(&User->CritSection);
    cmError = SendDataToClient(client, dataBuffer, &sentBytes);
    LeaveCriticalSection(&User->CritSection);
    DestroyDataBuffer(dataBuffer);
    return cmError;
}

CM_ERROR IsUserLoggedInByUsername(LPTSTR Username, BOOLEAN* UserLoggedIn)
{
    if (NULL == UserLoggedIn)
    {
        return CM_INVALID_PARAMETER;
    }
    if (NULL == Username)
    {
        *UserLoggedIn = FALSE;
        return CM_SUCCESS;
    }
    return UsersHtHasKey(gUsers, Username, UserLoggedIn);
}
