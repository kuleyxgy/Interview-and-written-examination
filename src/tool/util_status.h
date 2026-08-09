#ifndef UTIL_STATUS_H
#define UTIL_STATUS_H

#include <stdint.h>

typedef int32_t sns_status_t;

#define SNS_OK               ((sns_status_t)0)
#define SNS_ERR_PARAM        ((sns_status_t)-1)
#define SNS_ERR_STATE        ((sns_status_t)-2)
#define SNS_ERR_NOT_FOUND    ((sns_status_t)-3)
#define SNS_ERR_NOT_READY    ((sns_status_t)-4)
#define SNS_ERR_TIMEOUT      ((sns_status_t)-5)
#define SNS_ERR_IO           ((sns_status_t)-6)
#define SNS_ERR_CRC          ((sns_status_t)-7)
#define SNS_ERR_NO_SPACE     ((sns_status_t)-8)
#define SNS_ERR_UNSUPPORTED  ((sns_status_t)-9)
#define SNS_ERR_INVALID_DATA ((sns_status_t)-10)

const char *sns_status_name(sns_status_t status);

#endif
