#ifndef _COMMUNICATION_ERROR_H_
#define _COMMUNICATION_ERROR_H_

typedef long CM_ERROR;

#define MAKE_ERROR_CODE(Severity, ErrorValue) (((Severity) << 30) + (ErrorValue))

#define CM_SUCCESS_SEVERITY 0x0
#define CM_ERROR_SEVERITY 0x1

/*
    One can use those MACROS to check the values returned from COMMUNICATION library.
*/
#define CM_IS_SUCCESS(ErrorValue) (((ErrorValue) >> 30) == 0)
#define CM_IS_ERROR(ErrorValue) (((ErrorValue) >> 30) == 1)

// error codes - if you encounter an error you can search the error code here
#define CM_SUCCESS MAKE_ERROR_CODE(CM_SUCCESS_SEVERITY, 0x0)        /// SUCCESS 0x0

#define CM_INTERNAL_ERROR                 MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0x1)
#define CM_LIB_INIT_FAILED                MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0x2)
#define CM_INVALID_CONNECTION             MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0x3)
#define CM_CONNECTION_SEND_FAILED         MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0x4)
#define CM_CONNECTION_RECEIVE_FAILED      MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0x5)
#define CM_CONNECTION_TERMINATED          MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0x6)
#define CM_INVALID_PARAMETER              MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0x7)
#define CM_NO_MEMORY                      MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0x8)
#define CM_NO_RESOURCES_FOR_SERVER        MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0x9)
#define CM_NO_RESOURCES_FOR_SERVER_CLIENT MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0xA)
#define CM_INSUFFICIENT_BUFFER            MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0xB)
#define CM_EXT_FUNC_FAILURE                  MAKE_ERROR_CODE(CM_ERROR_SEVERITY, 0xC)

#endif // !_COMMUNICATION_ERROR_H_
