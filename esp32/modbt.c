/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Pycom Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "py/mpstate.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "modbt.h"

#include "controller.h"
#include "bt.h"
#include "bt_trace.h"
#include "bt_types.h"
#include "btm_api.h"
#include "bta_api.h"
#include "bta_gatt_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/opt.h"
#include "lwip/def.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define BT_SCAN_QUEUE_SIZE_MAX                              (8)
#define BT_GATTS_QUEUE_SIZE_MAX                             (2)
#define BT_CHAR_VALUE_SIZE_MAX                              (20)

#define MOD_BT_CLIENT_APP_ID                                (0)
#define MOD_BT_SERVER_APP_ID                                (0)

#define MOD_BT_GATTC_ADV_EVT                                (0x0001)
#define MOD_BT_GATTS_CONN_EVT                               (0x0002)
#define MOD_BT_GATTS_DISCONN_EVT                            (0x0004)
#define MOD_BT_GATTS_READ_EVT                               (0x0008)
#define MOD_BT_GATTS_WRITE_EVT                              (0x0010)
#define MOD_BT_GATTC_NOTIFY_EVT                             (0x0020)

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t         base;
    int32_t               scan_duration;
    int32_t               conn_id;          // current activity connection id
    int32_t               gatts_conn_id;
    uint16_t              gatts_if;
    uint16_t              gattc_if;
    bool                  init;
    bool                  busy;
    bool                  scanning;
    bool                  advertising;
    bool                  controller_active;
} bt_obj_t;

typedef struct {
    mp_obj_base_t         base;
    mp_obj_list_t         srv_list;
    esp_bd_addr_t         srv_bda;
    int32_t               conn_id;
    esp_gatt_if_t         gatt_if;
} bt_connection_obj_t;

typedef struct {
    mp_obj_base_t         base;
    mp_obj_list_t         char_list;
    bt_connection_obj_t   *connection;
    esp_gatt_srvc_id_t    srv_id;
} bt_srv_obj_t;

typedef struct {
    esp_gatt_id_t           char_id;
    esp_gatt_char_prop_t    char_prop;
} bt_char_t;

typedef struct {
    mp_obj_base_t           base;
    bt_srv_obj_t            *service;
    bt_char_t               characteristic;
} bt_char_obj_t;

typedef struct {
    uint8_t     value[BT_CHAR_VALUE_SIZE_MAX];
    uint16_t    value_type;
    uint16_t    value_len;
} bt_read_value_t;

typedef struct {
    esp_gatt_status_t status;
} bt_write_value_t;

typedef enum {
    E_BT_STACK_MODE_BLE = 0,
    E_BT_STACK_MODE_BT
} bt_mode_t;

typedef struct {
    int32_t         conn_id;
    esp_bd_addr_t   srv_bda;
    esp_gatt_if_t   gatt_if;
} bt_connection_event_t;

typedef union {
    esp_ble_gap_cb_param_t      scan;
    esp_gatt_srvc_id_t          service;
    bt_char_t                   characteristic;
    bt_read_value_t             read;
    bt_write_value_t            write;
    bt_connection_event_t       connection;
} bt_event_result_t;

typedef union {
    uint16_t              service_handle;
    uint16_t              char_handle;
    uint16_t              char_descr_handle;
    bool                  adv_set;
} bt_gatts_event_result_t;

typedef struct {
    mp_obj_base_t         base;
    uint16_t              handle;
    bool                  started;
} bt_gatts_srv_obj_t;

typedef struct {
    mp_obj_base_t         base;
    mp_obj_t              parent;
    esp_bt_uuid_t         uuid;
    uint16_t              handle;
    uint16_t              value_len;
    uint8_t               value[BT_CHAR_VALUE_SIZE_MAX];
    bool                  is_char;
} bt_gatts_attr_obj_t;

typedef struct {
    bt_gatts_attr_obj_t   attr_obj;
    uint32_t              trans_id;
    uint16_t              config;
} bt_gatts_char_obj_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static volatile bt_obj_t bt_obj;
static QueueHandle_t xScanQueue;
static QueueHandle_t xGattsQueue;

static const mp_obj_type_t mod_bt_connection_type;
static const mp_obj_type_t mod_bt_service_type;
static const mp_obj_type_t mod_bt_characteristic_type;

static const mp_obj_type_t mod_bt_gatts_service_type;
static const mp_obj_type_t mod_bt_gatts_char_type;

static esp_ble_adv_params_t bt_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void gap_events_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gattc_events_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void close_connection(int32_t conn_id);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modbt_init0(void) {
    if (!xScanQueue) {
        xScanQueue = xQueueCreate(BT_SCAN_QUEUE_SIZE_MAX, sizeof(bt_event_result_t));
    } else {
        xQueueReset(xScanQueue);
    }
    if (!xGattsQueue) {
        xGattsQueue = xQueueCreate(BT_GATTS_QUEUE_SIZE_MAX, sizeof(bt_gatts_event_result_t));
    } else {
        xQueueReset(xGattsQueue);
    }

    if (bt_obj.init) {
        esp_ble_gattc_app_unregister(MOD_BT_CLIENT_APP_ID);
        esp_ble_gatts_app_unregister(MOD_BT_SERVER_APP_ID);
    }

    mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(btc_conn_list), 0);
    mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(bts_srv_list), 0);
    mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(bts_attr_list), 0);
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
static esp_gatt_status_t status = ESP_GATT_ERROR;

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = ESP_PUBLIC_ADDR,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30
};

static void close_connection (int32_t conn_id) {
    for (mp_uint_t i = 0; i < MP_STATE_PORT(btc_conn_list).len; i++) {
        bt_connection_obj_t *connection_obj = ((bt_connection_obj_t *)(MP_STATE_PORT(btc_conn_list).items[i]));
        if (connection_obj->conn_id == conn_id) {
            connection_obj->conn_id = -1;
            mp_obj_list_remove((void *)&MP_STATE_PORT(btc_conn_list), connection_obj);
        }
    }
}

