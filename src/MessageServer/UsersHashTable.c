#include "stdafx.h"

#include "UsersHashTable.h"
#include <strsafe.h>

static CM_ERROR UsersHtHashFunction(
    USERS_HASH_TABLE* HashTable, LPTSTR Key, CM_SIZE* Hash);
static CM_ERROR UsersHtAppendKeyToNode(
    USERS_LIST_NODE** Node, LPTSTR Key, USER* Value);
static CM_ERROR UsersHtFindFirstValue(
    USERS_HASH_TABLE* HashTable, USERS_LIST_NODE** FirstValue);

CM_ERROR UsersHtCreate(USERS_HASH_TABLE** HashTable, CM_SIZE Size)
{
    if (NULL != *HashTable)
    {
        return -1;
    }

    USERS_HASH_TABLE* hashTable = (USERS_HASH_TABLE*)malloc(sizeof(USERS_HASH_TABLE));
    if (NULL == hashTable)
    {
        return CM_NO_MEMORY;
    }
    hashTable->Size = Size;
    hashTable->LinkedListKeyValue = (USERS_LIST_NODE**)
        malloc(Size * sizeof(USERS_LIST_NODE*));
    if (NULL == hashTable->LinkedListKeyValue)
    {
        return CM_NO_MEMORY;
    }
    for (CM_SIZE i = 0; i < Size; i++)
    {
        hashTable->LinkedListKeyValue[i] = NULL;
    }
    hashTable->KeyCount = 0;
    InitializeSRWLock(&hashTable->SRWLock);

    *HashTable = hashTable;
    return CM_SUCCESS;
}

CM_ERROR UsersHtDestroy(USERS_HASH_TABLE** HashTable)
{
    if (NULL == *HashTable)
    {
        return CM_INVALID_PARAMETER;
    }

    USERS_HASH_TABLE** hashTable = HashTable;

    for (CM_SIZE i = 0; i < (*hashTable)->Size; i++)
    {
        while (NULL != (*hashTable)->LinkedListKeyValue[i])
        {
            USERS_LIST_NODE* temp = (*hashTable)->LinkedListKeyValue[i];
            (*hashTable)->LinkedListKeyValue[i] = (*hashTable)->LinkedListKeyValue[i]->Next;
            if(NULL != temp->Key)
            {
                free(temp->Key);
                temp->Key = NULL;
            }
            if (NULL != temp)
            {
                free(temp);
                temp = NULL;
            }
        }
    }
    if (NULL != (*hashTable)->LinkedListKeyValue)
    {
        free((*hashTable)->LinkedListKeyValue);
        (*hashTable)->LinkedListKeyValue = 0;
    }

    if (NULL != hashTable)
    {
        free(*hashTable);
        *hashTable = NULL;
    }
    return CM_SUCCESS;
}

CM_ERROR UsersHtSetKeyValue(USERS_HASH_TABLE* HashTable, LPTSTR Key, USER* User)
{
    if (NULL == HashTable || NULL == Key)
    {
        return CM_INVALID_PARAMETER;
    }

    USERS_HASH_TABLE* hashTable = HashTable;
    LPTSTR key = Key;
    USER* value = User;

    CM_SIZE hash = 0;
    CM_ERROR cmError = UsersHtHashFunction(HashTable, Key, &hash);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }

    AcquireSRWLockExclusive(&hashTable->SRWLock);
    USERS_LIST_NODE** currentNode = &hashTable->LinkedListKeyValue[hash];

    // there is no string with current hash in the table, so we insert one
    if (NULL == *currentNode)
    {
        cmError = UsersHtAppendKeyToNode(currentNode, key, value);
        hashTable->KeyCount++;
        ReleaseSRWLockExclusive(&hashTable->SRWLock);
        return cmError;
    }

    while (NULL != *currentNode)
    {
        if (0 == _tcscmp((*currentNode)->Key, key))
        {
            (*currentNode)->User = value;
            ReleaseSRWLockExclusive(&hashTable->SRWLock);
            return CM_SUCCESS;
        }
        if (NULL == (*currentNode)->Next)
        {
            cmError = UsersHtAppendKeyToNode(&(*currentNode)->Next, key, value);
            hashTable->KeyCount++;
            ReleaseSRWLockExclusive(&hashTable->SRWLock);
            return cmError;
        }
        currentNode = &(*currentNode)->Next;
    }

    ReleaseSRWLockExclusive(&hashTable->SRWLock);
    return CM_INTERNAL_ERROR;
}

