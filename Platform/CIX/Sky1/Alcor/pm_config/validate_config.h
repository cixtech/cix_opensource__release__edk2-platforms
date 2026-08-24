/*
 * Copyright 2024 - Cix Technology Group Co., Ltd. All Rights Reserved.
 */
#ifndef __VALIDATE_CONFIG_H__
#define __VALIDATE_CONFIG_H__

#include <stdint.h>
#include "cfg_dpm_pwrrail.h"
#include "pm_export_config.h"

struct operator {
#define OPER_LT         0
#define OPER_LT_OR_EQ   1
#define OPER_EQ         2
#define OPER_GT         3
#define OPER_GT_OR_EQ   4
#define OPER_NOT_EQ     5
    uint8_t     oper;
    uint32_t    threshold;
};

struct validator {
#define OBJ_TYPE_INT8       0
#define OBJ_TYPE_UINT8      1
#define OBJ_TYPE_INT16      2
#define OBJ_TYPE_UINT16     3
#define OBJ_TYPE_INT32      4
#define OBJ_TYPE_UINT32     5
#define OBJ_TYPE_MAX        6
    uint8_t         obj_type;
    uint8_t         obj_nr;
    uint8_t         oper_nr;
    struct operator oper[4];
};

/* generic */
bool validate(const struct validator *vad, const void *data);

/* PMIC */
extern struct validator vad_pmic_opp_max;
bool validate_dpm_pwr_rail_cfg(const DPM_PWR_RAIL_CFG *cfg);

/* PVT */
bool validate_pvt_config(pm_config_pvt_t* config);

/* FAN */
bool validate_fan_table(pm_config_fan_t* config, uint32_t num);

/* GPIO */
bool validate_gpio_config(pm_config_gpio_t* config);

/* OP mode */
bool validate_op_mode(config_data_t *config);

#endif
