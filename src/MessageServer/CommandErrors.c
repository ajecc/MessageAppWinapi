#include "stdafx.h"
#include "CommandErrors.h"
#include "client_communication_api.h"

VOID ManageError(USER* User, CM_ERROR Error)
{
    if (CM_IS_SUCCESS(Error))
    {
        return;
    }
    if (!CM_IS_COMMAND_ERROR(Error))
    {
        _tprintf_s(TEXT("Unexpected error: err-code=0x%X is not a valid CM_COMMAND_ERROR\r\n"), Error);
        return;
    }

    switch (Error)
    {
    case CM_INVALID_USERNAME:
        SendStringToUser(User, TEXT("Error: Invalid username\r\n"), BUFFER_SIZE);
        break;
    case CM_INVALID_PASSWORD:
        SendStringToUser(User, TEXT("Error: Invalid password\r\n"), BUFFER_SIZE);
        break;
    case CM_ALREADY_REGISTERED:
        SendStringToUser(User, TEXT("Error: Username already registered\r\n"), BUFFER_SIZE);
        break;
    case CM_WEAK_PASSWORD:
        SendStringToUser(User, TEXT("Error: Password too weak\r\n"), BUFFER_SIZE);
        break;
    case CM_ALREADY_LOGGED_IN:
        SendStringToUser(User, TEXT("Error: User already logged in\r\n"), BUFFER_SIZE);
        break;
    case CM_INVALID_USERNAME_PASSWORD_COMB:
        SendStringToUser(User, TEXT("Error: Invalid username/password combination\r\n"), BUFFER_SIZE);
        break;
    case CM_ANOTHER_ALREADY_LOGGED_IN:
        SendStringToUser(User, TEXT("Error: Another user already logged in\r\n"), BUFFER_SIZE);
        break;
    case CM_NOT_LOGGED_IN:
        SendStringToUser(User, TEXT("Error: No user currently logged in\r\n"), BUFFER_SIZE);
        break;
    case CM_NOT_REGISTERED:
        SendStringToUser(User, TEXT("Error: No such user\r\n"), BUFFER_SIZE);
        break;
    case CM_ANOTHER_NOT_LOGGED_IN:
        SendStringToUser(User, TEXT("Error: User not active\r\n"), BUFFER_SIZE);
        break;
    case CM_FILE_NOT_FOUND:
        SendStringToUser(User, TEXT("Error: File not found\r\n"), BUFFER_SIZE);
        break;
    case CM_INVALID_COMMAND:
        SendStringToUser(User, TEXT("Error: No such command\r\n"), BUFFER_SIZE);
    default:
        break;
    }
}