static bt_gatts_attr_obj_t *find_gatts_attr_by_handle (uint16_t handle) {
    for (mp_uint_t i = 0; i < MP_STATE_PORT(bts_attr_list).len; i++) {
        bt_gatts_attr_obj_t *char_obj = ((bt_gatts_attr_obj_t *)(MP_STATE_PORT(bts_attr_list).items[i]));
        if (char_obj->handle == handle) {
            return char_obj;
        }
    }
    return NULL;
}

static void gap_events_handler (esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        int32_t duration = bt_obj.scan_duration;
        // the unit of the duration is seconds
        if (duration < 0) {
            duration = 0xFFFF;
        }
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
        bt_gatts_event_result_t gatts_event;
        gatts_event.adv_set = true;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
        break;
    }
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        bt_event_result_t bt_event_result;
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        memcpy(&bt_event_result.scan, scan_result, sizeof(esp_ble_gap_cb_param_t));
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
            break;
        case ESP_GAP_SEARCH_DISC_RES_EVT:
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            if (bt_obj.scan_duration < 0) {
                esp_ble_gap_set_scan_params(&ble_scan_params);
            } else {
                bt_obj.scanning = false;
            }
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void gattc_events_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    uint16_t conn_id = 0;
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;
    bt_event_result_t bt_event_result;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        status = p_data->reg.status;
        bt_obj.gattc_if = gattc_if;
        break;
    case ESP_GATTC_OPEN_EVT:
        conn_id = p_data->open.conn_id;
        if (p_data->open.status == ESP_GATT_OK) {
            bt_event_result.connection.conn_id = conn_id;
            bt_event_result.connection.gatt_if = gattc_if;
            memcpy(bt_event_result.connection.srv_bda, p_data->open.remote_bda, ESP_BD_ADDR_LEN);
        } else {
            bt_event_result.connection.conn_id = -1;
        }
        xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        bt_obj.busy = false;
        break;
    case ESP_GATTC_READ_CHAR_EVT:
        if (p_data->read.status == ESP_GATT_OK) {
            memcpy(&bt_event_result.read.value, p_data->read.value, p_data->read.value_len);
            bt_event_result.read.value_len = p_data->read.value_len;
            bt_event_result.read.value_type = p_data->read.value_type;
            xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        }
        bt_obj.busy = false;
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        bt_event_result.write.status = p_data->write.status;
        xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        bt_obj.busy = false;
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        esp_gatt_srvc_id_t *srvc_id = &p_data->search_res.srvc_id;
        memcpy(&bt_event_result.service, srvc_id, sizeof(esp_gatt_srvc_id_t));
        xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
        bt_obj.busy = false;
        break;
    }
    case ESP_GATTC_GET_CHAR_EVT:
        if (p_data->get_char.status == ESP_GATT_OK) {
            esp_gatt_id_t *char_id = &p_data->get_char.char_id;
            memcpy(&bt_event_result.characteristic.char_id, char_id, sizeof(esp_gatt_id_t));
            bt_event_result.characteristic.char_prop = p_data->get_char.char_prop;
            xQueueSend(xScanQueue, (void *)&bt_event_result, (TickType_t)0);
            esp_ble_gattc_get_characteristic(gattc_if, bt_obj.conn_id, &p_data->get_char.srvc_id, char_id);
        } else {
            bt_obj.busy = false;
        }
        break;
    case ESP_GATTC_NOTIFY_EVT:
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
        bt_obj.busy = false;
        break;
    case ESP_GATTC_CANCEL_OPEN_EVT:
        bt_obj.busy = false;
        // intentional fall through
    case ESP_GATTC_CLOSE_EVT:
        bt_obj.conn_id = -1;
        close_connection(p_data->close.conn_id);
        break;
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    esp_ble_gatts_cb_param_t *p = (esp_ble_gatts_cb_param_t *)param;

    switch (event) {
    case ESP_GATTS_REG_EVT:
        bt_obj.gatts_if = gatts_if;
        break;
    case ESP_GATTS_READ_EVT: {
        bt_gatts_attr_obj_t *attr_obj = find_gatts_attr_by_handle (p->read.handle);
        if (attr_obj) {
            // send the response immediatelly
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = p->read.handle;
            rsp.attr_value.len = attr_obj->value_len;
            memcpy(&rsp.attr_value.value, attr_obj->value, attr_obj->value_len);
            esp_ble_gatts_send_response(gatts_if, p->read.conn_id, p->read.trans_id, ESP_GATT_OK, &rsp);
        }
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        bt_gatts_attr_obj_t *attr_obj = find_gatts_attr_by_handle (p->write.handle);
        if (attr_obj) {
            if (attr_obj->is_char) {
                // only write up to the maximum allowed size
                uint16_t write_len = p->write.len > BT_CHAR_VALUE_SIZE_MAX ? BT_CHAR_VALUE_SIZE_MAX : p->write.len;
                memcpy(attr_obj->value, p->write.value, write_len);
                attr_obj->value_len = write_len;
            }  else {    // characteristic descriptor (only characteristic configuration is available ATM)
                bt_gatts_char_obj_t *char_obj = (bt_gatts_char_obj_t *)attr_obj->parent;
                memcpy(&char_obj->config, p->write.value, 2);
            }
            esp_ble_gatts_send_response(gatts_if, p->write.conn_id, p->write.trans_id, ESP_GATT_OK, NULL);
        }
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
    case ESP_GATTS_MTU_EVT:
    case ESP_GATTS_CONF_EVT:
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT: {
        bt_gatts_event_result_t gatts_event;
        gatts_event.service_handle = p->create.service_handle;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
        break;
    }
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        bt_gatts_event_result_t gatts_event;
        gatts_event.char_handle = p->add_char.attr_handle;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
        bt_gatts_event_result_t gatts_event;
        gatts_event.char_descr_handle = p->add_char_descr.attr_handle;
        xQueueSend(xGattsQueue, (void *)&gatts_event, (TickType_t)0);
        break;
    }
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT:
        // only allow one connection at a time
        if (bt_obj.gatts_conn_id >= 0) {
            esp_ble_gatts_close(bt_obj.gatts_if, p->connect.conn_id);
        } else {
            bt_obj.gatts_conn_id = p->connect.conn_id;
        }
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        bt_obj.gatts_conn_id = -1;
        if (bt_obj.advertising) {
            esp_ble_gap_start_advertising(&bt_adv_params);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

/******************************************************************************/
// Micro Python bindings; BT class

/// \class Bluetooth
static mp_obj_t bt_init_helper(bt_obj_t *self, const mp_arg_val_t *args) {
    if (!self->init) {
        if (!self->controller_active) {
            esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
            esp_bt_controller_init(&bt_cfg);
            self->controller_active = true;
        }

        esp_bt_controller_enable(ESP_BT_MODE_BTDM);

        if (ESP_OK != esp_bluedroid_init()) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Bluetooth init failed"));
        }
        if (ESP_OK != esp_bluedroid_enable()) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Bluetooth enable failed"));
        }

        esp_ble_gap_register_callback(gap_events_handler);
        esp_ble_gattc_register_callback(gattc_events_handler);
        esp_ble_gatts_register_callback(gatts_event_handler);

        mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(btc_conn_list), 0);
        mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(bts_srv_list), 0);
        mp_obj_list_init((mp_obj_t)&MP_STATE_PORT(bts_attr_list), 0);

        self->init = true;
    }

    esp_ble_gattc_app_register(MOD_BT_CLIENT_APP_ID);
    esp_ble_gatts_app_register(MOD_BT_SERVER_APP_ID);
    bt_obj.gatts_conn_id = -1;

    return mp_const_none;
}

