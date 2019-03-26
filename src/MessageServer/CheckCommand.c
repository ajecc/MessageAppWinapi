#include "stdafx.h"
#include "CheckCommand.h"
#include "FileManagement.h"
#include "UserManagement.h"
#include "CommandErrors.h"
#include <strsafe.h>


static CM_ERROR IsUserRegistered(LPTSTR Username, BOOLEAN* UserRegistered);
static CM_ERROR GetUsername(LPTSTR RegisterString, LPTSTR Username, CM_SIZE BufferSize);
static CM_ERROR GetPassword(LPTSTR RegisterString, LPTSTR Password, CM_SIZE BufferSize);
static CM_ERROR CheckUsername(LPTSTR Username);
static CM_ERROR CheckPassword(LPTSTR Password);


CM_ERROR CheckCommandRegister(USER* User, LPTSTR Username, LPTSTR Password)
{
    if(NULL == User || NULL == Username || NULL == Password)
    {
        return CM_INVALID_PARAMETER;
    }
    BOOLEAN userLoggedIn = FALSE;
    CM_ERROR cmError = IsUserLoggedInByUsername(User->Username, &userLoggedIn);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    if (TRUE == userLoggedIn)
    {
        return CM_ALREADY_LOGGED_IN;
    }
    cmError = CheckUsername(Username);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    cmError = CheckPassword(Password);
    return cmError;
}

CM_ERROR CheckCommandLogin(USER* User, LPTSTR Username, LPTSTR Password)
{
    if (NULL == User || NULL == Username || NULL == Password)
    {
        return CM_INVALID_PARAMETER;
    }
    CM_ERROR cmError = CM_SUCCESS;

    if (IS_USER_LOGGED_IN(User))
    {
        return CM_ALREADY_LOGGED_IN;
    }
    BOOLEAN userLoggedIn = FALSE;
    cmError = IsUserLoggedInByUsername(Username, &userLoggedIn);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    if (TRUE == userLoggedIn)
    {
        return CM_ANOTHER_ALREADY_LOGGED_IN;
    }

    HANDLE registerFile = CreateFile(
                            REGISTER_FILE_PATH,                // file name
                            GENERIC_READ,                    // access mode
                            FILE_SHARE_READ,                // share mode
                            NULL,                            // security attributes
                            OPEN_EXISTING,                    // creation disposition
                            FILE_ATTRIBUTE_NORMAL,            // file attribute
                            NULL);                            // template file
    if (INVALID_HANDLE_VALUE == registerFile)
    {
        return CM_EXT_FUNC_FAILURE;
    }

    LPTSTR usernameFileName = NULL, passwordFileName = NULL;
    TOKEN_STRINGS* lines = NULL;
    __try
    {
        usernameFileName = (LPTSTR)malloc(BUFFER_SIZE);
        if (NULL == usernameFileName)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        passwordFileName = (LPTSTR)malloc(BUFFER_SIZE);
        if (NULL == passwordFileName)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        cmError = CreateTokenStrings(&lines, CHARS_COUNT);
        if (CM_IS_ERROR(cmError))
        {
            __leave;
        }
        
        BOOLEAN reachedEof = FALSE;
        while (!reachedEof)
        {
            cmError = ReadFileLines(registerFile, lines, BUFFER_SIZE - 5 * sizeof(TCHAR), &reachedEof);
            if (CM_IS_ERROR(cmError))
            {
                __leave;
            }
            for (CM_SIZE i = 0; i < lines->TokenCount; ++i)
            {
                cmError = GetUsername(lines->TokenStrings[i], usernameFileName, BUFFER_SIZE);
                if (CM_IS_ERROR(cmError))
                {
                    __leave;
                }
                cmError = GetPassword(lines->TokenStrings[i], passwordFileName, BUFFER_SIZE);
                if (CM_IS_ERROR(cmError))
                {
                    __leave;
                }
                if (0 == _tcscmp(usernameFileName, Username))
                {
                    HRESULT hrError = StringCbCat(passwordFileName, BUFFER_SIZE, TEXT("\n"));
                    if(FAILED(hrError))
                    {
                        cmError = CM_INSUFFICIENT_BUFFER;
                        __leave;
                    }
                    if (0 != _tcscmp(passwordFileName, Password))
                    {
                        cmError = CM_INVALID_USERNAME_PASSWORD_COMB;
                        __leave;
                    }
                    __leave;
                }
            }
        }
        cmError = CM_INVALID_USERNAME_PASSWORD_COMB;
    }
    __finally
    {
        if (NULL != usernameFileName)
        {
            free(usernameFileName);
            usernameFileName = NULL;
        }
        if (NULL != passwordFileName)
        {
            free(passwordFileName);
            passwordFileName = NULL;
        }
        DestroyTokenStrings(lines);
        CloseHandle(registerFile); 
    }
    return cmError;
}

