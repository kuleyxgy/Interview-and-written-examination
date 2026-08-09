#include "proto_display.h"

#include <stddef.h>

sns_status_t proto_display_show(proto_display_t *display,
                                const proto_display_record_t *record)
{
    if ((display == NULL) || (display->ops == NULL) ||
        (display->ops->show == NULL) || (record == NULL)) {
        return SNS_ERR_PARAM;
    }

    return display->ops->show(display->ctx, record);
}