STATIC const mp_arg_t bt_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,   {.u_int  = 0} },
    { MP_QSTR_mode,         MP_ARG_KW_ONLY  | MP_ARG_INT,   {.u_int  = E_BT_STACK_MODE_BLE} },
};
STATIC mp_obj_t bt_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(bt_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), bt_init_args, args);

    // setup the object
    bt_obj_t *self = (bt_obj_t *)&bt_obj;
    self->base.type = (mp_obj_t)&network_bt_type;

    // check the peripheral id
    if (args[0].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "resource not available"));
    }

    // run the constructor if the peripehral is not initialized or extra parameters are given
    if (n_kw > 0 || !self->init) {
        // start the peripheral
        bt_init_helper(self, &args[1]);
    }

    return (mp_obj_t)self;
}

STATIC mp_obj_t bt_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(bt_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &bt_init_args[1], args);
    return bt_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_init_obj, 1, bt_init);

mp_obj_t bt_deinit(mp_obj_t self_in) {
    if (bt_obj.init) {
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_disable(ESP_BT_MODE_BTDM);
        bt_obj.init = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_deinit_obj, bt_deinit);

STATIC mp_obj_t bt_start_scan(mp_obj_t self_in, mp_obj_t timeout) {
    if (bt_obj.scanning || bt_obj.busy) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "operation already in progress"));
    }

    int32_t duration = mp_obj_get_int(timeout);
    if (duration == 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid scan time"));
    }

    bt_obj.scan_duration = duration;
    bt_obj.scanning = true;
    xQueueReset(xScanQueue);
    if (ESP_OK != esp_ble_gap_set_scan_params(&ble_scan_params)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "operation failed"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_start_scan_obj, bt_start_scan);

STATIC mp_obj_t bt_isscanning(mp_obj_t self_in) {
    if (bt_obj.scanning) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_isscanning_obj, bt_isscanning);

STATIC mp_obj_t bt_stop_scan(mp_obj_t self_in) {
    if (bt_obj.scanning) {
        esp_ble_gap_stop_scanning();
        bt_obj.scanning = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_stop_scan_obj, bt_stop_scan);

STATIC mp_obj_t bt_read_scan(mp_obj_t self_in) {
    bt_event_result_t bt_event;

    STATIC const qstr bt_scan_info_fields[] = {
        MP_QSTR_mac, MP_QSTR_addr_type, MP_QSTR_adv_type, MP_QSTR_rssi, MP_QSTR_data,
    };

    if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)0)) {
        mp_obj_t tuple[5];
        tuple[0] = mp_obj_new_bytes((const byte *)bt_event.scan.scan_rst.bda, 6);
        tuple[1] = mp_obj_new_int(bt_event.scan.scan_rst.ble_addr_type);
        tuple[2] = mp_obj_new_int(bt_event.scan.scan_rst.ble_evt_type & 0x03);    // FIXME
        tuple[3] = mp_obj_new_int(bt_event.scan.scan_rst.rssi);
        tuple[4] = mp_obj_new_bytes((const byte *)bt_event.scan.scan_rst.ble_adv, ESP_BLE_ADV_DATA_LEN_MAX);

        return mp_obj_new_attrtuple(bt_scan_info_fields, 5, tuple);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_read_scan_obj, bt_read_scan);