CM_ERROR CheckCommandLogout(USER* User)
{
    if (NULL == User)
    {
        return CM_INVALID_PARAMETER;
    }
    BOOLEAN userLoggedIn = FALSE;
    CM_ERROR cmError = IsUserLoggedInByUsername(User->Username, &userLoggedIn);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    if (!userLoggedIn)
    {
        return CM_NOT_LOGGED_IN;
    }
    return CM_SUCCESS;
}

CM_ERROR CheckCommandMsg(USER* Sender, LPTSTR ReceiverUsername)
{
    if (NULL == Sender || NULL == ReceiverUsername)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR senderUsername = Sender->Username;

    BOOLEAN userLoggedIn = FALSE;
    BOOLEAN userRegistered = FALSE;
    CM_ERROR cmError = IsUserLoggedInByUsername(senderUsername, &userLoggedIn);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    if (!userLoggedIn)
    {
        return CM_NOT_LOGGED_IN;
    }
    cmError = IsUserRegistered(ReceiverUsername, &userRegistered);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    if (!userRegistered)
    {
        return CM_NOT_REGISTERED;
    }
    return CM_SUCCESS;
}

CM_ERROR CheckCommandBroadcast(USER* Sender)
{
    if (NULL == Sender)
    {
        return CM_INVALID_PARAMETER;
    }
    if (!IS_USER_LOGGED_IN(Sender))
    {
        return CM_NOT_LOGGED_IN;
    }
    return CM_SUCCESS;
}

CM_ERROR CheckCommandSendfile(USER* Sender, LPTSTR ReceiverUsername)
{
    if(NULL == Sender || NULL == ReceiverUsername)
    {
        return CM_INVALID_PARAMETER;
    }

    if(!IS_USER_LOGGED_IN(Sender))
    {
        return CM_NOT_LOGGED_IN;
    }
    BOOLEAN isUserRegistered = FALSE;
    CM_ERROR cmError = IsUserRegistered(ReceiverUsername, &isUserRegistered);
    if(!isUserRegistered)
    {
        return CM_NOT_REGISTERED;
    }
    BOOLEAN isUserLoggedInbyUsername = FALSE;
    cmError = IsUserLoggedInByUsername(ReceiverUsername, &isUserLoggedInbyUsername);
    if(!isUserLoggedInbyUsername)
    {
        return CM_ANOTHER_NOT_LOGGED_IN;
    }
    return CM_SUCCESS;
}

