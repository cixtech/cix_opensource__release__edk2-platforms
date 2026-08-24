#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#include "validate_config.h"

#define VALIDATOR_1_OPER(_name, _obj_type, _obj_nr, _oper, _thresh)    \
    struct validator    _name = {       \
        .obj_type   = (_obj_type),      \
        .obj_nr     = (_obj_nr),        \
        .oper_nr    = 1,                \
        .oper[0]    = {                 \
            .oper   = (_oper),          \
            .threshold  = (_thresh),    \
        },                              \
    };

#define VALIDATOR_2_OPER(_name, _obj_type, _obj_nr, _oper0, _thresh0, _oper1, _thresh1)    \
    struct validator    _name = {       \
        .obj_type   = (_obj_type),      \
        .obj_nr     = (_obj_nr),        \
        .oper_nr    = 2,                \
        .oper[0]    = {                 \
            .oper   = (_oper0),         \
            .threshold  = (_thresh0),   \
        },                              \
        .oper[1]    = {                 \
            .oper   = (_oper1),         \
            .threshold  = (_thresh1),   \
        },                              \
    };

#define VAL_CMP(_oper, _val, _ref)  \
    switch (_oper) {                \
    case OPER_LT:                   \
        return ((_val) < (_ref));   \
    case OPER_LT_OR_EQ:             \
        return ((_val) <= (_ref));  \
    case OPER_EQ:                   \
        return ((_val) == (_ref));  \
    case OPER_GT:                   \
        return ((_val) > (_ref));   \
    case OPER_GT_OR_EQ:             \
        return ((_val) >= (_ref));  \
    case OPER_NOT_EQ:               \
        return ((_val) != (_ref));  \
    default:                        \
        assert(0);                  \
        return false;               \
    }

static bool validate_int8(const struct operator *op, const void *data)
{
    int8_t val;
    int8_t ref;

    memcpy(&val, data, sizeof(val));
    ref = (int8_t)(op->threshold);

    VAL_CMP(op->oper, val, ref)
}

static bool validate_uint8(const struct operator *op, const void *data)
{
    uint8_t val;
    uint8_t ref;

    memcpy(&val, data, sizeof(val));
    ref = (uint8_t)(op->threshold);

    VAL_CMP(op->oper, val, ref)
}

static bool validate_int16(const struct operator *op, const void *data)
{
    int16_t val;
    int16_t ref;

    memcpy(&val, data, sizeof(val));
    ref = (int16_t)(op->threshold);

    VAL_CMP(op->oper, val, ref)
}

static bool validate_uint16(const struct operator *op, const void *data)
{
    uint16_t val;
    uint16_t ref;

    memcpy(&val, data, sizeof(val));
    ref = (uint16_t)(op->threshold);

    VAL_CMP(op->oper, val, ref)
}

static bool validate_int32(const struct operator *op, const void *data)
{
    int32_t val;
    int32_t ref;

    memcpy(&val, data, sizeof(val));
    ref = (int32_t)(op->threshold);

    VAL_CMP(op->oper, val, ref)
}

static bool validate_uint32(const struct operator *op, const void *data)
{
    uint32_t val;
    uint32_t ref;

    memcpy(&val, data, sizeof(val));
    ref = (uint32_t)(op->threshold);

    VAL_CMP(op->oper, val, ref)
}

bool validate(const struct validator *vad, const void *data)
{
    typedef bool (*validate_func)(const struct operator *op, const void *data);
    static const validate_func callbacks[OBJ_TYPE_MAX] = {
        [OBJ_TYPE_INT8]     = validate_int8,
        [OBJ_TYPE_UINT8]    = validate_uint8,
        [OBJ_TYPE_INT16]    = validate_int16,
        [OBJ_TYPE_UINT16]   = validate_uint16,
        [OBJ_TYPE_INT32]    = validate_int32,
        [OBJ_TYPE_UINT32]   = validate_uint32,
    };
    bool ret;
    size_t obj_size;

    switch (vad->obj_type) {
    case OBJ_TYPE_INT8:
    case OBJ_TYPE_UINT8:
        obj_size = 1;
        break;
    case OBJ_TYPE_INT16:
    case OBJ_TYPE_UINT16:
        obj_size = 2;
        break;
    case OBJ_TYPE_INT32:
    case OBJ_TYPE_UINT32:
        obj_size = 4;
        break;
    default:
        assert(0);
        return false;
    }

    ret = true;
    for (uint8_t i = 0; i < vad->obj_nr; i++) {
        for (uint8_t j = 0; j < vad->oper_nr; j++) {
            const struct operator *op = &vad->oper[j];
            ret &= callbacks[vad->obj_type](op, ((const uint8_t *)data) + obj_size * i);
        }
        if (!ret) {
            break;
        }
    }
    return ret;
}

VALIDATOR_1_OPER(vad_pmic_opp_max, OBJ_TYPE_UINT16, OPP_DXS_MAX, OPER_LT_OR_EQ, OPP_NO_LIMIT);