STATIC mp_obj_t bt_resolve_adv_data(mp_obj_t self_in, mp_obj_t adv_data, mp_obj_t data_type) {
    mp_buffer_info_t bufinfo;
    uint8_t data_len;
    uint8_t *data;

    uint8_t type = mp_obj_get_int(data_type);
    mp_get_buffer_raise(adv_data, &bufinfo, MP_BUFFER_READ);
    data = esp_ble_resolve_adv_data(bufinfo.buf, type, &data_len);

    if (data) {
        switch(type) {
            case ESP_BLE_AD_TYPE_FLAG:
                return mp_obj_new_int(*(int8_t *)data);
            case ESP_BLE_AD_TYPE_16SRV_PART:
            case ESP_BLE_AD_TYPE_16SRV_CMPL:
            case ESP_BLE_AD_TYPE_32SRV_PART:
            case ESP_BLE_AD_TYPE_32SRV_CMPL:
            case ESP_BLE_AD_TYPE_128SRV_PART:
            case ESP_BLE_AD_TYPE_128SRV_CMPL:
                return mp_obj_new_bytes(data, data_len);
            case ESP_BLE_AD_TYPE_NAME_SHORT:
            case ESP_BLE_AD_TYPE_NAME_CMPL:
                return mp_obj_new_str((char *)data, data_len, false);
            case ESP_BLE_AD_TYPE_TX_PWR:
                return mp_obj_new_int(*(int8_t *)data);
            case ESP_BLE_AD_TYPE_DEV_CLASS:
                break;
            case ESP_BLE_AD_TYPE_SERVICE_DATA:
                return mp_obj_new_bytes(data, data_len);
            case ESP_BLE_AD_TYPE_APPEARANCE:
                break;
            case ESP_BLE_AD_TYPE_ADV_INT:
                break;
            case ESP_BLE_AD_TYPE_32SERVICE_DATA:
                break;
            case ESP_BLE_AD_TYPE_128SERVICE_DATA:
                break;
            case ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE:
                return mp_obj_new_bytes(data, data_len);
            default:
                break;
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(bt_resolve_adv_data_obj, bt_resolve_adv_data);

STATIC mp_obj_t bt_connect(mp_obj_t self_in, mp_obj_t addr) {
    bt_event_result_t bt_event;

    if (bt_obj.busy) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "operation already in progress"));
    } else if (bt_obj.scanning) {
        esp_ble_gap_stop_scanning();
        bt_obj.scanning = false;
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(addr, &bufinfo, MP_BUFFER_READ);

    xQueueReset(xScanQueue);
    bt_obj.busy = true;
    if (ESP_OK != esp_ble_gattc_open(bt_obj.gattc_if, bufinfo.buf, true)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "operation failed"));
    }

    while (bt_obj.busy) {
        mp_hal_delay_ms(2);
    }

    if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
        if (bt_event.connection.conn_id < 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection refused"));
        }

        // setup the object
        bt_connection_obj_t *conn = m_new_obj(bt_connection_obj_t);
        conn->base.type = (mp_obj_t)&mod_bt_connection_type;
        conn->conn_id = bt_event.connection.conn_id;
        memcpy(conn->srv_bda, bt_event.connection.srv_bda, 6);
        mp_obj_list_append((void *)&MP_STATE_PORT(btc_conn_list), conn);
        return conn;
    }

    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection failed"));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_connect_obj, bt_connect);

STATIC mp_obj_t bt_set_advertisement (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_name,                     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_manufacturer_data,        MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_service_data,             MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_service_uuid,             MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    esp_ble_adv_data_t adv_data;
    mp_buffer_info_t manuf_bufinfo;
    mp_buffer_info_t srv_bufinfo;
    mp_buffer_info_t uuid_bufinfo;

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    // device name
    if (args[0].u_obj != mp_const_none) {
        const char *name = mp_obj_str_get_str(args[0].u_obj);
        adv_data.include_name = true;
        esp_ble_gap_set_device_name(name);
    } else {
        adv_data.include_name = false;
        esp_ble_gap_set_device_name(" ");
    }

    // manufacturer data
    if (args[1].u_obj != mp_const_none) {
        mp_get_buffer_raise(args[1].u_obj, &manuf_bufinfo, MP_BUFFER_READ);
        adv_data.manufacturer_len = manuf_bufinfo.len;
        adv_data.p_manufacturer_data = manuf_bufinfo.buf;
    } else {
        adv_data.manufacturer_len = 0;
        adv_data.p_manufacturer_data =  NULL;
    }

    // service data
    if (args[2].u_obj != mp_const_none) {
        mp_get_buffer_raise(args[2].u_obj, &srv_bufinfo, MP_BUFFER_READ);
        adv_data.service_data_len = srv_bufinfo.len;
        adv_data.p_service_data = srv_bufinfo.buf;
    } else {
        adv_data.service_data_len = 0;
        adv_data.p_service_data = NULL;
    }

    // service uuid
    if (args[3].u_obj != mp_const_none) {
        if (mp_obj_is_integer(args[3].u_obj)) {
            uint32_t srv_uuid = mp_obj_get_int_truncated(args[3].u_obj);
            uint8_t uuid_buf[16] = {0};
            memcpy(uuid_buf, (uint8_t *)&srv_uuid, sizeof(uuid_buf));
            adv_data.service_uuid_len = 16;
            adv_data.p_service_uuid = (uint8_t *)&srv_uuid;
        } else {
            mp_get_buffer_raise(args[3].u_obj, &uuid_bufinfo, MP_BUFFER_READ);
            adv_data.service_uuid_len = uuid_bufinfo.len;
            adv_data.p_service_uuid = uuid_bufinfo.buf;
            if (adv_data.service_uuid_len != 16) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "incorrect service UUID length"));
            }
        }
    } else {
        adv_data.service_uuid_len = 0;
        adv_data.p_service_uuid = NULL;
    }

    adv_data.set_scan_rsp = false;
    adv_data.include_txpower = true,
    adv_data.min_interval = 0x20;
    adv_data.max_interval = 0x40;
    adv_data.appearance = 0x00;
    adv_data.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

    esp_ble_gap_config_adv_data(&adv_data);

    // wait for the advertisement data to be configured
    bt_gatts_event_result_t gatts_event;
    xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_set_advertisement_obj, 1, bt_set_advertisement);

