#pragma once
#include "communication_api.h"

#define ID_CHARS_COUNT 32
#define ID_BUFFER_SIZE    (ID_CHARS_COUNT * sizeof(TCHAR))
#define ID_CHAR_UPPER_BOUND 255


/*
 * This structure represents a user
 * 
 * Each user has its own Critical Section, Client, Username and Id
 * All of these are unique
 */
typedef struct _USER
{
    CRITICAL_SECTION CritSection;
    CM_SERVER_CLIENT* Client;
    LPTSTR Username;
    LPTSTR Id;
} USER, *PUSER;

#define IS_USER_LOGGED_IN(User) (NULL != (User)->Username)

/*
 * Creates an instance of a USER structure 
 * 
 * Parameters:  __Out__ USER** User - the user to be created
 *                __In__ CM_SERVER_CLIENT* Client - the client that corresponds to the user
 *                __In__ LPTSTR Id - the unique Id of the user
 *                
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR CreateUser(USER** User, CM_SERVER_CLIENT* Client, LPTSTR Id);

/*
 * Destroys an instance of a USER structure
 * 
 * Parameters: __Out__ USER* User - the user to be destroyed
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR DestroyUser(USER* User);

/*
 * Sends a string to the user, after being converted into bytes
 * 
 * Parameters:  __In__ USER* User - the receiver of the string
 *                __In__ LPTSTR String - the string to send
 *                __In__ CM_SIZE BufferSize - the size of the string
 *                
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR SendStringToUser(USER* User, LPTSTR String, CM_SIZE BufferSize);

/*
 * Checks if a user identified by a username is logged in
 * 
 * Parameters:  __In__ LPTSTR Username - the username of the user to be checked
 *                __Out__ BOOLEAN* UserLoggedIn - TRUE if the user is logged in and FALSE otherwise
 *                
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR IsUserLoggedInByUsername(LPTSTR Username, BOOLEAN* UserLoggedIn);
