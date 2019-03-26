#include "stdafx.h"

#include "communication_string_functions.h"

#include <stdlib.h>
#include <strsafe.h>
#include <tchar.h>

CM_ERROR ConvertStringToDataBuffer(CM_DATA_BUFFER* DataBuffer, LPTSTR String,
    CM_SIZE BufferSize)
{
    if (NULL == DataBuffer || NULL == String 
        || 0 == BufferSize || 0 != BufferSize % sizeof(TCHAR))
    {
        return CM_INVALID_PARAMETER;
    }
    CM_SIZE stringSize = 0;
    HRESULT hrError = StringCbLength(String, BufferSize, &stringSize);
    if(FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    return CopyDataIntoBuffer(DataBuffer, (const CM_BYTE*) String, stringSize);
}

CM_ERROR GetFirstWord(LPTSTR FirstWord, LPTSTR String, CM_SIZE WordMaxLen)
{
    if (NULL == FirstWord || NULL == String || 0 == WordMaxLen)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR firstWord = FirstWord;
    LPTSTR string = String;
    const CM_SIZE wordMaxLen = WordMaxLen;

    CM_SIZE firstWordLen = 0;
    for (firstWordLen = 0; firstWordLen < wordMaxLen - 1; ++firstWordLen)
    {
        if (TEXT(' ') == string[firstWordLen] ||
            TEXT('\0') == string[firstWordLen])
        {
            break;
        }
        firstWord[firstWordLen] = string[firstWordLen];
    }
    
    firstWord[firstWordLen] = TEXT('\0');
    
    return CM_SUCCESS;
}

CM_ERROR CreateTokenStrings(TOKEN_STRINGS** TokenStrings, CM_SIZE TokenMaxCount)
{
    if (NULL == TokenStrings || NULL != *TokenStrings ||  0 == TokenMaxCount)
    {
        return CM_INVALID_PARAMETER;
    }
    
    *TokenStrings = (TOKEN_STRINGS*)malloc(sizeof(TOKEN_STRINGS));
    if (NULL == TokenStrings)
    {
        return CM_NO_MEMORY;
    }
    LPTSTR* tokenStrings = (LPTSTR*)malloc(sizeof(LPTSTR) * TokenMaxCount);
    if (NULL == tokenStrings)
    {
        if (NULL != *TokenStrings)
        {
            free(*TokenStrings);
            *TokenStrings = NULL;
        }
        return CM_NO_MEMORY;
    }
    for (CM_SIZE i = 0; i < TokenMaxCount; ++i)
    {
        tokenStrings[i] = NULL;
    }
    (*TokenStrings)->TokenStrings = tokenStrings;
    (*TokenStrings)->TokenMaxCount = TokenMaxCount;
    (*TokenStrings)->TokenCount = 0;
    return CM_SUCCESS;
}

CM_ERROR DestroyTokenStrings(TOKEN_STRINGS* TokenStrings)
{
    if (NULL == TokenStrings)
    {
        return CM_INVALID_PARAMETER;
    }

    for (CM_SIZE i = 0; i < TokenStrings->TokenMaxCount; ++i)
    {
        if (NULL != TokenStrings->TokenStrings[i])
        {
            free(TokenStrings->TokenStrings[i]);
            TokenStrings->TokenStrings[i] = NULL;
        }
    }
    if (NULL != TokenStrings->TokenStrings)
    {
        free(TokenStrings->TokenStrings);
        TokenStrings->TokenStrings = NULL;
    }
    if (NULL != TokenStrings)
    {
        free(TokenStrings);
    }
    return CM_SUCCESS;
}

CM_ERROR TokenizeString(TOKEN_STRINGS* TokenStrings, LPTSTR StringToTokenize,
    TCHAR Token, CM_SIZE TokenCount, CM_SIZE BufferSize)
{
    if (NULL == TokenStrings || NULL == StringToTokenize || TokenCount > TokenStrings->TokenMaxCount)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR* tokenStrings     = TokenStrings->TokenStrings;
    LPTSTR stringToTokenize  = StringToTokenize;
    const TCHAR token         = Token;
    const DWORD tokenCount   = TokenCount;
    const CM_SIZE bufferSize = BufferSize;

    CM_SIZE stringToTokenizeLen = 0;
    HRESULT hrError = StringCchLength(stringToTokenize, bufferSize, &stringToTokenizeLen);
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    LPTSTR currentTokenString = (LPTSTR)malloc(sizeof(TCHAR) * (stringToTokenizeLen + 1));
    if(NULL == currentTokenString)
    {
        return CM_NO_MEMORY;
    }
    CM_SIZE currentToken = 0;
    CM_SIZE currentTokenIndex = 0;
    for (CM_SIZE i = 0; i < stringToTokenizeLen; ++i)
    {
        TCHAR currentChar = stringToTokenize[i];
        if (token != currentChar || currentToken == tokenCount - 1)
        {
            currentTokenString[currentTokenIndex++] = currentChar;
            continue;
        }

        currentTokenString[currentTokenIndex] = TEXT('\0');
        CM_SIZE bufferSizeCurrentToken = sizeof(TCHAR) * (currentTokenIndex + 1);
        if(NULL != tokenStrings[currentToken])
        {
            free(tokenStrings[currentToken]);
            tokenStrings[currentToken] = NULL;
        }
        tokenStrings[currentToken] = (LPTSTR)malloc(bufferSizeCurrentToken);
        if(NULL == tokenStrings[currentToken])
        {
            if(NULL != currentTokenString)
            {
                free(currentTokenString);
                currentTokenString = NULL;
            }
            return CM_NO_MEMORY;
        }
        hrError = StringCbCopy(tokenStrings[currentToken], bufferSizeCurrentToken, currentTokenString);
        if(FAILED(hrError))
        {
            if(NULL != currentTokenString)
            {
                free(currentTokenString);
                currentTokenString = NULL;
            }
            return CM_INSUFFICIENT_BUFFER;
        }
        if (i != stringToTokenizeLen - 1)
        {
            currentToken++;
            currentTokenIndex = 0;
        }
    }
    currentTokenString[currentTokenIndex] = TEXT('\0');
    CM_SIZE bufferSizeCurrentToken = sizeof(TCHAR) * (currentTokenIndex + 1);
    if(NULL != tokenStrings[currentToken])
    {
        free(tokenStrings[currentToken]);
        tokenStrings[currentToken] = NULL;
    }
    tokenStrings[currentToken] = (LPTSTR)malloc(bufferSizeCurrentToken);
    if(NULL == tokenStrings[currentToken])
    {
        if(NULL != currentTokenString)
        {
            free(currentTokenString);
            currentTokenString = NULL;
        }
        return CM_NO_MEMORY;
    }
    hrError = StringCbCopy(tokenStrings[currentToken], bufferSizeCurrentToken, currentTokenString);
    if(NULL != currentTokenString)
    {
        free(currentTokenString);
        currentTokenString = NULL;
    }
    if(FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }

    TokenStrings->TokenCount = currentToken + 1;
    return CM_SUCCESS;
}

CM_ERROR ConvertStringToDword(DWORD* Dword, LPTSTR String)
{
    if (NULL == String || NULL == Dword)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR string = String;
    DWORD64 dwordValue = 0;

    HRESULT hrError = S_OK;

    CM_SIZE lengthString = 0;
    hrError = StringCchLength(string, 10, &lengthString);
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }

    if (string[0] == TEXT('0') && 1 != lengthString)
    {
        return CM_INVALID_PARAMETER;
    }

    for (CM_SIZE i = 0; i < lengthString; ++i)
    {
        TCHAR currentTchar = string[i];
        if (currentTchar > TEXT('9') || currentTchar < TEXT('0'))
        {
            return CM_INVALID_PARAMETER;
        }
        dwordValue = dwordValue * 10 + currentTchar - TEXT('0');
    }
    if (MAXDWORD < dwordValue)
    {
        return CM_INVALID_PARAMETER;
    }

    *Dword = (DWORD)dwordValue;
    return CM_SUCCESS;
}

