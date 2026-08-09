#include "func_filter.h"

#include "func_cfg.h"

static sns_status_t func_filter_instance_valid(
    const func_filter_instance_t *instance)
{
    if ((instance == NULL) || (instance->ops == NULL) ||
        (instance->ops->init == NULL) || (instance->ops->reset == NULL) ||
        (instance->ops->process == NULL) || (instance->state == NULL) ||
        (instance->state_size == 0U) || (instance->cfg == NULL)) {
        return SNS_ERR_PARAM;
    }
    return SNS_OK;
}

sns_status_t func_filter_chain_init(func_filter_chain_t *chain)
{
    uint8_t index;
    sns_status_t status;

    if (chain == NULL) {
        return SNS_ERR_PARAM;
    }
    if (chain->count > FUNC_CFG_MAX_FILTERS_PER_SENSOR) {
        return SNS_ERR_NO_SPACE;
    }
    if ((chain->count != 0U) && (chain->items == NULL)) {
        return SNS_ERR_PARAM;
    }
    for (index = 0U; index < chain->count; ++index) {
        status = func_filter_instance_valid(&chain->items[index]);
        if (status != SNS_OK) {
            return status;
        }
        status = chain->items[index].ops->init(chain->items[index].state,
                                               chain->items[index].state_size,
                                               chain->items[index].cfg);
        if (status != SNS_OK) {
            return status;
        }
    }
    return SNS_OK;
}

sns_status_t func_filter_chain_reset(func_filter_chain_t *chain)
{
    uint8_t index;
    sns_status_t status;

    if (chain == NULL) {
        return SNS_ERR_PARAM;
    }
    if ((chain->count > FUNC_CFG_MAX_FILTERS_PER_SENSOR) ||
        ((chain->count != 0U) && (chain->items == NULL))) {
        return SNS_ERR_STATE;
    }
    for (index = 0U; index < chain->count; ++index) {
        status = func_filter_instance_valid(&chain->items[index]);
        if (status != SNS_OK) {
            return SNS_ERR_STATE;
        }
        status = chain->items[index].ops->reset(chain->items[index].state,
                                                chain->items[index].cfg);
        if (status != SNS_OK) {
            return status;
        }
    }
    return SNS_OK;
}

sns_status_t func_filter_chain_process(func_filter_chain_t *chain,
                                       func_filter_value_t input,
                                       func_filter_value_t *output)
{
    uint8_t index;
    func_filter_value_t current;
    func_filter_value_t next;
    sns_status_t status;

    if ((chain == NULL) || (output == NULL)) {
        return SNS_ERR_PARAM;
    }
    if ((chain->count > FUNC_CFG_MAX_FILTERS_PER_SENSOR) ||
        ((chain->count != 0U) && (chain->items == NULL))) {
        return SNS_ERR_STATE;
    }

    current = input;
    for (index = 0U; index < chain->count; ++index) {
        status = func_filter_instance_valid(&chain->items[index]);
        if (status != SNS_OK) {
            return SNS_ERR_STATE;
        }
        status = chain->items[index].ops->process(chain->items[index].state,
                                                  current,
                                                  &next);
        if (status != SNS_OK) {
            return status;
        }
        current = next;
    }
    *output = current;
    return SNS_OK;
}
