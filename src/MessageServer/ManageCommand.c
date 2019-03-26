#include "stdafx.h"

#include "MessageServer.h"
#include "ManageCommand.h"
#include "CheckCommand.h"
#include "DoCommand.h"
#include "UserManagement.h"
#include "CommandErrors.h"
#include "FileManagement.h"
#include "UsersHashTable.h"
#include "communication_string_functions.h"

#include <strsafe.h>

static CM_ERROR GetCommandId(DWORD* CommandId, LPTSTR Command);

static CM_ERROR ManageCommandEcho(USER* User, LPTSTR* CommandTokenized);
static CM_ERROR ManageCommandRegister(USER* User, LPTSTR* CommandTokenized);
static CM_ERROR ManageCommandLogin(USER* User, LPTSTR* CommandTokenized);
static CM_ERROR ManageCommandLogout(USER* User);
static CM_ERROR ManageCommandMsg(USER* Sender, LPTSTR* CommandTokenized);
static CM_ERROR ManageCommandBroadcast(USER* User, LPTSTR* CommandTokenized);
static CM_ERROR ManageCommandList(USER* User);
static CM_ERROR ManageCommandExit(USER* User);
static CM_ERROR ManageCommandSendfile(USER* User, LPTSTR* CommandTokenized);
static CM_ERROR ManageCommandHistory(USER* User, LPTSTR* CommandTokenized);


 CM_ERROR ManageUserCommand(USER* User, LPTSTR Command)
{
    if (NULL == User || NULL == Command)
    {
        return CM_INVALID_PARAMETER;
    }
    USER* user = User;
    LPTSTR command = Command;

    LPTSTR commandFirstWord = (LPTSTR)malloc(sizeof(TCHAR) * (WORD_MAX_LEN + 1));
    if (NULL == commandFirstWord)
    {
        return CM_NO_MEMORY;
    }
    CM_ERROR cmError = GetFirstWord(commandFirstWord, command, WORD_MAX_LEN);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    DWORD commandId = 0; 
    cmError = GetCommandId(&commandId, commandFirstWord);
    if (CM_IS_ERROR(cmError))
    {
        if (NULL != commandFirstWord)
        {
            free(commandFirstWord);
            commandFirstWord = NULL;
        }
        return cmError;
    }
    if (NULL != commandFirstWord)
    {
        free(commandFirstWord);
        commandFirstWord = NULL;
    }

    TOKEN_STRINGS* commandTokenized = NULL;
    cmError = CreateTokenStrings(&commandTokenized, TOKEN_MAX_CNT);
    if (CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(commandTokenized);
        return cmError;
    }

    switch (commandId)
    {
    case COMMAND_ID_ECHO:
    {
        cmError = TokenizeString(commandTokenized,
            command, TEXT(' '), COMMAND_TOKENS_ECHO, BUFFER_SIZE);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        if(COMMAND_TOKENS_ECHO != commandTokenized->TokenCount)
        {
            cmError = CM_INVALID_COMMAND;
            goto cleanup;
        }
        cmError = ManageCommandEcho(user, commandTokenized->TokenStrings);
    } break;
    case COMMAND_ID_REGISTER:
    {
        cmError = TokenizeString(commandTokenized,
            command, TEXT(' '), COMMAND_TOKENS_REGISTER, BUFFER_SIZE);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        if(COMMAND_TOKENS_REGISTER != commandTokenized->TokenCount)
        {
            cmError = CM_INVALID_COMMAND;
            goto cleanup;
        }
        cmError = ManageCommandRegister(user, commandTokenized->TokenStrings);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        cmError = SendStringToUser(User, TEXT("Success\r\n"), BUFFER_SIZE);
    } break;
    case COMMAND_ID_LOGIN:
    {
        cmError = TokenizeString(commandTokenized,
            command, TEXT(' '), COMMAND_TOKENS_LOGIN, BUFFER_SIZE);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        if(COMMAND_TOKENS_LOGIN != commandTokenized->TokenCount)
        {
            cmError = CM_INVALID_COMMAND;
            goto cleanup;
        }
        cmError = ManageCommandLogin(user, commandTokenized->TokenStrings);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        cmError = SendStringToUser(User, TEXT("Success\r\n"), BUFFER_SIZE);
    } break;
    case COMMAND_ID_LOGOUT:
    {
        cmError = ManageCommandLogout(user);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        cmError = SendStringToUser(User, TEXT("Success\r\n"), BUFFER_SIZE);
    } break;
    case COMMAND_ID_MSG:
    {
        cmError = TokenizeString(commandTokenized,
            command, TEXT(' '), COMMAND_TOKENS_MSG, BUFFER_SIZE);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        if(COMMAND_TOKENS_MSG != commandTokenized->TokenCount)
        {
            cmError = CM_INVALID_COMMAND;
            goto cleanup;
        }
        AcquireSRWLockShared(&gUsers->SRWLock);
        cmError = ManageCommandMsg(user, commandTokenized->TokenStrings);
        ReleaseSRWLockShared(&gUsers->SRWLock);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        SendStringToUser(User, TEXT("Success\r\n"), BUFFER_SIZE);
    } break;
    case COMMAND_ID_BROADCAST:
    {
        cmError = TokenizeString(commandTokenized,
            command, TEXT(' '), COMMAND_TOKENS_BROADCAST, BUFFER_SIZE);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        if(COMMAND_TOKENS_BROADCAST != commandTokenized->TokenCount)
        {
            cmError = CM_INVALID_COMMAND;
            goto cleanup;
        }
        AcquireSRWLockShared(&gUsers->SRWLock);
        cmError = ManageCommandBroadcast(user, commandTokenized->TokenStrings);
        ReleaseSRWLockShared(&gUsers->SRWLock);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        cmError = SendStringToUser(User, TEXT("Success\r\n"), BUFFER_SIZE);
    } break;
    case COMMAND_ID_SENDFILE:
    {
        cmError = TokenizeString(commandTokenized,
            command, TEXT(' '), COMMAND_TOKENS_SENDFILE, BUFFER_SIZE);
        if(CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        if(COMMAND_TOKENS_SENDFILE != commandTokenized->TokenCount)
        {
            cmError = CM_INVALID_COMMAND;
            goto cleanup;
        }
        cmError = ManageCommandSendfile(User, commandTokenized->TokenStrings);
    } break;
    case COMMAND_ID_LIST:
    {
        cmError = ManageCommandList(user);
    } break;
    case COMMAND_ID_EXIT:
    {
        cmError = ManageCommandExit(user);
    } break;
    case COMMAND_ID_HISTORY:
    {
        cmError = TokenizeString(commandTokenized,
            command, TEXT(' '), COMMAND_TOKENS_HISTORY, BUFFER_SIZE);
        if (CM_IS_ERROR(cmError))
        {
            goto cleanup;
        }
        if(COMMAND_TOKENS_HISTORY != commandTokenized->TokenCount)
        {
            cmError = CM_INVALID_COMMAND;
            goto cleanup;
        }
        cmError = ManageCommandHistory(user, commandTokenized->TokenStrings);
    } break;
    default:
    {
        cmError = CM_INVALID_COMMAND;
    } 
    }

cleanup:
    DestroyTokenStrings(commandTokenized);
    return cmError;
}

static CM_ERROR GetCommandId(DWORD* CommandId, LPTSTR Command)
{
    if (NULL == CommandId || NULL == Command)
    {
        return CM_INVALID_PARAMETER;
    }

    if (0 == _tcsncmp(Command, TEXT("echo"), 5))
    {
        *CommandId = COMMAND_ID_ECHO;
        return CM_SUCCESS;
    }
    if (0 == _tcsncmp(Command, TEXT("register"), 9))
    {
        *CommandId = COMMAND_ID_REGISTER;
        return CM_SUCCESS;
    }
    if (0 == _tcsncmp(Command, TEXT("login"), 6))
    {
        *CommandId = COMMAND_ID_LOGIN;
        return CM_SUCCESS;
    }
    if (0 == _tcsncmp(Command, TEXT("logout\r\n"), 9))
    {
        *CommandId = COMMAND_ID_LOGOUT;
        return CM_SUCCESS;
    }
    if (0 == _tcsncmp(Command, TEXT("msg"), 8))
    {
        *CommandId = COMMAND_ID_MSG;
        return CM_SUCCESS;
    }
    if (0 == _tcsncmp(Command, TEXT("broadcast"), 10))
    {
        *CommandId = COMMAND_ID_BROADCAST;
        return CM_SUCCESS;
    }
    if (0 == _tcsncmp(Command, TEXT("sendfile"), 9))
    {
        *CommandId = COMMAND_ID_SENDFILE;
        return CM_SUCCESS;
    }
    if (0 == _tcsncmp(Command, TEXT("list\r\n"), 7))
    {
        *CommandId = COMMAND_ID_LIST;
        return CM_SUCCESS;
    }
    if (0 == _tcsncmp(Command, TEXT("exit\r\n"), 7))
    {
        *CommandId = COMMAND_ID_EXIT;
        return CM_SUCCESS;
    }
    if (0 == _tcsncmp(Command, TEXT("history"), 8))
    {
        *CommandId = COMMAND_ID_HISTORY;
        return CM_SUCCESS;
    }

    return CM_INVALID_COMMAND;
}

static CM_ERROR ManageCommandEcho(USER* User, LPTSTR* CommandTokenized)
{
    if (NULL == User || NULL == User->Client || NULL == CommandTokenized)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_ERROR cmError = DoCommandEcho(User, CommandTokenized[1]);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    return CM_SUCCESS;
}

static CM_ERROR ManageCommandRegister(USER* User, LPTSTR* CommandTokenized)
{
    if (NULL == User || NULL == CommandTokenized)
    {
        return CM_INVALID_PARAMETER;
    }
    LPTSTR username = CommandTokenized[1];
    LPTSTR password = CommandTokenized[2];

    CM_ERROR cmError = CheckCommandRegister(User, username, password);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    return DoCommandRegister(username, password);
}

static CM_ERROR ManageCommandLogin(USER* User, LPTSTR* CommandTokenized)
{
    if (NULL == User || NULL == CommandTokenized)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR username = CommandTokenized[1];
    LPTSTR password = CommandTokenized[2];


    CM_ERROR cmError = CheckCommandLogin(User, username, password);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    return DoCommandLogin(User, username);
}

static CM_ERROR ManageCommandLogout(USER* User)
{
    if (NULL == User)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_ERROR cmError = CheckCommandLogout(User);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    return DoCommandLogout(User);
}

static CM_ERROR ManageCommandMsg(USER* Sender, LPTSTR* CommandTokenized)
{
    if (NULL == Sender || NULL == CommandTokenized)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR receiverUsername = CommandTokenized[1];
    LPTSTR messageToSendNoUsername = CommandTokenized[2];

    CM_ERROR cmError = CheckCommandMsg(Sender, receiverUsername);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    return DoCommandMsg(Sender, receiverUsername, messageToSendNoUsername);
}

static CM_ERROR ManageCommandBroadcast(USER* User, LPTSTR* CommandTokenized)
{
    if (NULL == User || NULL == CommandTokenized)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR messageToBroadcastNoUsername = CommandTokenized[1];
    
    CM_ERROR cmError = CheckCommandBroadcast(User);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    return DoCommandBroadcast(User, messageToBroadcastNoUsername);
}

static CM_ERROR ManageCommandList(USER* User)
{
    if (NULL == User)
    {
        return CM_INVALID_PARAMETER;
    }
    return DoCommandList(User);
}

static CM_ERROR ManageCommandExit(USER* User)
{
    if(NULL == User)
    {
        return CM_INVALID_PARAMETER;
    }
    return DoCommandExit(User);
}

CM_ERROR ManageCommandSendfile(USER* User, LPTSTR* CommandTokenized)
{
    if(NULL == User || NULL == CommandTokenized)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR receiverUsername = CommandTokenized[1];
    LPTSTR filePath = CommandTokenized[2];

    CM_ERROR cmError = CheckCommandSendfile(User, receiverUsername);
    if(CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    return DoCommandSendfile(User, receiverUsername, filePath);
}

static CM_ERROR ManageCommandHistory(USER* User, LPTSTR* CommandTokenized)
{
    if(NULL == User || NULL == CommandTokenized)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR otherUsername = CommandTokenized[1];
    LPTSTR historyCountString = CommandTokenized[2];
    CM_SIZE historyCountStringLen = 0;
    HRESULT hrError = StringCchLength(historyCountString, CHARS_COUNT, &historyCountStringLen);
    if(FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    historyCountString[historyCountStringLen - 2] = TEXT('\0');

    DWORD historyCount = 0;
    CM_ERROR cmError = ConvertStringToDword(&historyCount, historyCountString);
    if(CM_IS_ERROR(cmError))
    {
        return CM_INVALID_COMMAND;
    }
    cmError = CheckCommandHistory(User, otherUsername);
    if(CM_IS_ERROR(cmError))
    {
        return cmError;
    }

    return DoCommandHistory(User, otherUsername, historyCount);
}