STATIC mp_obj_t bt_advertise(mp_obj_t self_in, mp_obj_t enable) {
    if (mp_obj_is_true(enable)) {
        esp_ble_gap_start_advertising(&bt_adv_params);
        bt_obj.advertising = true;
    } else {
        esp_ble_gap_stop_advertising();
        bt_obj.advertising = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_advertise_obj, bt_advertise);

STATIC mp_obj_t bt_service (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uuid,                     MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_isprimary,                MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_nbr_chars,                MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 1} },
        { MP_QSTR_start,                    MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = true} },
    };

    mp_buffer_info_t uuid_bufinfo;
    esp_gatt_srvc_id_t service_id;

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    // service uuid
    if (mp_obj_is_integer(args[0].u_obj)) {
        uint32_t srv_uuid = mp_obj_get_int_truncated(args[0].u_obj);
        if (srv_uuid > UINT16_MAX) {
            service_id.id.uuid.len = 4;
            service_id.id.uuid.uuid.uuid32 = srv_uuid;
        } else {
            service_id.id.uuid.len = 2;
            service_id.id.uuid.uuid.uuid16 = srv_uuid;
        }
    } else {
        mp_get_buffer_raise(args[0].u_obj, &uuid_bufinfo, MP_BUFFER_READ);
        if (uuid_bufinfo.len != 16) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid service uuid length"));
        }
        service_id.id.uuid.len = uuid_bufinfo.len;
        memcpy(service_id.id.uuid.uuid.uuid128, uuid_bufinfo.buf, sizeof(service_id.id.uuid.uuid.uuid128));
    }

    service_id.is_primary = args[1].u_bool;
    service_id.id.inst_id = 0x00;

    esp_ble_gatts_create_service(bt_obj.gatts_if, &service_id, (args[2].u_int * 3) + 1);

    bt_gatts_event_result_t gatts_event;
    xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);

    bt_gatts_srv_obj_t *srv = m_new_obj(bt_gatts_srv_obj_t);
    srv->base.type = (mp_obj_t)&mod_bt_gatts_service_type;
    srv->handle = gatts_event.service_handle;

    if (args[3].u_bool) {
        esp_ble_gatts_start_service(gatts_event.service_handle);
        srv->started = true;
    }

    mp_obj_list_append((mp_obj_t)&MP_STATE_PORT(bts_srv_list), srv);

    return srv;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_service_obj, 1, bt_service);

STATIC mp_obj_t bt_service_start(mp_obj_t self_in) {
    bt_gatts_srv_obj_t *self = self_in;
    if (!self->started) {
        if (ESP_OK != esp_ble_gatts_start_service(self->handle)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "service start failed"));
        }
        self->started = true;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_service_start_obj, bt_service_start);

STATIC mp_obj_t bt_service_stop(mp_obj_t self_in) {
    bt_gatts_srv_obj_t *self = self_in;
    if (self->started) {
        if (ESP_OK != esp_ble_gatts_stop_service(self->handle)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "service stop failed"));
        }
        self->started = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_service_stop_obj, bt_service_stop);

STATIC mp_obj_t bt_characteristic (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uuid,                     MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ },
        { MP_QSTR_permissions,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_properties,               MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_value,                    MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    bt_gatts_srv_obj_t *self = pos_args[0];

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    mp_buffer_info_t uuid_bufinfo;
    esp_bt_uuid_t char_uuid;

    // characteristic uuid
    if (mp_obj_is_integer(args[0].u_obj)) {
        uint32_t srv_uuid = mp_obj_get_int_truncated(args[0].u_obj);
        if (srv_uuid > UINT16_MAX) {
            char_uuid.len = 4;
            char_uuid.uuid.uuid32 = srv_uuid;
        } else {
            char_uuid.len = 2;
            char_uuid.uuid.uuid16 = srv_uuid;
        }
    } else {
        mp_get_buffer_raise(args[0].u_obj, &uuid_bufinfo, MP_BUFFER_READ);
        if (uuid_bufinfo.len != 16) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid service uuid length"));
        }
        char_uuid.len = uuid_bufinfo.len;
        memcpy(char_uuid.uuid.uuid128, uuid_bufinfo.buf, sizeof(char_uuid.uuid.uuid128));
    }

    uint32_t permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
    if (args[1].u_obj != mp_const_none) {
        permissions = mp_obj_get_int(args[1].u_obj);
    }

    uint32_t properties = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
    if (args[2].u_obj != mp_const_none) {
        properties = mp_obj_get_int(args[2].u_obj);
    }

    bt_gatts_char_obj_t *characteristic = m_new_obj(bt_gatts_char_obj_t);
    characteristic->attr_obj.base.type = (mp_obj_t)&mod_bt_gatts_char_type;
    characteristic->attr_obj.parent = self;
    characteristic->attr_obj.is_char = true;

    if (args[3].u_obj != mp_const_none) {
        // characteristic value
        if (mp_obj_is_integer(args[3].u_obj)) {
            uint32_t value = mp_obj_get_int_truncated(args[3].u_obj);
            if (value > UINT16_MAX) {
                characteristic->attr_obj.value_len = 4;
                value = lwip_htonl(value);
            } else if (value > UINT8_MAX) {
                characteristic->attr_obj.value_len = 2;
                value = lwip_htons(value);
            } else {
                characteristic->attr_obj.value_len = 1;
            }
            memcpy(characteristic->attr_obj.value, &value, sizeof(value));
        } else {
            mp_buffer_info_t value_bufinfo;
            mp_get_buffer_raise(args[3].u_obj, &value_bufinfo, MP_BUFFER_READ);
            uint16_t write_len = value_bufinfo.len > BT_CHAR_VALUE_SIZE_MAX ? BT_CHAR_VALUE_SIZE_MAX : value_bufinfo.len;
            memcpy(characteristic->attr_obj.value, value_bufinfo.buf, write_len);
            characteristic->attr_obj.value_len = write_len;
        }
    } else {
        characteristic->attr_obj.value[0] = 0;
        characteristic->attr_obj.value_len = 1;
    }

    esp_ble_gatts_add_char(self->handle, &char_uuid, permissions, properties, NULL, NULL);

    bt_gatts_event_result_t gatts_event;
    xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);

    characteristic->attr_obj.handle = gatts_event.char_handle;
    memcpy(&characteristic->attr_obj.uuid, &char_uuid, sizeof(char_uuid));

    mp_obj_list_append((mp_obj_t)&MP_STATE_PORT(bts_attr_list), characteristic);

    bt_gatts_attr_obj_t *descriptor = m_new_obj(bt_gatts_attr_obj_t);
    descriptor->uuid.len = ESP_UUID_LEN_16;
    descriptor->uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

    esp_ble_gatts_add_char_descr(self->handle, &descriptor->uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);

    xQueueReceive(xGattsQueue, &gatts_event, portMAX_DELAY);

    descriptor->base.type = (mp_obj_t)&mod_bt_gatts_char_type;
    descriptor->handle = gatts_event.char_descr_handle;
    descriptor->parent = characteristic;
    descriptor->is_char = false;
    descriptor->value_len = 2;
    descriptor->value[0] = 0;
    descriptor->value[1] = 0;

    mp_obj_list_append((mp_obj_t)&MP_STATE_PORT(bts_attr_list), descriptor);

    return characteristic;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(bt_characteristic_obj, 1, bt_characteristic);


