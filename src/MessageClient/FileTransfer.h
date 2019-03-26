#pragma once
#include "communication_data.h"
#include "client_communication_api.h"

#include <Windows.h>

/*
 * Checks if a string contains a Receive Signal used for transferring a file
 * 
 * Parameters:  __In__ LPTSTR String - the string to be analyzed
 *
 * Returns: FALSE if the function fails or if the String doesn't contain a Receive Signal
 *            and TRUE otherwise
 */
BOOLEAN IsReceiveSignal(LPTSTR String);

/*
 * This function initializes the writing to a receive file from another client
 * 
 * Parameters:  __Out__ HANDLE* ReceivedFile - the handle to the file that is being received
 *                __Out__ CM_SIZE* WriteByteCount - the number of bytes to be written to the file
 *                __In__ LPTSTR ReceiveSignalString - the string that contains the Receive Signal
 *                
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR InitWritingToReceivedFile(HANDLE* ReceivedFile, CM_SIZE* WriteByteCount, LPTSTR ReceiveSignalString);

/*
 * This function writes some bytes from DataBuffer to a received file
 * 
 * Parameters:  __Out__ HANDLE* ReceivedFile - the handle to the file that is being received,
 *                                                that will be closed after the function exits
 *                __Out__ CM_SIZE* WriteToFileByteCount - the number of bytes to be written,
 *                                                        updated after the function exits
 *                __Out__ CM_DATA_BUFFER* DataBuffer - the data buffer from which the bytes to write
 *                                                to file will be extracted and removed from the buffer
 *    
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR WriteToReceivedFile(HANDLE* ReceivedFile, CM_SIZE* WriteToFileByteCount, CM_DATA_BUFFER* DataBuffer);

typedef struct _SEND_FILE_ARGS 
{
    CM_CLIENT* Client;
    LPTSTR FilePath;
    LPTSTR ReceiverUsername;
    CRITICAL_SECTION* CritSection;
} SEND_FILE_ARGS, *P_SEND_FILE_ARGS;

/*
 * Creates an instance of SEND_FILE_ARGS, that will be passed to SendFile function
 * which will create a new thread
 * 
 * Parameters:  __Out__ SEND_FILE_ARGS** SendFileArgs - the arguments that will be created
 *                __In__ CM_CLIENT* Client - the client that sends the file
 *                __In__ LPTSTR SendFileSignal - the string that contains a Send Signal, from which
 *                                            the rest of the structure members will be extracted
 *    
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR CreateSendFileArgs(SEND_FILE_ARGS** SendFileArgs, CM_CLIENT* Client, LPTSTR SendFileSignal);

/*
 * Destroys an instance of SEND_FILE_ARGS
 * 
 * Parameters:  __Out__ SEND_FILE_ARGS* SendFileArgs - the instance to be destroyed
 *    
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR DestroySendFileArgs(SEND_FILE_ARGS* SendFileArgs);

/*
 * The function that shall be used to start a new thread to 
 * send a file to the server, that will be redirected to another client
 * 
 * Parameters:  __Out__ P_SEND_FILE_ARGS SendFileArgs - the arguments of this function:
                CM_CLIENT* Client; - the client that sends the file to the server
                LPTSTR FilePath; - the path of the file to be sent
                LPTSTR ReceiverUsername; - the username of the receiver
                CRITICAL_SECTION* CritSection; - a unique critical section that will not
                                                allow 2 files to be sent at the same time
                This Parameter will be destroyed when the function exits
 *    
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR WINAPI SendFile(P_SEND_FILE_ARGS SendFileArgs);