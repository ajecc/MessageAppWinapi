#pragma once

#include "communication_string_functions.h"


#define REGISTER_FILE_PATH TEXT("C:\\registration.txt")

/*
 * This function sets up the register file,
 * the directory where the pending messages will be stored,
 * the directory where the history between users will be stored.
 * 
 * Returns a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR SetupFiles(void);

/*
 * This function reads the lines of a file and stores them in a
 * TOKEN_STRINGS structure, where each token represents a line
 * This function needs to be called until EOF is reached if reading 
 * the whole file is wanted.
 * 
 * Parameters:  __In__ HANDLE File - the handle to the file to be read
 *                __Out__ TOKEN_STRINGS* Lines - the lines read 
 *                __In__ BufferSize - the amount of bytes to be read from the file
 *                                    in case a line exceeds this buffer size, no line will
 *                                    be read and the function will be stuck at that line.
 *                                    The last line read that doesn't end with CRLF will not be read,
 *                                    even though the BufferSize "touched" it
 *                __Out__ BOOLEAN* ReachedEof - FALSE if EOF is not reached and TRUE otherwise
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR ReadFileLines(HANDLE File, TOKEN_STRINGS* Lines, CM_SIZE BufferSize, BOOLEAN* ReachedEof);

/*
 * Appends a string at the end of a file
 * 
 * Parameters:  __In__ HANDLE File - the file to be appended to
 *                __In__ LPTSTR String - the string to append
 *    
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR AppendStringToFile(HANDLE File, LPTSTR String);

/*
 * Clears a file, leaving it empty (the file is not deleted)
 * 
 * Parameters:  __In__ HANDLE File - the file to be cleared
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR ClearFile(HANDLE File);

/*
 * Counts the lines of a file
 * 
 * Parameters:  __In__ HANDLE File - the file whose line will be counted
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR CountLinesOfFile(HANDLE File, CM_SIZE* LinesCount);

/*
 * Creates a file in the directory that is supposed to hold the pending messages.
 * This file will hold the holding messages of a user identified by a username
 * 
 * Parameters:  __Out__ HANDLE* PendingMsgsFile - the pending messages file
 *                __In__ LPTSTR Username - the username for which the file is created
 *                __In__ DWORD CreationDisposition - the same king of creation disposition 
 *                                                    that is used by the CreateFile function
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR CreatePendingMsgsFile(HANDLE* PendingMsgsFile, LPTSTR Username, DWORD CreationDisposition);

/*
 * Gets the path to the pending messages file of a user identified by its username
 *    
 * Parameters:  __In__ LPTSTR Username - the username of the user we want the pending messages file path
 *                __Out__ LPTSTR PendingMsgPath - the path to the pending messages file
 *                __In__ CM_SIZE CharsCount - the number of characters in the path
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR GetPendingMsgsPath(LPTSTR Username, LPTSTR PendingMsgPath, CM_SIZE CharsCount);

/*
 * Creates a file in which the history of messages between 2 users will be held
 * 
 * Parameters:  __Out__ HANDLE* HistoryFile - the corresponding history file handle
 *                __In__ LPTSTR LhsUsername - the username of the first user
 *                __In__ LPTSTR RhsUsername - the username of the second user
 *                __In__ DWORD CreationDisposition - the same king of creation disposition 
 *                                                    that is used by the CreateFile function
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR CreateHistoryFile(HANDLE* HistoryFile, LPTSTR LhsUsername, LPTSTR RhsUsername, DWORD CreationDisposition);

/*
 * Initializes the receiver username, the name of the file to be sent and the number of bytes to send
 * corresponding to a certain file transfer.
 * All of these will be extracted from a file with a send signal
 * 
 * Parameters:  __Out__ LPTSTR ReceiverUsername - the username of the user which will receive the file
 *                __Out__ LPTSTR FileToSendName - the name of the file to be sent
 *                __Out__ CM_SIZE* BytesToSendCount - the number of bytes to be sent
 *                __In__ LPTSTR FileSendSignalString - the string which contains the information for the 
 *                                                    file transfer
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR InitFileTransfer(LPTSTR ReceiverUsername, LPTSTR FileToSendName, CM_SIZE* BytesToSendCount, LPTSTR FileSendSignalString, LPTSTR SenderUsername);

/*
 * Sends the bytes from a file to the receiver of that file
 * 
 * Parameters:  __In__ LPTSTR ReceiverUsername - the username of the receiver
 *                __In__ LPTSTR FileToSendName - the name of the file that is sent to the user
 *                __Out__ CM_SIZE* BytesToSendCount - how many bytes the server is supposed to send.
 *                                                    After the bytes are sent, this variable will hold
 *                                                    the number of bytes that still need to be sent
 *                __Out__ CM_DATA_BUFFER* DataBuffer - the data buffer from which the bytes of the file 
 *                                                     will be extracted and sent. The DataBuffer will no
 *                                                     longer contain those bytes 
 *                                                    
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR SendFileToClient(LPTSTR ReceiverUsername, LPTSTR FileToSendName, CM_SIZE* BytesToSendCount, CM_DATA_BUFFER* DataBuffer);

CRITICAL_SECTION gFileCritSection;

