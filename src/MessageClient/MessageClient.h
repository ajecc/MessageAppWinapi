#pragma once
#include "communication_api.h"
#include <Windows.h>

#define CLIENT_THREADS_COUNT 2

typedef CM_CLIENT *PCM_CLIENT;

CM_ERROR SendStringToServer(CM_CLIENT* Client, CM_DATA_BUFFER* DataBuffer, LPTSTR String);

