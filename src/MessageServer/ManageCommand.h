#pragma once

#include "UserManagement.h"
#include "communication_api.h"

#include <Windows.h>

#define COMMAND_ID_ECHO      ((1 << 6) + 1)
#define COMMAND_ID_REGISTER  ((1 << 6) + 2)
#define COMMAND_ID_LOGIN     ((1 << 6) + 3)
#define COMMAND_ID_LOGOUT    ((1 << 6) + 4)
#define COMMAND_ID_MSG       ((1 << 6) + 5)
#define COMMAND_ID_BROADCAST ((1 << 6) + 6)
#define COMMAND_ID_SENDFILE  ((1 << 6) + 7)
#define COMMAND_ID_LIST      ((1 << 6) + 8)
#define COMMAND_ID_EXIT      ((1 << 6) + 9)
#define COMMAND_ID_HISTORY   ((1 << 6) + 10)

#define COMMAND_TOKENS_ECHO      2
#define COMMAND_TOKENS_REGISTER  3
#define COMMAND_TOKENS_LOGIN     3
#define COMMAND_TOKENS_LOGOUT    1
#define COMMAND_TOKENS_MSG       3
#define COMMAND_TOKENS_BROADCAST 2  
#define COMMAND_TOKENS_SENDFILE  3
#define COMMAND_TOKENS_LIST      1
#define COMMAND_TOKENS_EXIT      1
#define COMMAND_TOKENS_HISTORY   3

/*
 * This function does manages the string sent by the user
 * (the command)
 * 
 * Parameters:  __In__ USER* User - the user that sent the command
 *                __In__ LPTSTR Command - the string that represents the command
 *    
 * Returns: a special CM_ERROR code in case the command is wrong - this code will be managed by a function
 *            a CM_ERROR code that doesn't coincide with the special one in case the function failed
 *            CM_SUCCESS if everything is ok
 */
CM_ERROR ManageUserCommand(USER* User, LPTSTR Command);