CM_ERROR UsersHtGetKeyValue(USERS_HASH_TABLE* HashTable, LPTSTR Key, USER* User)
{
    if (NULL == HashTable || NULL == Key || NULL == User)
    {
        return CM_INVALID_PARAMETER;
    }

    USERS_HASH_TABLE* hashTable = HashTable;
    LPTSTR key = Key;

    CM_SIZE hash = 0;
    CM_ERROR cmError = UsersHtHashFunction(HashTable, key, &hash);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    AcquireSRWLockShared(&hashTable->SRWLock);
    USERS_LIST_NODE* currentNode = hashTable->LinkedListKeyValue[hash];
    while (NULL != currentNode)
    {
        if (0 == _tcscmp(currentNode->Key, key))
        {
            *User = *(currentNode->User);
            ReleaseSRWLockShared(&hashTable->SRWLock);
            return CM_SUCCESS;
        }
        currentNode = currentNode->Next;
    }

    ReleaseSRWLockShared(&hashTable->SRWLock);
    return CM_INTERNAL_ERROR;
}

CM_ERROR UsersHtHasKey(USERS_HASH_TABLE* HashTable, LPTSTR Key, BOOLEAN* HasKey)
{
    if (NULL == HashTable || NULL == Key || NULL == HasKey)
    {
        return FALSE;
    }
    USERS_HASH_TABLE* hashTable = HashTable;
    LPTSTR key = Key;
    *HasKey = FALSE;

    CM_SIZE hash = 0;
    CM_ERROR cmError = UsersHtHashFunction(HashTable, key, &hash);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    AcquireSRWLockShared(&hashTable->SRWLock);
    USERS_LIST_NODE* currentNode = hashTable->LinkedListKeyValue[hash];
    while (NULL != currentNode)
    {
        if (0 == _tcscmp(currentNode->Key, key))
        {
            ReleaseSRWLockShared(&hashTable->SRWLock);
            *HasKey = TRUE;
            return CM_SUCCESS;
        }
        currentNode = currentNode->Next;
    }

    ReleaseSRWLockShared(&hashTable->SRWLock);
    *HasKey = FALSE;
    return CM_SUCCESS;
}

CM_ERROR UsersHtRemoveKey(USERS_HASH_TABLE* HashTable, LPTSTR Key)
{
    if (NULL == HashTable || NULL == Key)
    {
        return CM_INVALID_PARAMETER;
    }

    USERS_HASH_TABLE* hashTable = HashTable;
    LPTSTR key = Key;

    CM_SIZE hash = 0;
    CM_ERROR cmError = UsersHtHashFunction(HashTable, key, &hash);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }

    AcquireSRWLockExclusive(&hashTable->SRWLock);
    USERS_LIST_NODE* currentNode = hashTable->LinkedListKeyValue[hash];
    USERS_LIST_NODE* prevNode = NULL;

    while (NULL != currentNode && 0 != _tcscmp(currentNode->Key, key))
    {
        prevNode = currentNode;
        currentNode = currentNode->Next;
    }
    if (NULL == currentNode)
    {
        ReleaseSRWLockExclusive(&hashTable->SRWLock);
        return CM_INTERNAL_ERROR;
    }

    if (NULL != prevNode)
    {
        prevNode->Next = currentNode->Next;
    }
    else
    {
        hashTable->LinkedListKeyValue[hash] = currentNode->Next;
    }
    hashTable->KeyCount--;
    if(NULL != currentNode->Key)
    {
        free(currentNode->Key);
        currentNode->Key = NULL;
    }
    if (NULL != currentNode)
    {
        free(currentNode);
        currentNode = NULL;
    }
    ReleaseSRWLockExclusive(&hashTable->SRWLock);
    return CM_SUCCESS;
}

CM_ERROR UsersHtParse(USERS_HASH_TABLE* HashTable, USERS_LIST_NODE** CurrentNode)
{
    if (NULL == HashTable || NULL == CurrentNode)
    {
        return CM_INVALID_PARAMETER;
    }
    CM_ERROR cmError;

    USERS_LIST_NODE* currentNode = *CurrentNode;
    AcquireSRWLockShared(&HashTable->SRWLock);
    if (NULL == currentNode)
    {
        cmError = UsersHtFindFirstValue(HashTable, CurrentNode);
        ReleaseSRWLockShared(&HashTable->SRWLock);
        return cmError;
    }
    if (NULL != currentNode->Next)
    {
        *CurrentNode = currentNode->Next;
        ReleaseSRWLockShared(&HashTable->SRWLock);
        return CM_SUCCESS;
    }

    CM_SIZE hash = 0;
    cmError = UsersHtHashFunction(HashTable, currentNode->Key, &hash);
    if (CM_IS_ERROR(cmError))
    {
        return cmError;
    }
    hash++;
    for (CM_SIZE i = hash; i < HashTable->Size; ++i)
    {
        if (NULL != HashTable->LinkedListKeyValue[i])
        {
            *CurrentNode = HashTable->LinkedListKeyValue[i];
            ReleaseSRWLockShared(&HashTable->SRWLock);
            return CM_SUCCESS;
        }
    }
    ReleaseSRWLockShared(&HashTable->SRWLock);
    *CurrentNode = NULL;
    return CM_SUCCESS;
}

