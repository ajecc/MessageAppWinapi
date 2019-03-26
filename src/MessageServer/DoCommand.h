#pragma once
#include "UserManagement.h"
#include "communication_api.h"

/*
 * All of these functions actually do the command sent by the user,
 * after they are verified by the functions in the "CheckCommand.h".
 * All of these functions are as thread safe as they can be, but making additional 
 * calls to gUsers->SRWLock might provide more safety in case some user LOGS OUT.
 * 
 * A CM_ERROR code returned in case of failure.
 */

CM_ERROR DoCommandEcho(USER* User, LPTSTR StringToPrint);
CM_ERROR DoCommandRegister(LPTSTR Username, LPTSTR Password);
CM_ERROR DoCommandLogin(USER* User, LPTSTR Username);
CM_ERROR DoCommandLogout(USER* User);
CM_ERROR DoCommandMsg(USER* Sender, LPTSTR ReceiverUsername, LPTSTR MessageToSendNoUsername);
CM_ERROR DoCommandBroadcast(USER* Sender, LPTSTR MessageToSendNoUsername);
CM_ERROR DoCommandList(USER* User);
CM_ERROR DoCommandExit(USER* User);
CM_ERROR DoCommandSendfile(USER* User, LPTSTR Username, LPTSTR FilePath);
CM_ERROR DoCommandHistory(USER* User, LPTSTR OtherUsername, CM_SIZE HistoryCount);