CM_ERROR CheckCommandHistory(USER* CurrentUser, LPTSTR OtherUsername)
{
    if(NULL == CurrentUser || NULL == OtherUsername)
    {
        return CM_INVALID_PARAMETER;
    }

    if(!IS_USER_LOGGED_IN(CurrentUser))
    {
        return CM_NOT_LOGGED_IN;
    }
    BOOLEAN otherUserRegistered = TRUE;
    CM_ERROR cmError = IsUserRegistered(OtherUsername, &otherUserRegistered);
    if(CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    if(!otherUserRegistered)
    {
        return CM_NOT_REGISTERED;
    }
    return CM_SUCCESS;
}

static CM_ERROR IsUserRegistered(LPTSTR Username, BOOLEAN* UserRegistered)
{
    if (NULL == Username || NULL == UserRegistered)
    {
        return CM_INVALID_PARAMETER;
    }

    *UserRegistered = FALSE;

    CM_ERROR cmError = CM_SUCCESS;
    HANDLE registerFile = NULL;
    registerFile = CreateFile(
                            REGISTER_FILE_PATH,                // file name
                            GENERIC_READ,                    // access mode
                            FILE_SHARE_READ,                // share mode
                            NULL,                            // security attributes
                            OPEN_EXISTING,                    // creation disposition
                            FILE_ATTRIBUTE_NORMAL,            // file attribute
                            NULL);                            // template file
    if (INVALID_HANDLE_VALUE == registerFile)
    {
        return CM_INTERNAL_ERROR;
    }

    TOKEN_STRINGS* lines = NULL;
    LPTSTR usernameLine = NULL;
    BOOLEAN reachedEof = FALSE;
    __try
    {
        cmError = CreateTokenStrings(&lines, CHARS_COUNT);
        if (CM_IS_ERROR(cmError))
        {
            __leave;
        }
        usernameLine = (LPTSTR)malloc(BUFFER_SIZE);
        if (NULL == usernameLine)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        while (!reachedEof)
        {
            cmError = ReadFileLines(registerFile, lines, BUFFER_SIZE - 5 * sizeof(TCHAR), &reachedEof);
            if (CM_IS_ERROR(cmError))
            {
                __leave;
            }
            for (CM_SIZE i = 0; i < lines->TokenCount; ++i)
            {
                cmError = GetUsername(lines->TokenStrings[i], usernameLine, BUFFER_SIZE);
                if (CM_IS_ERROR(cmError))
                {
                    __leave;
                }
                if (0 == _tcscmp(usernameLine, Username))
                {
                    *UserRegistered = TRUE;
                    __leave;
                }
            }
        }
        __leave;
    }
    __finally
    {
        DestroyTokenStrings(lines);
        if (NULL != usernameLine)
        {
            free(usernameLine);
            usernameLine = NULL;
        }
        CloseHandle(registerFile); 
    }
    return cmError;
}

static CM_ERROR GetUsername(LPTSTR RegisterString, LPTSTR Username, CM_SIZE BufferSize)
{
    if (NULL == RegisterString || NULL == Username)
    {
        return CM_INVALID_PARAMETER;
    }

    TOKEN_STRINGS* registerStringTokenized = NULL;
    CM_ERROR cmError = CreateTokenStrings(&registerStringTokenized, 2);
    if (CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(registerStringTokenized);
        return cmError;
    }
    cmError = TokenizeString(registerStringTokenized, RegisterString, TEXT(','), 2, BufferSize);
    if (CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(registerStringTokenized);
        return cmError;
    }

    HRESULT hrError = StringCbCopy(Username, BufferSize, registerStringTokenized->TokenStrings[0]);
    DestroyTokenStrings(registerStringTokenized);
    if(FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    return CM_SUCCESS;
}

static CM_ERROR GetPassword(LPTSTR RegisterString, LPTSTR Password, CM_SIZE BufferSize)
{
    if (NULL == RegisterString || NULL == Password)
    {
        return CM_INVALID_PARAMETER;
    }

    TOKEN_STRINGS* registerStringTokenized = NULL;
    CM_ERROR cmError = CreateTokenStrings(&registerStringTokenized, 2);
    if (CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(registerStringTokenized);
        return cmError;
    }
    cmError = TokenizeString(registerStringTokenized, RegisterString, TEXT(','), 2, BufferSize);
    if (CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(registerStringTokenized);
        return cmError;
    }

    HRESULT hrError = StringCbCopy(Password, BufferSize, registerStringTokenized->TokenStrings[1]);
    DestroyTokenStrings(registerStringTokenized);
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    return CM_SUCCESS;
}

static CM_ERROR CheckUsername(LPTSTR Username)
{
    if (NULL == Username)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_SIZE usernameLen = 0;
    HRESULT hrError = StringCchLength(Username, CHARS_COUNT, &usernameLen);
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }

    for (CM_SIZE i = 0; i < usernameLen; ++i)
    {
        if (FALSE == IsCharAlphaNumeric(Username[i]))
        {
            return CM_INVALID_USERNAME;
        }
    }

    BOOLEAN userRegistered = FALSE;
    CM_ERROR cmError = IsUserRegistered(Username, &userRegistered);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    if (userRegistered)
    {
        return CM_ALREADY_REGISTERED;
    }
    return CM_SUCCESS;
}

static CM_ERROR CheckPassword(LPTSTR Password)
{
    if (NULL == Password)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_SIZE passwordLen = 0;
    HRESULT hrError = StringCchLength(Password, CHARS_COUNT, &passwordLen);
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    passwordLen -= 2;
    if (passwordLen < 5)
    {
        return CM_WEAK_PASSWORD;
    }

    BOOLEAN alphaNumericFlag = FALSE, uppercaseFlag = FALSE, noInvalidCharsFlag = TRUE;
    for (CM_SIZE i = 0; i < passwordLen; ++i)
    {
        TCHAR currentChar = Password[i];
        if (TEXT(' ') == currentChar || TEXT(',') == currentChar)
        {
            noInvalidCharsFlag = FALSE;
            break;
        }
        if (FALSE == IsCharAlphaNumeric(currentChar))
        {
            alphaNumericFlag = TRUE;
            continue;
        }
        if (TRUE == IsCharUpper(currentChar))
        {
            uppercaseFlag = TRUE;
        }
    }
    if (alphaNumericFlag && uppercaseFlag && noInvalidCharsFlag)
    {
        return CM_SUCCESS;
    }
    if (!noInvalidCharsFlag)
    {
        return CM_INVALID_PASSWORD;
    }
    return CM_WEAK_PASSWORD;
}