static CM_ERROR UsersHtHashFunction(USERS_HASH_TABLE* HashTable, LPTSTR Key, CM_SIZE* Hash)
{
    if (NULL == HashTable || Key == NULL || NULL == Hash)
    {
        return CM_INVALID_PARAMETER;
    }

    LPSTR key = NULL;
    CM_SIZE keyLen = 0;

    HRESULT hrError;
#if defined(UNICODE) || defined(_UNICODE)
    CM_SIZE keyLenWide = 0;
    hrError = StringCchLengthW(Key, CHARS_COUNT, &keyLenWide);
    if (FAILED(hrError))
    {
        return CM_INSUFFICIENT_BUFFER;
    }
    keyLenWide++;
    CM_SIZE bufferSize = WideCharToMultiByte(CP_UTF8, 0, Key,
                                             keyLenWide, NULL, 0, NULL, NULL);
    if (0 == bufferSize)
    {
        return CM_EXT_FUNC_FAILURE;
    }
    key = (LPSTR)malloc(bufferSize);
    if (NULL == key)
    {
        return CM_NO_MEMORY;
    }
    bufferSize = WideCharToMultiByte(CP_UTF8, 0, Key,
                                     keyLenWide, key, bufferSize, NULL, NULL);
    if (0 == bufferSize)
    {
        if (NULL != key)
        {
            free(key);
            key = NULL;
        }
        return CM_EXT_FUNC_FAILURE;
    }
#else
    key = Key;
#endif

    CM_ERROR cmError = CM_SUCCESS;
    __try
    {
        keyLen = 0;
        hrError = StringCchLengthA((STRSAFE_PCNZCH)key, CHARS_COUNT, &keyLen);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        CM_SIZE hash = 5381;
        for (CM_SIZE i = 0; i < keyLen; ++i)
        {
            hash = ((hash << 5) + hash + (BYTE)key[i]) % HashTable->Size;
        }

        *Hash = hash % HashTable->Size;
    }
    __finally
    {
#if defined(UNICODE) || defined(_UNICODE)
        if (NULL != key)
        {
            free(key);
            key = NULL;
        }
#endif
    }
    return cmError;
}

static CM_ERROR UsersHtAppendKeyToNode(USERS_LIST_NODE** Node,
                                  LPTSTR Key, USER* Value)
{
    if (NULL == Node || NULL == Key)
    {
        return CM_INVALID_PARAMETER;
    }

    LPTSTR key = Key;
    USER* value = Value;

    USERS_LIST_NODE* node = NULL;

    CM_ERROR cmError = CM_SUCCESS;
    HRESULT hrError;
    __try
    {
        node = (USERS_LIST_NODE*)malloc(
            sizeof(USERS_LIST_NODE));
        if (NULL == node)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        CM_SIZE keyLen = 0;
        hrError = StringCchLength(key, STRSAFE_MAX_LENGTH, &keyLen);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        node->Key = (LPTSTR)malloc(sizeof(TCHAR) * (keyLen + 1));
        if (NULL == node)
        {
            cmError = CM_NO_MEMORY;
            __leave;
        }
        hrError = StringCchCopy(node->Key, keyLen + 1, key);
        if (FAILED(hrError))
        {
            cmError = CM_INSUFFICIENT_BUFFER;
            __leave;
        }
        node->User = value;
        node->Next = NULL;
        __leave;
    }
    __finally
    {
        if (CM_IS_ERROR(cmError))
        {
            if (NULL != node->Key)
            {
                free(node->Key);
                node->Key = NULL;
            }
            if (NULL != node)
            {
                free(node);
                node = NULL;
            }
        }
        else
        {
            *Node = node;
        }
    }
    return cmError;
}

static CM_ERROR UsersHtFindFirstValue(USERS_HASH_TABLE* HashTable, USERS_LIST_NODE** FirstValue)
{
    if (NULL == HashTable || NULL == FirstValue)
    {
        return CM_INVALID_PARAMETER;
    }

    for (CM_SIZE i = 0; i < HashTable->Size; ++i)
    {
        if (NULL != HashTable->LinkedListKeyValue[i])
        {
            *FirstValue = HashTable->LinkedListKeyValue[i];
            return CM_SUCCESS;
        }
    }
    return CM_INTERNAL_ERROR;
}