bool validate_dpm_pwr_rail_cfg(const DPM_PWR_RAIL_CFG *cfg)
{
    static VALIDATOR_1_OPER(vad_vr_type, OBJ_TYPE_UINT32, 1, OPER_LT, VR_MAX);
    static VALIDATOR_1_OPER(vad_i2c_port, OBJ_TYPE_UINT32, 1, OPER_LT, 4);  /* A maximum of 4 CSU_PM I2C masters */
    static VALIDATOR_1_OPER(vad_i2c_buck, OBJ_TYPE_UINT32, 1, OPER_LT, 4);  /* A maximum of 4 bucks of each PMIC */
    static VALIDATOR_2_OPER(vad_delta_mV, OBJ_TYPE_INT32, 1, OPER_GT_OR_EQ, -500, OPER_LT_OR_EQ, 500);
    bool ret;
    uint32_t valu32;
    int32_t vali32;

    valu32 = cfg->vr_type;
    ret = validate(&vad_vr_type, &valu32);
    valu32 = cfg->i2c_port;
    ret &= validate(&vad_i2c_port, &valu32);
    valu32 = cfg->i2c_buck;
    ret &= validate(&vad_i2c_buck, &valu32);
    vali32 = cfg->delta_mV;
    ret &= validate(&vad_delta_mV, &vali32);

    return ret;
}

static bool validate_board_sensor(board_sensor_config_t* config)
{
    if (config->sensor_valid.fields.valid == PM_CONFIG_VALID) {
        if (config->sensor_valid.fields.raw_data > 1) {
            printf("board_sensor.sensor_valid invalid\n");
            return false;
        }
        if (config->sensor_valid.fields.raw_data == 0) {//memory type
            if (config->reg_id < 0x10 || config->reg_id >= 0x20) {
                printf("board_sensor.reg_id invalid\n");
                return false;
            }
        }
        if (config->sensor_valid.fields.raw_data == 1) {//i2c type
            if (config->i2c_ctrl > 3) {
                printf("board_sensor.i2c_ctrl invalid\n");
                return false;
            }
        }
    }

    return true;
}

bool validate_pvt_config(pm_config_pvt_t* config)
{
    uint32_t i = 0;
    uint32_t total_weight = 0;
    if (config->weight_valid == PM_CONFIG_VALID) {
        for (i = 0; i < 13; i ++) {
            total_weight += config->weight[i];
        }
        if (total_weight != 1024) {
            printf("all weight sum should equal 1024\n");
            return false;
        }
    }

    return (validate_board_sensor(&config->board_sensor1) &&
            validate_board_sensor(&config->board_sensor2));
}

/* FAN table */
static bool validate_rpm_table(rpm_entry_t *rpm_table, uint32_t num)
{
    for (uint32_t i = 1; i < num; i++) {
        if (rpm_table[i].rpm <= rpm_table[i - 1].rpm ||
            rpm_table[i].up_temp <= rpm_table[i - 1].up_temp ||
            rpm_table[i].down_temp <= rpm_table[i - 1].down_temp) {
                printf("rpm entry %d error\n", i);
            return false;
        }
    }
    return true;
}

bool validate_fan_table(pm_config_fan_t* config, uint32_t num)
{
    uint32_t i = 0;
    pm_config_fan_t* ptr = NULL;
    for (i = 0; i < num; i++) {
        ptr = &config[i];
        if (ptr->fan_valid.fields.valid == PM_CONFIG_INVALID) {
            continue;
        }
        if (ptr->fan_id.fields.valid == PM_CONFIG_VALID && ptr->fan_id.fields.raw_data >= MAX_FAN_NUM) {
            return false;
        }
        for(fan_mode_t m = FAN_MODE_NORMAL; m < FAN_MODE_MAX; m++) {
            if (ptr->rpm_table_valid[m] == PM_CONFIG_INVALID) {
                continue;
            }

            if (ptr->rpm_table_items[m] < 2 || ptr->rpm_table_items[m] > MAX_FAN_TABLE_ENTRIES) {
                return false;
            }

            if (!validate_rpm_table(ptr->rpm_table[m], ptr->rpm_table_items[m])) {
                printf("fan_config[%d].rpm_table[%d] error\n", i, m);
                return false;
            }
        }
    }

    return true;
}

bool validate_gpio_config(pm_config_gpio_t* config)
{
    config_data_t *gpio = &config->gpio[0];
    uint32_t gpio_num = 3;
    do {
        if (gpio->fields.valid == PM_CONFIG_VALID) {
            if (gpio->fields.raw_data >= PM_GPIO_FUNC_MAX || gpio->fields.raw_data == PM_GPIO_FUNC_NULL) {
                printf("gpio function invalid\n");
                return false;
            }
        }
        gpio++;
        gpio_num--;
    } while (gpio_num);

    return true;
}

bool validate_op_mode(config_data_t *config)
{
    if (config->fields.valid != PM_CONFIG_VALID) {
        return true;
    }
    return (config->fields.raw_data < PM_OP_MODE_MAX);
}

void avoid_compiler_warnings(void)
{
#if PM_PVT_SENSOR_CONFIG
    (void)pvt_config;
#endif
#if PM_PMIC_CONFIG
    (void)pmic_config;
#endif
#if PM_FAN_TABLE_CONFIG
    (void)fan_config;
#endif
#if PM_LOG_CONFIG
    (void)log_config;
#endif
#if PM_VMIN_CONFIG
    (void)vmin_config;
#endif
#if PM_NOC_IDLE_CONFIG
    (void)noc_idle_config_data;
#endif
#if PM_SPT_CONFIG
    (void)spt_config;
#endif
#if PM_WDT_CONFIG
    (void)wdt_timeout;
#endif
#if PM_OPP_100M_CONFIG
    (void)opp_100M_enable;
#endif
#if PM_GPIO_CONFIG
    (void)gpio_config;
#endif
#if PM_OP_MODE_CONFIG
    (void)op_mode_config;
#endif
}
