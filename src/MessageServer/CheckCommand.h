#pragma once
#include "UserManagement.h"
#include "communication_api.h"

/*
 * All of these functions check if the command sent by the user is valid
 * If it is not, a special CM_ERROR code will be returned by the functions
 * which indicate how the user entered a wrong command.
 * This error code will then be managed by a function (ManageError) and 
 * an appropriate message will be sent to the user
 * 
 *  In case of failure, the functions return an appropriate CM_ERROR code,
 *  that doesn't coincide with the special error codes 
 */

CM_ERROR CheckCommandRegister(USER* User, LPTSTR Username, LPTSTR Password); 
CM_ERROR CheckCommandLogin(USER* User, LPTSTR Username, LPTSTR Password);
CM_ERROR CheckCommandLogout(USER* User);
CM_ERROR CheckCommandMsg(USER* Sender, LPTSTR ReceiverUsername);
CM_ERROR CheckCommandBroadcast(USER* Sender);
CM_ERROR CheckCommandSendfile(USER* Sender, LPTSTR ReceiverUsername);
CM_ERROR CheckCommandHistory(USER* CurrentUser, LPTSTR OtherUsername);