#pragma once
#include "communication_api.h"
#include "UserManagement.h"
#define CC_HASH_TABLE_MAX_SIZE 65521
/*
 * In this header a hash table is implemented that is meant to hold
 * the users in an efficient manner. The actual repository of the users 
 * is global, otherwise it would need to be passed to almost every function
 * that deals with managing the users.
 */

typedef struct _USERS_LIST_NODE {
    struct _USERS_LIST_NODE *Next;
    LPTSTR Key;
    USER* User;
} USERS_LIST_NODE;

typedef struct _USERS_HASH_TABLE {
    // Pointer to the start of each chain
    USERS_LIST_NODE** LinkedListKeyValue;
    SRWLOCK SRWLock;
    CM_SIZE Size;
    CM_SIZE KeyCount; 
} USERS_HASH_TABLE;

/*
 * This function creates an instance of the USERS_HASH_TABLE structure
 * 
 * Parameters:  __Out__ USERS_HASH_TABLE** HashTable - the hash table to be created
 *                __In__ CM_SIZE Size - the size of the hash table, in number of 
 *                                    different hashes it can hold
 *                                    
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR UsersHtCreate(USERS_HASH_TABLE** HashTable, CM_SIZE Size);

/*
 * This function destroys an instance of the USERS_HASH_TABLE structure
 * 
 * Parameters:  __Out__ USERS_HASH_TABLE** HashTable - the hash table to destroy
 * 
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR UsersHtDestroy(USERS_HASH_TABLE** HashTable);

/*
 * Sets the value of a Key in the hash table, the value being a User
 * 
 * Parameters:  __Out__ USERS_HASH_TABLE* HashTable - the hash table to be updated
 *                __In__ LPTSTR Key - the key to be updated
 *                __In__ USER* User - the user held at the corresponding key
 *
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR UsersHtSetKeyValue(USERS_HASH_TABLE* HashTable, LPTSTR Key, USER* User);

/*
 * Gets the user that corresponds to a certain key. For more thread safety, the User
 * must be allocated
 * 
 * Parameters:  __In__ USERS_HASH_TABLE* HashTable - the hash table to get the user from
 *                __In__ LPTSTR Key - the key to be searched
 *                __Out__ USER* User - the user that corresponds to the key
 *
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR UsersHtGetKeyValue(USERS_HASH_TABLE* HashTable, LPTSTR Key, USER* User);

/*
 * Checks if a certain key is in the hash table
 * 
 * Parameters:  __In__ USERS_HASH_TABLE* HashTable - the hash table to be searched
 *                __OUT__ BOOLEAN* HasKey - TRUE if the key is in the hash table and FALSE otherwise
 *
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR UsersHtHasKey(USERS_HASH_TABLE* HashTable, LPTSTR Key, BOOLEAN* HasKey);

/*
 * Removes a key from the hash table and frees the corresponding memory
 * 
 * Parameters:  __Out__ USERS_HASH_TABLE* HashTable - the hash table to be modified
 *                __In__ LPTSTR Key - the key to be removed
 *                
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR UsersHtRemoveKey(USERS_HASH_TABLE* HashTable, LPTSTR Key);

/*
 * A function to that allows iteration through the hashtable
 * The iteration starts by providing a NULL USER_LIST_NODE*
 * For each function call, this USER_LIST_NODE will be updated
 * The iteration stops when the node is NULL again
 * 
 * Parameters:  __In__  USERS_HASH_TABLE* HashTable - the hash table to be parsed
 *                __Out__ USERS_LIST_NODE** CurrentNode - the current node in the iteration
 *        
 * Returns: a CM_ERROR code in case of failure and CM_SUCCESS otherwise
 */
CM_ERROR UsersHtParse(USERS_HASH_TABLE* HashTable, USERS_LIST_NODE** CurrentNode);

USERS_HASH_TABLE* gUsers;