STATIC const mp_map_elem_t bt_gatts_service_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_characteristic),          (mp_obj_t)&bt_characteristic_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_start),                   (mp_obj_t)&bt_service_start_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_stop),                    (mp_obj_t)&bt_service_stop_obj },
};
STATIC MP_DEFINE_CONST_DICT(bt_gatts_service_locals_dict, bt_gatts_service_locals_dict_table);

static const mp_obj_type_t mod_bt_gatts_service_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTSService,
    .locals_dict = (mp_obj_t)&bt_gatts_service_locals_dict,
};

STATIC mp_obj_t bt_characteristic_value (mp_uint_t n_args, const mp_obj_t *args) {
    bt_gatts_char_obj_t *self = args[0];
    if (n_args == 1) {
        // get
        return mp_obj_new_bytes(self->attr_obj.value, self->attr_obj.value_len);
    } else {
        // set
        if (mp_obj_is_integer(args[1])) {
            uint32_t value = mp_obj_get_int_truncated(args[1]);
            memcpy(self->attr_obj.value, &value, sizeof(value));
            if (value > 0xFF) {
                self->attr_obj.value_len = 2;
            } else if (value > 0xFFFF) {
                self->attr_obj.value_len = 4;
            } else {
                self->attr_obj.value_len = 1;
            }
            esp_ble_gatts_send_indicate(bt_obj.gatts_if, bt_obj.gatts_conn_id, self->attr_obj.handle, 
                                        self->attr_obj.value_len, self->attr_obj.value, false);
        } else {
            mp_buffer_info_t value_bufinfo;
            mp_get_buffer_raise(args[1], &value_bufinfo, MP_BUFFER_READ);
            uint8_t value_len = value_bufinfo.len > BT_CHAR_VALUE_SIZE_MAX ? BT_CHAR_VALUE_SIZE_MAX : value_bufinfo.len;
            memcpy(self->attr_obj.value, value_bufinfo.buf, value_len);
            self->attr_obj.value_len = value_len;
        }
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(bt_characteristic_value_obj, 1, 2, bt_characteristic_value);

STATIC const mp_map_elem_t bt_gatts_char_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_value),          (mp_obj_t)&bt_characteristic_value_obj },
};
STATIC MP_DEFINE_CONST_DICT(bt_gatts_char_locals_dict, bt_gatts_char_locals_dict_table);

static const mp_obj_type_t mod_bt_gatts_char_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTSCharacteristic,
    .locals_dict = (mp_obj_t)&bt_gatts_char_locals_dict,
};

