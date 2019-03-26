#pragma once

#include <Windows.h>
#include "communication_error.h"
#include "communication_data.h"

#define CHARS_COUNT         (1 << 15) 
#define BUFFER_SIZE      (CHARS_COUNT * sizeof(TCHAR)) 
#define CAT_CHARS_COUNT  (CHARS_COUNT * (1 << 6))
#define CAT_BUFFER_SIZE  (CAT_CHARS_COUNT * sizeof(TCHAR))
#define WORD_MAX_LEN      10
#define TOKEN_MAX_CNT     4

#define EXIT_SIGNAL TEXT("$EXIT SIGNAL$\r\n")
#define FILE_SEND_SIGNAL TEXT("$FILE_SEND_SIGNAL$")
#define FILE_RECEIVE_SIGNAL TEXT("$FILE_RECEIVE_SIGNAL$")

/*
 * Safe function to convert a string to a data buffer
 * 
 * Parameters: __Out__ CM_DATA_BUFFER* DataBuffer - the data buffer in which the converted string will lie in
 *               __In__ LPTSTR String - the string to be converted
 *               __In__ CM_SIZE BufferSize - the buffer size of the string
 *               
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR ConvertStringToDataBuffer(CM_DATA_BUFFER* DataBuffer, LPTSTR String,
    CM_SIZE BufferSize);

/*
 * Gets the first word of before a Space (' ') of a string 
 * 
 * Parameters: __Out__ LPTSTR FirstWord - the first word of the string
 *               __In__ LPTSTR String - the string from which the first word will be extracted
 *               __In__ CM_SIZE WordMaxLen - the maximum length of the first word
 *               
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR GetFirstWord(LPTSTR FirstWord, LPTSTR String, CM_SIZE WordMaxLen);

typedef struct _TOKEN_STRINGS
{
    LPTSTR* TokenStrings;
    CM_SIZE TokenMaxCount;
    CM_SIZE TokenCount;
} TOKEN_STRINGS;

/*
 * Creates an instance of TOKEN_STRINGS structure that is used to hold a tokenized string
 *
 * Parameters:  __Out__ TOKEN_STRINGS** TokenStrings - the structure that will hold some tokens
 *              __In__ CM_SIZE TokenMaxCount - the maximum amount of tokens that this instance can hold
 *              
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR CreateTokenStrings(TOKEN_STRINGS** TokenStrings, CM_SIZE TokenMaxCount);

/*
 * Destroys an instance of TOKEN_STRING structure and frees every token
 * 
 * Parameters:  __Out__ TOKEN_STRINGS* TokenStrings - the instance to be destroyed
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR DestroyTokenStrings(TOKEN_STRINGS* TokenStrings);

/*
 * This is the function that tokenizes a string and puts the tokenized strings
 * in a TOKEN _STRINGS* instance. This function also allocates memory for the tokens,
 * memory that will be freed by the DestroyTokenStrings function
 * 
 * Parameters:  __Out__ TOKEN_STRINGS* TokenStrings - the instance that will hold the tokens
 *                __In__ LPTSTR StringToTokenize - the string to tokenize
 *                __In__ TCHAR Token - the character by which the string is tokenized
 *                __In__ CM_SIZE TokenCount - the number of tokens to be held by TokenStrings
 *                __In__ CM_SIZE BufferSize - the maximum buffer size of a token
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR TokenizeString(TOKEN_STRINGS* TokenStrings, LPTSTR StringToTokenize,
    TCHAR Token, CM_SIZE TokenCount, CM_SIZE BufferSize);

/*
 * Converts a string to DWORD
 *
 * Parameters:  __Out__ DWORD* Dword - the converted DWORD
 *                __In__ LPTSTR String - the string to convert
 *                 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR ConvertStringToDword(DWORD* Dword, LPTSTR String);

/*
 * Extracts the first string that contains CRLF out of a given string,
 * then removes this string from the given string
 * If there is no CRLF substring in the given string, an empty string
 * is returned and the function exits successfully 

 * Parameters:  __Out__ LPTSTR StringWithCrlf - the first string with CRLF
 *                __In__ CM_SIZE StringWithCrlfCharsCount - the size of the string with CRLF,
 *                                                            in characters
 *                __In__ LPTSTR Source - the string from which the string with CRLF is extracted
 *                __In__ CM_SIZE SourceCharsCount - the size of the Source, in characters
 *                
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR ExtractStringWithCrlf(LPTSTR StringWithCrlf, CM_SIZE StringWithCrlfCharsCount, 
    LPTSTR Source, CM_SIZE SourceCharsCount);

/*
 * Checks if a string contains a Send Signal used for transferring a file
 * 
 * Parameters:  __In__ LPTSTR String - the string to be analyzed
 *
 * Returns: FALSE if the function fails or if the String doesn't contain a Send Signal
 *            and TRUE otherwise
 */
BOOLEAN IsSendSignal(LPTSTR String);