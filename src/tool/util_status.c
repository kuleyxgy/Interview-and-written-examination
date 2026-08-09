#include "util_status.h"

const char *sns_status_name(sns_status_t status)
{
    switch (status) {
    case SNS_OK:
        return "OK";
    case SNS_ERR_PARAM:
        return "PARAM";
    case SNS_ERR_STATE:
        return "STATE";
    case SNS_ERR_NOT_FOUND:
        return "NOT_FOUND";
    case SNS_ERR_NOT_READY:
        return "NOT_READY";
    case SNS_ERR_TIMEOUT:
        return "TIMEOUT";
    case SNS_ERR_IO:
        return "IO";
    case SNS_ERR_CRC:
        return "CRC";
    case SNS_ERR_NO_SPACE:
        return "NO_SPACE";
    case SNS_ERR_UNSUPPORTED:
        return "UNSUPPORTED";
    case SNS_ERR_INVALID_DATA:
        return "INVALID_DATA";
    default:
        return "UNKNOWN";
    }
}