STATIC mp_obj_t bt_gatts_disconnect_client(mp_obj_t self_in) {
    if (bt_obj.gatts_conn_id >= 0) {
        esp_ble_gatts_close(bt_obj.gatts_if, bt_obj.gatts_conn_id);
        bt_obj.gatts_conn_id = -1;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_gatts_disconnect_client_obj, bt_gatts_disconnect_client);

STATIC const mp_map_elem_t bt_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                    (mp_obj_t)&bt_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),                  (mp_obj_t)&bt_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_start_scan),              (mp_obj_t)&bt_start_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isscanning),              (mp_obj_t)&bt_isscanning_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_stop_scan),               (mp_obj_t)&bt_stop_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_adv),                 (mp_obj_t)&bt_read_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_resolve_adv_data),        (mp_obj_t)&bt_resolve_adv_data_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),                 (mp_obj_t)&bt_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_advertisement),       (mp_obj_t)&bt_set_advertisement_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_advertise),               (mp_obj_t)&bt_advertise_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_service),                 (mp_obj_t)&bt_service_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect_client),       (mp_obj_t)&bt_gatts_disconnect_client_obj },

    // constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_CONN_ADV),                MP_OBJ_NEW_SMALL_INT(ESP_BLE_EVT_CONN_ADV) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CONN_DIR_ADV),            MP_OBJ_NEW_SMALL_INT(ESP_BLE_EVT_CONN_DIR_ADV) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_DISC_ADV),                MP_OBJ_NEW_SMALL_INT(ESP_BLE_EVT_DISC_ADV) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_NON_CONN_ADV),            MP_OBJ_NEW_SMALL_INT(ESP_BLE_EVT_NON_CONN_ADV) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SCAN_RSP),                MP_OBJ_NEW_SMALL_INT(ESP_BLE_EVT_SCAN_RSP) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_PUBLIC_ADDR),             MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_PUBLIC) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RANDOM_ADDR),             MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_RANDOM) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PUBLIC_RPA_ADDR),         MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_RPA_PUBLIC) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RANDOM_RPA_ADDR),         MP_OBJ_NEW_SMALL_INT(BLE_ADDR_TYPE_RPA_RANDOM) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_FLAG),                MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_FLAG) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_16SRV_PART),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_16SRV_PART) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_T16SRV_CMPL),         MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_16SRV_CMPL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_32SRV_PART),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_32SRV_PART) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_32SRV_CMPL),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_32SRV_CMPL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_128SRV_PART),         MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_128SRV_PART) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_128SRV_CMPL),         MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_128SRV_CMPL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_NAME_SHORT),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_NAME_SHORT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_NAME_CMPL),           MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_NAME_CMPL) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_TX_PWR),              MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_TX_PWR) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_DEV_CLASS),           MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_DEV_CLASS) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_SERVICE_DATA),        MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_SERVICE_DATA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_APPEARANCE),          MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_APPEARANCE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_ADV_INT),             MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_ADV_INT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_32SERVICE_DATA),      MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_32SERVICE_DATA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_128SERVICE_DATA),     MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_TYPE_128SERVICE_DATA) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADV_MANUFACTURER_DATA),   MP_OBJ_NEW_SMALL_INT(ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_BROADCAST),          MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_BROADCAST) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_READ),               MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_READ) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_WRITE_NR),           MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_WRITE_NR) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_WRITE),              MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_WRITE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_NOTIFY),             MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_NOTIFY) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_INDICATE),           MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_INDICATE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_AUTH),               MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_AUTH) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PROP_EXT_PROP),           MP_OBJ_NEW_SMALL_INT(ESP_GATT_CHAR_PROP_BIT_EXT_PROP) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_NEW_ADV_EVENT),           MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTC_ADV_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CLIENT_CONNECTED),        MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_CONN_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CLIENT_DISCONNECTED),     MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_DISCONN_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_READ_EVENT),         MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_READ_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_WRITE_EVENT),        MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTS_WRITE_EVT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CHAR_NOTIFY_EVENT),       MP_OBJ_NEW_SMALL_INT(MOD_BT_GATTC_NOTIFY_EVT) },
};
STATIC MP_DEFINE_CONST_DICT(bt_locals_dict, bt_locals_dict_table);

const mp_obj_type_t network_bt_type = {
    { &mp_type_type },
    .name = MP_QSTR_Bluetooth,
    .make_new = bt_make_new,
    .locals_dict = (mp_obj_t)&bt_locals_dict,
};

STATIC mp_obj_t bt_conn_isconnected(mp_obj_t self_in) {
    bt_connection_obj_t *self = self_in;

    if (self->conn_id > 0) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_conn_isconnected_obj, bt_conn_isconnected);

STATIC mp_obj_t bt_conn_disconnect(mp_obj_t self_in) {
    bt_connection_obj_t *self = self_in;

    if (self->conn_id > 0) {
        esp_ble_gattc_close(bt_obj.gattc_if, self->conn_id);
        self->conn_id = -1;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_conn_disconnect_obj, bt_conn_disconnect);

STATIC mp_obj_t bt_conn_services (mp_obj_t self_in) {
    bt_connection_obj_t *self = self_in;
    bt_event_result_t bt_event;

    if (self->conn_id > 0) {
        xQueueReset(xScanQueue);
        bt_obj.busy = true;
        bt_obj.conn_id = self->conn_id;
        mp_obj_list_init(&self->srv_list, 0);
        if (ESP_OK != esp_ble_gattc_search_service(bt_obj.gattc_if, self->conn_id, NULL)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "service search failed"));
        }
        while (bt_obj.busy || xQueuePeek(xScanQueue, &bt_event, 0)) {
            while (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
                bt_srv_obj_t *srv = m_new_obj(bt_srv_obj_t);
                srv->base.type = (mp_obj_t)&mod_bt_service_type;
                srv->connection = self;
                memcpy(&srv->srv_id, &bt_event.service, sizeof(esp_gatt_srvc_id_t));
                mp_obj_list_append(&self->srv_list, srv);
            }
        }
        return &self->srv_list;
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_conn_services_obj, bt_conn_services);

STATIC const mp_map_elem_t bt_connection_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected),             (mp_obj_t)&bt_conn_isconnected_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect),              (mp_obj_t)&bt_conn_disconnect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_services),                (mp_obj_t)&bt_conn_services_obj },

};
STATIC MP_DEFINE_CONST_DICT(bt_connection_locals_dict, bt_connection_locals_dict_table);

static const mp_obj_type_t mod_bt_connection_type = {
     { &mp_type_type },
    .name = MP_QSTR_GATTCConnection,
    .locals_dict = (mp_obj_t)&bt_connection_locals_dict,
};

STATIC mp_obj_t bt_srv_isprimary(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;

    if (self->srv_id.is_primary) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_isprimary_obj, bt_srv_isprimary);