CM_ERROR ExtractStringWithCrlf(LPTSTR StringWithCrlf, CM_SIZE StringWithCrlfCharsCount, LPTSTR Source, CM_SIZE SourceCharsCount)
{
    if(NULL == StringWithCrlf || NULL == Source)
    {
        return CM_INVALID_PARAMETER;
    }

    CM_SIZE sourceLen = 0;
    HRESULT hrError = StringCchLength(Source, SourceCharsCount, &sourceLen);
    if(FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }

    for(CM_SIZE i = 0; i < sourceLen && i < StringWithCrlfCharsCount; i++)
    {
        StringWithCrlf[i] = Source[i];
        if(TEXT('\n') == StringWithCrlf[i])
        {
            StringWithCrlf[i + 1] = TEXT('\0');
            for(CM_SIZE j = 0; i + 1 <= sourceLen; j++, i++)
            {
                Source[j] = Source[i + 1];
            }
            return CM_SUCCESS;
        }
    }

    StringWithCrlf[0] = TEXT('\0');
    return CM_SUCCESS;
}

BOOLEAN IsSendSignal(LPTSTR String)
{
    if(NULL == String)
    {
        return FALSE;
    }

    TOKEN_STRINGS* tokenString = NULL;
    CM_ERROR cmError = CreateTokenStrings(&tokenString, 2);
    if(CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(tokenString);
        return FALSE;
    }
    cmError = TokenizeString(tokenString, String, TEXT(' '), 2, BUFFER_SIZE);
    if(CM_IS_ERROR(cmError))
    {
        DestroyTokenStrings(tokenString);
        return FALSE;
    }
    BOOLEAN isSendSignal = 0 == _tcscmp(tokenString->TokenStrings[0], FILE_SEND_SIGNAL);
    DestroyTokenStrings(tokenString);
    return isSendSignal;
}
