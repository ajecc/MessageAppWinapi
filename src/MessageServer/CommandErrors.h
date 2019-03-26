#pragma once
#include "UserManagement.h"
#include "communication_api.h"

/*
 * This header serves the purpose of providing an error code
 * for the commands entered by the user in case it got the command wrong
 * and sending an appropriate message to the user.
 */

#define MAKE_COMMAND_ERROR_CODE(Severity, ErrorValue) (((Severity) << 30) + \
                                                      ((Severity) << 29)  + \
                                                      (ErrorValue))
#define CM_IS_COMMAND_ERROR(ErrorValue) (((ErrorValue) >> 29) == 0b11)

#define CM_INVALID_USERNAME               MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0x1)
#define CM_INVALID_PASSWORD               MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0x2)
#define CM_ALREADY_REGISTERED             MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0x3)
#define CM_WEAK_PASSWORD                  MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0x4)
#define CM_ALREADY_LOGGED_IN              MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0x5)
#define CM_INVALID_USERNAME_PASSWORD_COMB MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0x6)
#define CM_ANOTHER_ALREADY_LOGGED_IN      MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0x7)
#define CM_NOT_LOGGED_IN                  MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0x8)
#define CM_NOT_REGISTERED                 MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0x9)
#define CM_ANOTHER_NOT_LOGGED_IN          MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0xA)
#define CM_FILE_NOT_FOUND                 MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0xB)
#define CM_INVALID_COMMAND                MAKE_COMMAND_ERROR_CODE(CM_ERROR_SEVERITY, 0xC)


VOID ManageError(USER* User, CM_ERROR Error);