STATIC mp_obj_t bt_srv_uuid(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;

    if (self->srv_id.id.uuid.len == ESP_UUID_LEN_16) {
        return mp_obj_new_int(self->srv_id.id.uuid.uuid.uuid16);
    } else if (self->srv_id.id.uuid.len == ESP_UUID_LEN_32) {
        return mp_obj_new_int(self->srv_id.id.uuid.uuid.uuid32);
    } else {
        return mp_obj_new_bytes(self->srv_id.id.uuid.uuid.uuid128, ESP_UUID_LEN_128);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_uuid_obj, bt_srv_uuid);

STATIC mp_obj_t bt_srv_instance(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;
    return mp_obj_new_int(self->srv_id.id.inst_id);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_instance_obj, bt_srv_instance);

STATIC mp_obj_t bt_srv_characteristics(mp_obj_t self_in) {
    bt_srv_obj_t *self = self_in;
    bt_event_result_t bt_event;

    if (self->connection->conn_id > 0) {
        xQueueReset(xScanQueue);
        bt_obj.busy = true;
        bt_obj.conn_id = self->connection->conn_id;
        mp_obj_list_init(&self->char_list, 0);
        if (ESP_OK != esp_ble_gattc_get_characteristic(bt_obj.gattc_if, self->connection->conn_id, &self->srv_id, NULL)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "characteristic search failed"));
        }
        while (bt_obj.busy || xQueuePeek(xScanQueue, &bt_event, 0)) {
            while (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
                bt_char_obj_t *chr = m_new_obj(bt_char_obj_t);
                chr->base.type = (mp_obj_t)&mod_bt_characteristic_type;
                chr->service = self;
                memcpy(&chr->characteristic.char_id, &bt_event.characteristic.char_id, sizeof(esp_gatt_id_t));
                chr->characteristic.char_prop = bt_event.characteristic.char_prop;
                mp_obj_list_append(&self->char_list, chr);
            }
        }
        return &self->char_list;
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_srv_characteristics_obj, bt_srv_characteristics);

STATIC const mp_map_elem_t bt_service_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_isprimary),               (mp_obj_t)&bt_srv_isprimary_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_uuid),                    (mp_obj_t)&bt_srv_uuid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_instance),                (mp_obj_t)&bt_srv_instance_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_characteristics),         (mp_obj_t)&bt_srv_characteristics_obj },
};
STATIC MP_DEFINE_CONST_DICT(bt_service_locals_dict, bt_service_locals_dict_table);

static const mp_obj_type_t mod_bt_service_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTCService,
    .locals_dict = (mp_obj_t)&bt_service_locals_dict,
};

STATIC mp_obj_t bt_char_uuid(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;

    if (self->characteristic.char_id.uuid.len == ESP_UUID_LEN_16) {
        return mp_obj_new_int(self->characteristic.char_id.uuid.uuid.uuid16);
    } else if (self->characteristic.char_id.uuid.len == ESP_UUID_LEN_32) {
        return mp_obj_new_int(self->characteristic.char_id.uuid.uuid.uuid32);
    } else {
        return mp_obj_new_bytes(self->characteristic.char_id.uuid.uuid.uuid128, ESP_UUID_LEN_128);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_uuid_obj, bt_char_uuid);

STATIC mp_obj_t bt_char_instance(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;
    return mp_obj_new_int(self->characteristic.char_id.inst_id);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_instance_obj, bt_char_instance);

STATIC mp_obj_t bt_char_properties(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;
    return mp_obj_new_int(self->characteristic.char_prop);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_properties_obj, bt_char_properties);

STATIC mp_obj_t bt_char_read(mp_obj_t self_in) {
    bt_char_obj_t *self = self_in;
    bt_event_result_t bt_event;

    if (self->service->connection->conn_id > 0) {
        xQueueReset(xScanQueue);
        bt_obj.busy = true;
        bt_obj.conn_id = self->service->connection->conn_id;

        if (ESP_OK != esp_ble_gattc_read_char (bt_obj.gattc_if, self->service->connection->conn_id,
                                               &self->service->srv_id,
                                               &self->characteristic.char_id,
                                               ESP_GATT_AUTH_REQ_NONE)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "characteristic read failed"));
        }
        while (bt_obj.busy) {
            mp_hal_delay_ms(5);
        }
        if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
            return mp_obj_new_bytes(bt_event.read.value, bt_event.read.value_len);
        }
    }
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "connection already closed"));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bt_char_read_obj, bt_char_read);

STATIC mp_obj_t bt_char_write(mp_obj_t self_in, mp_obj_t value) {
    bt_char_obj_t *self = self_in;
    bt_event_result_t bt_event;

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(value, &bufinfo, MP_BUFFER_READ);

    if (self->service->connection->conn_id > 0) {
        xQueueReset(xScanQueue);
        bt_obj.busy = true;
        bt_obj.conn_id = self->service->connection->conn_id;

        if (ESP_OK != esp_ble_gattc_write_char (bt_obj.gattc_if, self->service->connection->conn_id,
                                                &self->service->srv_id,
                                                &self->characteristic.char_id,
                                                bufinfo.len,
                                                bufinfo.buf,
                                                ESP_GATT_WRITE_TYPE_NO_RSP,
                                                ESP_GATT_AUTH_REQ_NONE)) {
            goto error;
        }

        while (bt_obj.busy) {
            mp_hal_delay_ms(5);
        }
        if (xQueueReceive(xScanQueue, &bt_event, (TickType_t)5)) {
            if (bt_event.write.status != ESP_GATT_OK) {
                goto error;
            }
        } else {
            goto error;
        }
    } else {
        goto error;
    }
    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "characteristic write failed"));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bt_char_write_obj, bt_char_write);

STATIC const mp_map_elem_t bt_characteristic_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR_uuid),                    (mp_obj_t)&bt_char_uuid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_instance),                (mp_obj_t)&bt_char_instance_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_properties),              (mp_obj_t)&bt_char_properties_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read),                    (mp_obj_t)&bt_char_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_write),                   (mp_obj_t)&bt_char_write_obj },
};
STATIC MP_DEFINE_CONST_DICT(bt_characteristic_locals_dict, bt_characteristic_locals_dict_table);

static const mp_obj_type_t mod_bt_characteristic_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTCCharacteristic,
    .locals_dict = (mp_obj_t)&bt_characteristic_locals_dict,
};
