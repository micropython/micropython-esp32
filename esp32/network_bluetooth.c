/* 
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 "Eric Poulsen" <eric@zyxod.com>
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "py/runtime.h"
#include "py/runtime0.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "py/objstr.h"
#include "modmachine.h"

#include "bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_gatts_api.h"
#include "esp_gattc_api.h"


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define EVENT_DEBUG


#ifdef EVENT_DEBUG
#   define EVENT_DEBUG_GATTC
#   define EVENT_DEBUG_GATTS
#   define EVENT_DEBUG_GAP
#endif

#define UUID_LEN 16
#define NETWORK_BLUETOOTH_DEBUG_PRINTF(args...) printf(args)

#if 1
#   define MALLOC(x) pvPortMalloc(x)
#   define FREE(x) vPortFree(x)
#else
#   define MALLOC(x) NULL
#   define FREE(x) 
#endif

const mp_obj_type_t network_bluetooth_type;
const mp_obj_type_t network_bluetooth_connection_type;
const mp_obj_type_t network_bluetooth_gatts_service_type;
const mp_obj_type_t network_bluetooth_gattc_service_type;
const mp_obj_type_t network_bluetooth_characteristic_type;

STATIC SemaphoreHandle_t item_mut;
STATIC bool network_bluetooth_bt_controller_enabled = false;


#define MP_OBJ_IS_BT_DATATYPE(O) (MP_OBJ_IS_STR_OR_BYTES(O) || MP_OBJ_IS_TYPE(O, &mp_type_bytearray) || (O == mp_const_none))


#define ITEM_BEGIN() assert(xSemaphoreTake(item_mut, portMAX_DELAY) == pdTRUE)
#define ITEM_END() xSemaphoreGive(item_mut)

// CONSTANTS

typedef enum {
    // GAP / GATTC events
    NETWORK_BLUETOOTH_CONNECT,
    NETWORK_BLUETOOTH_DISCONNECT,
    NETWORK_BLUETOOTH_SCAN_DATA,
    NETWORK_BLUETOOTH_SCAN_CMPL,

    // characteristic events
    NETWORK_BLUETOOTH_READ,
    NETWORK_BLUETOOTH_WRITE,
} network_bluetooth_event_t;

// "Service"
// Structure used for both GATTS and GATTC
typedef struct {
    mp_obj_base_t base;

    esp_gatt_srvc_id_t service_id;
    uint16_t handle;

    mp_obj_t chars;  // list
    bool started;
    bool valid;

} network_bluetooth_service_obj_t; 


// "Connection"

typedef struct {
    mp_obj_base_t           base;
    esp_bd_addr_t           bda;
    uint16_t                conn_id;
    uint16_t                mtu;
    mp_obj_t                services;
    bool                    connected;

} network_bluetooth_connection_obj_t;

// "Char"

typedef struct {
    mp_obj_base_t           base;
    esp_bt_uuid_t           uuid;

    esp_gatt_perm_t         perm;
    esp_gatt_char_prop_t    prop;
    esp_attr_control_t      control;

    uint16_t                handle;

    mp_obj_t                callback;
    mp_obj_t                callback_userdata;

    mp_obj_t                value;

} network_bluetooth_char_obj_t;

// "Bluetooth" Declaration
typedef struct {
    mp_obj_base_t           base;
    enum {
        NETWORK_BLUETOOTH_STATE_DEINIT,
        NETWORK_BLUETOOTH_STATE_INIT
    }                       state;
    bool                    advertising;
    bool                    scanning;
    bool                    gatts_connected;

    uint16_t                conn_id;
    esp_gatt_if_t           gatts_interface;
    esp_gatt_if_t           gattc_interface;


    esp_ble_adv_params_t    adv_params;
    esp_ble_adv_data_t      adv_data;

    mp_obj_t                services;       // implemented as a list

    mp_obj_t                callback;
    mp_obj_t                callback_userdata;


} network_bluetooth_obj_t;

// Singleton
STATIC network_bluetooth_obj_t* network_bluetooth_singleton = NULL;


STATIC network_bluetooth_obj_t* network_bluetooth_get_singleton() {

    if (network_bluetooth_singleton == NULL) {
        network_bluetooth_singleton = m_new_obj(network_bluetooth_obj_t);

        network_bluetooth_singleton->base.type = &network_bluetooth_type;

        network_bluetooth_singleton->state = NETWORK_BLUETOOTH_STATE_DEINIT;
        network_bluetooth_singleton->advertising = false;
        network_bluetooth_singleton->scanning = false;
        network_bluetooth_singleton->gatts_connected = false;
        network_bluetooth_singleton->conn_id = 0;
        network_bluetooth_singleton->gatts_interface = ESP_GATT_IF_NONE;
        network_bluetooth_singleton->gattc_interface = ESP_GATT_IF_NONE;
        network_bluetooth_singleton->adv_params.adv_int_min = 1280 * 1.6;
        network_bluetooth_singleton->adv_params.adv_int_max = 1280 * 1.6;
        network_bluetooth_singleton->adv_params.adv_type = ADV_TYPE_IND;
        network_bluetooth_singleton->adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
        network_bluetooth_singleton->adv_params.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;
        network_bluetooth_singleton->adv_params.channel_map = ADV_CHNL_ALL; 
        network_bluetooth_singleton->adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
        network_bluetooth_singleton->adv_data.set_scan_rsp = false;
        network_bluetooth_singleton->adv_data.include_name = false;
        network_bluetooth_singleton->adv_data.include_txpower = false;
        network_bluetooth_singleton->adv_data.min_interval = 1280 * 1.6;
        network_bluetooth_singleton->adv_data.max_interval = 1280 * 1.6;
        network_bluetooth_singleton->adv_data.appearance = 0;
        network_bluetooth_singleton->adv_data.p_manufacturer_data = NULL;
        network_bluetooth_singleton->adv_data.manufacturer_len = 0;
        network_bluetooth_singleton->adv_data.p_service_data = NULL;
        network_bluetooth_singleton->adv_data.service_data_len = 0;
        network_bluetooth_singleton->adv_data.p_service_uuid = 0;
        network_bluetooth_singleton->adv_data.flag = 0;
        
        network_bluetooth_singleton->callback = mp_const_none;
        network_bluetooth_singleton->callback_userdata = mp_const_none;

        network_bluetooth_singleton->services = mp_obj_new_list(0, NULL);
        memset(network_bluetooth_singleton->adv_params.peer_addr, 0, sizeof(network_bluetooth_singleton->adv_params.peer_addr));
    }
    return network_bluetooth_singleton;
}

typedef struct {
    network_bluetooth_event_t event;
    union {
        struct {
            uint16_t        conn_id;
            esp_bd_addr_t   bda;
        } connect;

        struct {
            uint16_t        conn_id;
            esp_bd_addr_t   bda;
        } disconnect;

        struct {
            esp_bd_addr_t   bda;
            uint8_t*        adv_data; // Need to free this!
            uint8_t         adv_data_len;
        } scan_data;

        struct {
            uint16_t        handle;
            uint16_t        conn_id;
            uint32_t        trans_id;
            bool            need_rsp;
        } read;

        struct {
            uint16_t        handle;
            uint16_t        conn_id;
            uint32_t        trans_id;
            bool            need_rsp;
            uint8_t*        value; // Need to free this!
            size_t          value_len;
        } write;
    };

} callback_data_t;

#define CALLBACK_QUEUE_SIZE 10

STATIC QueueHandle_t callback_q = NULL;

STATIC size_t cbq_count() {
    return uxQueueMessagesWaiting(callback_q);
}


STATIC bool cbq_push (const callback_data_t* data, bool block) {
    //NETWORK_BLUETOOTH_DEBUG_PRINTF("push: Qsize: %u\n", cbq_count());
    return xQueueSend(callback_q, data, block ? portMAX_DELAY : 10 / portTICK_PERIOD_MS) == pdPASS;
}

STATIC bool cbq_pop (callback_data_t* data) {
    return xQueueReceive(callback_q, data, 0) == pdPASS;
}

STATIC void dumpBuf(const uint8_t *buf, size_t len) {
    while(len--) 
        printf("%02X ", *buf++);
}

STATIC void parse_uuid(mp_obj_t src, esp_bt_uuid_t* target) {

    if (MP_OBJ_IS_INT(src)) {
        uint32_t arg_uuid = mp_obj_get_int(src);
        target->len = arg_uuid == (arg_uuid & 0xFFFF) ? sizeof(uint16_t) : sizeof(uint32_t);
        uint8_t * uuid_ptr = (uint8_t*)&target->uuid.uuid128;
        for(int i = 0; i < target->len; i++) {
            // LSB first
            *uuid_ptr++ = arg_uuid & 0xff; 
            arg_uuid >>= 8;
       }
    } else if (mp_obj_get_type(src) != &mp_type_bytearray) {
        goto PARSE_UUID_BAD;
    } else {
        mp_buffer_info_t buf;
        mp_get_buffer(src, &buf, MP_BUFFER_READ);
        if (buf.len != UUID_LEN) {
            goto PARSE_UUID_BAD;
        }
        target->len = UUID_LEN;
        memcpy(&target->uuid.uuid128, buf.buf, buf.len);
    }

    return;
PARSE_UUID_BAD:
    mp_raise_ValueError("uuid must be integer or bytearray(16)");
}

static void uuid_to_uuid128(const esp_bt_uuid_t* in, esp_bt_uuid_t* out) {

    static const uint8_t base_uuid[] = 
    {	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
        0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB };
    
    if (in->len == 16) {
        *out = *in;
        return;
    } 
    
    out->len = 16;
    memcpy(out->uuid.uuid128, base_uuid, MP_ARRAY_SIZE(base_uuid));

    if (in->len == 2) {
        uint16_t t = htons(in->uuid.uuid16);
        memcpy(out->uuid.uuid128 + sizeof(t), &t, sizeof(t));
    } else {
        uint32_t t = htonl(in->uuid.uuid32);
        memcpy(out->uuid.uuid128, &t, sizeof(t));
    }

}

STATIC bool uuid_eq(const esp_bt_uuid_t* a, const esp_bt_uuid_t* b) {
    if (a->len != b->len) {
        return false;
    }
    return memcmp(&a->uuid.uuid128, &b->uuid.uuid128, a->len) == 0;
}

STATIC void network_bluetooth_uuid_print(const mp_print_t *print, const esp_bt_uuid_t* uuid) {
    esp_bt_uuid_t uuid128;
    uuid_to_uuid128(uuid, &uuid128);
    for(int i = 0; i < uuid128.len; i++) {
        if (print != NULL) {
            mp_printf(print, "%02X", uuid128.uuid.uuid128[i]);
        } else {
            NETWORK_BLUETOOTH_DEBUG_PRINTF("%02X", uuid128.uuid.uuid128[i]);
        }
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            if (print != NULL) {
                mp_printf(print, "-");
            } else  {
                NETWORK_BLUETOOTH_DEBUG_PRINTF("-");
            }
        }
    }
}

typedef enum {
    NETWORK_BLUETOOTH_FIND_SERVICE,
    NETWORK_BLUETOOTH_FIND_CHAR,
    NETWORK_BLUETOOTH_FIND_CHAR_IN_SERVICES,
    NETWORK_BLUETOOTH_DEL_SERVICE,
} item_op_t;


STATIC mp_obj_t network_bluetooth_item_op(mp_obj_t list, esp_bt_uuid_t* uuid, uint16_t handle, item_op_t kind) {
    NETWORK_BLUETOOTH_DEBUG_PRINTF("network_bluetooth_item_op, list is %p\n", list);

    ITEM_BEGIN();
    mp_obj_t ret = MP_OBJ_NULL;

    size_t len;
    mp_obj_t *items;
    mp_obj_get_array(list, &len, &items);
    for(int i = 0; i < len; i++) {
        switch(kind) {
            case NETWORK_BLUETOOTH_FIND_SERVICE: 
            case NETWORK_BLUETOOTH_FIND_CHAR_IN_SERVICES:
            case NETWORK_BLUETOOTH_DEL_SERVICE:
                {
                    network_bluetooth_service_obj_t* service = (network_bluetooth_service_obj_t*) items[i];

                    if (kind == NETWORK_BLUETOOTH_FIND_CHAR_IN_SERVICES) {
                        size_t char_len;
                        mp_obj_t *char_items;
                        mp_obj_get_array(service->chars, &char_len, &char_items);
                        for(int j = 0; j < char_len; j++) {
                            network_bluetooth_char_obj_t* chr = (network_bluetooth_char_obj_t*) char_items[j];
                            if ((uuid != NULL && uuid_eq(&chr->uuid, uuid)) || (uuid == NULL && chr->handle == handle)) {
                                ret = chr;
                                goto NETWORK_BLUETOOTH_ITEM_END;
                            }                     
                        }

                    } else if ((uuid != NULL && uuid_eq(&service->service_id.id.uuid, uuid)) || (uuid == NULL && service->handle == handle)) {
                        ret = service;

                        if (kind == NETWORK_BLUETOOTH_DEL_SERVICE) {
                            // a bit ineffecient, but
                            // list_pop() is static
                            mp_obj_list_remove(list, service);
                        }
                        goto NETWORK_BLUETOOTH_ITEM_END;
                    }
                }
                break;

            case NETWORK_BLUETOOTH_FIND_CHAR:
                {
                    network_bluetooth_char_obj_t* chr = (network_bluetooth_char_obj_t*) items[i];
                    if ((uuid != NULL && uuid_eq(&chr->uuid, uuid)) || (uuid == NULL && chr->handle == handle)) {
                        ret = chr;
                        goto NETWORK_BLUETOOTH_ITEM_END;
                    }                     
                }
                break;
        }
    }
NETWORK_BLUETOOTH_ITEM_END:
    ITEM_END();
    return ret;
}


STATIC bool check_locals_dict(mp_obj_t self, qstr attr, mp_obj_t *dest) {
    mp_obj_type_t *type = mp_obj_get_type(self);

    if (type->locals_dict != NULL) {
        mp_map_t *locals_map = &type->locals_dict->map;
        mp_map_elem_t *elem = mp_map_lookup(locals_map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
        if (elem != NULL && elem->value != NULL) {
            mp_convert_member_lookup(self, type, elem->value, dest);
            return true;
        } 
    }
    return false;
}

#ifdef EVENT_DEBUG 

    typedef struct {
        uint8_t id; 
        const char * name;
    } gatt_status_name_t;

STATIC const gatt_status_name_t gatt_status_names[] = {
        {0x00, "OK"},
        {0x01, "INVALID_HANDLE"},
        {0x02, "READ_NOT_PERMIT"},
        {0x03, "WRITE_NOT_PERMIT"},
        {0x04, "INVALID_PDU"},
        {0x05, "INSUF_AUTHENTICATION"},
        {0x06, "REQ_NOT_SUPPORTED"},
        {0x07, "INVALID_OFFSET"},
        {0x08, "INSUF_AUTHORIZATION"},
        {0x09, "PREPARE_Q_FULL"},
        {0x0a, "NOT_FOUND"},
        {0x0b, "NOT_LONG"},
        {0x0c, "INSUF_KEY_SIZE"},
        {0x0d, "INVALID_ATTR_LEN"},
        {0x0e, "ERR_UNLIKELY"},
        {0x0f, "INSUF_ENCRYPTION"},
        {0x10, "UNSUPPORT_GRP_TYPE"},
        {0x11, "INSUF_RESOURCE"},
        {0x80, "NO_RESOURCES"},
        {0x81, "INTERNAL_ERROR"},
        {0x82, "WRONG_STATE"},
        {0x83, "DB_FULL"},
        {0x84, "BUSY"},
        {0x85, "ERROR"},
        {0x86, "CMD_STARTED"},
        {0x87, "ILLEGAL_PARAMETER"},
        {0x88, "PENDING"},
        {0x89, "AUTH_FAIL"},
        {0x8a, "MORE"},
        {0x8b, "INVALID_CFG"},
        {0x8c, "SERVICE_STARTED"},
        {0x8d, "ENCRYPED_NO_MITM"},
        {0x8e, "NOT_ENCRYPTED"},
        {0x8f, "CONGESTED"},
        {0x90, "DUP_REG"},
        {0x91, "ALREADY_OPEN"},
        {0x92, "CANCEL"},
        {0xfd, "CCC_CFG_ERR"},
        {0xfe, "PRC_IN_PROGRESS"},
        {0xff, "OUT_OF_RANGE"},
        {0x00,  NULL},
    };

#define PRINT_STATUS(STATUS) { \
    bool found = false; \
    NETWORK_BLUETOOTH_DEBUG_PRINTF("status = %02X / ", (STATUS)); \
    for(int i = 0; gatt_status_names[i].name != NULL; i++) { \
        if (gatt_status_names[i].id == (STATUS)) { \
            found = true; \
            NETWORK_BLUETOOTH_DEBUG_PRINTF("%s", gatt_status_names[i].name); \
            break; \
        } \
    } \
    if (!found) { \
        NETWORK_BLUETOOTH_DEBUG_PRINTF("???"); \
    }  \
}


STATIC void gattc_event_dump(
        esp_gattc_cb_event_t event, 
        esp_gatt_if_t gattc_if, 
        esp_ble_gattc_cb_param_t *param) {
    const char * event_names[] = {
        "REG",
        "UNREG",
        "OPEN",
        "READ_CHAR",
        "WRITE_CHAR",
        "CLOSE",
        "SEARCH_CMPL",
        "SEARCH_RES",
        "READ_DESCR",
        "WRITE_DESCR",
        "NOTIFY",
        "PREP_WRITE",
        "EXEC",
        "ACL",
        "CANCEL_OPEN",
        "SRVC_CHG",
        "ENC_CMPL_CB",
        "CFG_MTU",
        "ADV_DATA",
        "MULT_ADV_ENB",
        "MULT_ADV_UPD",
        "MULT_ADV_DATA",
        "MULT_ADV_DIS",
        "CONGEST",
        "BTH_SCAN_ENB",
        "BTH_SCAN_CFG",
        "BTH_SCAN_RD",
        "BTH_SCAN_THR",
        "BTH_SCAN_PARAM",
        "BTH_SCAN_DIS",
        "SCAN_FLT_CFG",
        "SCAN_FLT_PARAM",
        "SCAN_FLT_STATUS",
        "ADV_VSC",
        "GET_CHAR",
        "GET_DESCR",
        "GET_INCL_SRVC",
        "REG_FOR_NOTIFY",
        "UNREG_FOR_NOTIFY"
    };

    NETWORK_BLUETOOTH_DEBUG_PRINTF(
            "network_bluetooth_gattc_event_handler("
            "event = %02X / %s"
            ", if = %02X",
            event, event_names[event], gattc_if
            );

    NETWORK_BLUETOOTH_DEBUG_PRINTF( ", param = (");

    switch(event) {
        case ESP_GATTC_REG_EVT:
            {
                PRINT_STATUS(param->reg.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", app_id = %04X",
                        param->reg.app_id
                        );
            }
            break;

        case ESP_GATTC_OPEN_EVT:
            {
                PRINT_STATUS(param->open.status);
            NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    ", conn_id = %04X"
                    ", addr = %02X:%02X:%02X:%02X:%02X:%02X"
                    ", mtu = %04X",
                    param->open.conn_id,
                    param->open.remote_bda[0],
                    param->open.remote_bda[1],
                    param->open.remote_bda[2],
                    param->open.remote_bda[3],
                    param->open.remote_bda[4],
                    param->open.remote_bda[5],
                    param->open.mtu);
            }
            break;

        case ESP_GATTC_CLOSE_EVT:
            {
                PRINT_STATUS(param->close.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        "conn_id = %04X"
                        ", addr = %02X:%02X:%02X:%02X:%02X:%02X"
                        ", reason = ",
                        param->close.conn_id,
                        param->close.remote_bda[0],
                        param->close.remote_bda[1],
                        param->close.remote_bda[2],
                        param->close.remote_bda[3],
                        param->close.remote_bda[4],
                        param->close.remote_bda[5]);
                switch(param->close.reason) {
                    case ESP_GATT_CONN_UNKNOWN:
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("UNKNOWN");
                        break;
                    case ESP_GATT_CONN_L2C_FAILURE:
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("L2C_FAILURE");
                        break;
                    case ESP_GATT_CONN_TIMEOUT:
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("TIMEOUT");
                        break;
                    case ESP_GATT_CONN_TERMINATE_PEER_USER:
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("TERMINATE_PEER_USER");
                        break;
                    case ESP_GATT_CONN_TERMINATE_LOCAL_HOST:
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("TERMINATE_LOCAL_HOST");
                        break;
                    case ESP_GATT_CONN_FAIL_ESTABLISH:
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("FAIL_ESTABLISH");
                        break;
                    case ESP_GATT_CONN_LMP_TIMEOUT:
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("LMP_TIMEOUT");
                        break;
                    case ESP_GATT_CONN_CONN_CANCEL:
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("CONN_CANCEL");
                        break;
                    case ESP_GATT_CONN_NONE:
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("NONE");
                        break;

                }
            }
            break;

        case ESP_GATTC_CFG_MTU_EVT:
            {
                PRINT_STATUS(param->open.status);
            NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    "conn_id = %04X"
                    ", mtu = %04X",
                    param->cfg_mtu.conn_id,
                    param->cfg_mtu.mtu);
            }
            break;

        case ESP_GATTC_SEARCH_CMPL_EVT:
            {
                PRINT_STATUS(param->search_cmpl.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        "conn_id = %04X",
                        param->search_cmpl.conn_id);
            }
            break;

        case ESP_GATTC_SEARCH_RES_EVT:
            {
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        "conn_id = %04X",
                        param->search_res.conn_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", service_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->search_res.srvc_id.id.uuid.uuid, param->search_res.srvc_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->search_res.srvc_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->search_res.srvc_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");
            }
            break;

        case ESP_GATTC_READ_CHAR_EVT:
        case ESP_GATTC_READ_DESCR_EVT:
            {
                PRINT_STATUS(param->read.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF( "conn_id = %04X", param->read.conn_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->read.srvc_id.id.uuid.uuid, param->read.srvc_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->read.srvc_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->read.srvc_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->read.char_id.uuid.uuid, param->read.char_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->read.char_id.inst_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", descr_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->read.descr_id.uuid.uuid, param->read.descr_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->read.descr_id.inst_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", value = (");
                dumpBuf(param->read.value, param->read.value_len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", value_len = %d "
                        ", value_type = %X",
                        param->read.value_len,
                        param->read.value_type);
            }
            break;

            case ESP_GATTC_WRITE_CHAR_EVT:
            case ESP_GATTC_PREP_WRITE_EVT:
            case ESP_GATTC_WRITE_DESCR_EVT:
            {
                PRINT_STATUS(param->write.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF("conn_id = %04X", param->read.conn_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", service_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->write.srvc_id.id.uuid.uuid, param->write.srvc_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->write.srvc_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->write.srvc_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->write.char_id.uuid.uuid, param->write.char_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->write.char_id.inst_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", descr_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->write.descr_id.uuid.uuid, param->write.descr_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->write.descr_id.inst_id);
            }
            break;

            case ESP_GATTC_EXEC_EVT:
            {
                PRINT_STATUS(param->exec_cmpl.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF("conn_id = %04X", param->exec_cmpl.conn_id);
            }
            break;

            case ESP_GATTC_NOTIFY_EVT:
            {
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        "conn_id = %04X"
                        ", addr = %02X:%02X:%02X:%02X:%02X:%02X",
                        param->notify.conn_id,
                        param->notify.remote_bda[0],
                        param->notify.remote_bda[1],
                        param->notify.remote_bda[2],
                        param->notify.remote_bda[3],
                        param->notify.remote_bda[4],
                        param->notify.remote_bda[5]);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->notify.srvc_id.id.uuid.uuid, param->notify.srvc_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->notify.srvc_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->notify.srvc_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->notify.char_id.uuid.uuid, param->notify.char_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->notify.char_id.inst_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", descr_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->notify.descr_id.uuid.uuid, param->notify.descr_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->notify.descr_id.inst_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", value = (");
                dumpBuf(param->notify.value, param->notify.value_len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", value_len = %d "
                        ", %s",
                        param->notify.value_len,
                        param->notify.is_notify ? "NOTIFY" : "INDICATE");

            }
            break;


            case ESP_GATTC_SRVC_CHG_EVT:
            {
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        "addr = %02X:%02X:%02X:%02X:%02X:%02X",
                        param->srvc_chg.remote_bda[0],
                        param->srvc_chg.remote_bda[1],
                        param->srvc_chg.remote_bda[2],
                        param->srvc_chg.remote_bda[3],
                        param->srvc_chg.remote_bda[4],
                        param->srvc_chg.remote_bda[5]);

            }
            break;

            case ESP_GATTC_CONGEST_EVT:
            {
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        "conn_id = %04X, %s CONGESTED",
                        param->congest.conn_id,
                        param->congest.congested ? "" : "NOT");
            }
            break;

            case ESP_GATTC_GET_CHAR_EVT: 
            {
                PRINT_STATUS(param->get_char.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF( "conn_id = %04X", param->get_char.conn_id); 

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->get_char.srvc_id.id.uuid.uuid, param->get_char.srvc_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->get_char.srvc_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->get_char.srvc_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->get_char.char_id.uuid.uuid, param->get_char.char_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->get_char.char_id.inst_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_props = ");
                const char * char_props[] = { "BROADCAST", "READ", "WRITE_NR", "WRITE", "NOTIFY", "INDICATE", "AUTH", "EXT_PROP" };
                bool printed = false;
                for(int i = 0; i < 7; i ++) {
                    if (param->get_char.char_prop & (1<<i)) {
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("%s%s", printed ? " | " : "", char_props[i]);
                        printed = true;
                    }
                }
            }
            break;

            case ESP_GATTC_GET_DESCR_EVT:
            {
                PRINT_STATUS(param->get_descr.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF( "conn_id = %04X", param->get_descr.conn_id); 

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->get_descr.srvc_id.id.uuid.uuid, param->get_descr.srvc_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->get_descr.srvc_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->get_descr.srvc_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->get_descr.char_id.uuid.uuid, param->get_descr.char_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->get_descr.char_id.inst_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", descr_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->get_descr.descr_id.uuid.uuid, param->get_descr.descr_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->get_descr.descr_id.inst_id);
            }
            break;

            case ESP_GATTC_GET_INCL_SRVC_EVT:
            {
                PRINT_STATUS(param->get_incl_srvc.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF( "conn_id = %04X", param->get_incl_srvc.conn_id); 

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->get_incl_srvc.srvc_id.id.uuid.uuid, param->get_incl_srvc.srvc_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->get_incl_srvc.srvc_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->get_incl_srvc.srvc_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", incl_srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->get_incl_srvc.srvc_id.id.uuid.uuid, param->get_incl_srvc.srvc_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->get_incl_srvc.srvc_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->get_incl_srvc.srvc_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

            }
            break;

            case ESP_GATTC_REG_FOR_NOTIFY_EVT:
            {
                PRINT_STATUS(param->reg_for_notify.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->reg_for_notify.srvc_id.id.uuid.uuid, param->reg_for_notify.srvc_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->reg_for_notify.srvc_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->reg_for_notify.srvc_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->reg_for_notify.char_id.uuid.uuid, param->reg_for_notify.char_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->reg_for_notify.char_id.inst_id);
            }
            break;

            case ESP_GATTC_UNREG_FOR_NOTIFY_EVT:
            {
                PRINT_STATUS(param->unreg_for_notify.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->unreg_for_notify.srvc_id.id.uuid.uuid, param->unreg_for_notify.srvc_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->unreg_for_notify.srvc_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->unreg_for_notify.srvc_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->unreg_for_notify.char_id.uuid.uuid, param->unreg_for_notify.char_id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->unreg_for_notify.char_id.inst_id);
            }
            default:
            // Rest of events have no printable data
            // do nothing, intentionally
            break;
    }
NETWORK_BLUETOOTH_DEBUG_PRINTF(")\n");
}

STATIC void gatts_event_dump(
        esp_gatts_cb_event_t event, 
        esp_gatt_if_t gatts_if, 
        esp_ble_gatts_cb_param_t *param) {

    const char * event_names[] = {
        "REG",
        "READ",
        "WRITE",
        "EXEC_WRITE",
        "MTU",
        "CONF",
        "UNREG",
        "CREATE",
        "ADD_INCL_SRVC",
        "ADD_CHAR",
        "ADD_CHAR_DESCR",
        "DELETE",
        "START",
        "STOP",
        "CONNECT",
        "DISCONNECT",
        "OPEN",
        "CANCEL_OPEN",
        "CLOSE",
        "LISTEN",
        "CONGEST",
        "RESPONSE",
        "CREAT_ATTR_TAB",
        "SET_ATTR_VAL",
    };


    NETWORK_BLUETOOTH_DEBUG_PRINTF(
            "network_bluetooth_gatts_event_handler("
            "event = %02X / %s"
            ", if = %02X",
            event, event_names[event], gatts_if
            );

    NETWORK_BLUETOOTH_DEBUG_PRINTF( ", param = (");

    switch(event) {
        case ESP_GATTS_REG_EVT:
            {
                PRINT_STATUS(param->reg.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", app_id = %04X",
                        param->reg.app_id
                        );
            }
            break;

        case ESP_GATTS_READ_EVT:
            NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    "conn_id = %04X"
                    ", trans_id = %08X"
                    ", addr = %02X:%02X:%02X:%02X:%02X:%02X",
                    param->read.conn_id,
                    param->read.trans_id,
                    param->read.bda[0],
                    param->read.bda[1],
                    param->read.bda[2],
                    param->read.bda[3],
                    param->read.bda[4],
                    param->read.bda[5]
                    );

            NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    ", handle = %04X"
                    ", offset = %04X"
                    ", is_long = %s"
                    ", need_resp = %s",
                    param->read.handle,
                    param->read.offset,
                    param->read.is_long ? "true" : "false",
                    param->read.need_rsp ? "true" : "false");

            break;

        case ESP_GATTS_WRITE_EVT:
            {
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        "conn_id = %04X"
                        ", trans_id = %08X"
                        ", addr = %02X:%02X:%02X:%02X:%02X:%02X",
                        param->write.conn_id,
                        param->write.trans_id,
                        param->write.bda[0],
                        param->write.bda[1],
                        param->write.bda[2],
                        param->write.bda[3],
                        param->write.bda[4],
                        param->write.bda[5]
                        );

                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", handle = %04X"
                        ", offset = %04X"
                        ", need_resp = %s"
                        ", is_prep = %s"
                        ", len = %04X",
                        param->write.handle,
                        param->write.offset,
                        param->write.need_rsp ? "true" : "false",
                        param->write.is_prep ? "true" : "false",
                        param->write.len);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", data = ");
                dumpBuf(param->write.value, param->write.len);
            }
            break;

        case ESP_GATTS_EXEC_WRITE_EVT:
            {
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        "conn_id = %04X"
                        ", trans_id = %08X"
                        ", addr = %02X:%02X:%02X:%02X:%02X:%02X"
                        ", exec_write_flag = %02X",
                        param->exec_write.conn_id,
                        param->exec_write.trans_id,
                        param->exec_write.bda[0],
                        param->exec_write.bda[1],
                        param->exec_write.bda[2],
                        param->exec_write.bda[3],
                        param->exec_write.bda[4],
                        param->exec_write.bda[5],
                        param->exec_write.exec_write_flag);
            }
            break;

        case ESP_GATTS_MTU_EVT:
            {
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        "conn_id = %04X"
                        ", mtu = %04X",
                        param->mtu.conn_id,
                        param->mtu.mtu);
            }
            break;

        case ESP_GATTS_CONF_EVT:
            {
                PRINT_STATUS(param->conf.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", conn_id = %04X",
                        param->conf.conn_id
                        );
            }
            break;

        case ESP_GATTS_UNREG_EVT:
            // No params?
            break;

        case ESP_GATTS_CREATE_EVT:
            {
                PRINT_STATUS(param->create.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", service_handle = %04X", param->create.service_handle);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", service_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->create.service_id.id.uuid.uuid, param->create.service_id.id.uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id = %02X)", param->create.service_id.id.inst_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->create.service_id.is_primary ? "true" : "false"); 
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");
            }

            break;

        case ESP_GATTS_ADD_INCL_SRVC_EVT:
            {
                PRINT_STATUS(param->add_incl_srvc.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", attr_handle = %04X"
                        ", service_handle = %04X",
                        param->add_incl_srvc.attr_handle,
                        param->add_incl_srvc.service_handle);
            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            {
                PRINT_STATUS(param->add_char.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", attr_handle = %04X"
                        ", service_handle = %04X",
                        param->add_char.attr_handle,
                        param->add_char.service_handle);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", uuid = ");
                dumpBuf((uint8_t*)&param->add_char.char_uuid.uuid, param->add_char.char_uuid.len);
            }
            break;

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            {
                PRINT_STATUS(param->add_char_descr.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", attr_handle = %04X"
                        ", service_handle = %04X",
                        param->add_char_descr.attr_handle,
                        param->add_char_descr.service_handle);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", uuid = ");
                dumpBuf((uint8_t*)&param->add_char_descr.char_uuid.uuid, param->add_char_descr.char_uuid.len);
            }
            break;

        case ESP_GATTS_DELETE_EVT:
            {
                PRINT_STATUS(param->del.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", service_handle = %04X",
                        param->del.service_handle);
            }
            break;

        case ESP_GATTS_START_EVT:
            {
                PRINT_STATUS(param->start.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", service_handle = %04X",
                        param->start.service_handle);
            }
            break;

        case ESP_GATTS_STOP_EVT:
            {
                PRINT_STATUS(param->stop.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", service_handle = %04X",
                        param->stop.service_handle);
            }
            break;

        case ESP_GATTS_CONNECT_EVT:
            NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    "conn_id = %04X"
                    ", remote_bda = %02X:%02X:%02X:%02X:%02X:%02X"
                    ", is_connected = %s",
                    param->connect.conn_id,
                    param->connect.remote_bda[0],
                    param->connect.remote_bda[1],
                    param->connect.remote_bda[2],
                    param->connect.remote_bda[3],
                    param->connect.remote_bda[4],
                    param->connect.remote_bda[5],
                    param->connect.is_connected ? "true" : "false");
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    "conn_id = %04X"
                    ", remote_bda = %02X:%02X:%02X:%02X:%02X:%02X"
                    ", is_connected = %s",
                    param->disconnect.conn_id,
                    param->disconnect.remote_bda[0],
                    param->disconnect.remote_bda[1],
                    param->disconnect.remote_bda[2],
                    param->disconnect.remote_bda[3],
                    param->disconnect.remote_bda[4],
                    param->disconnect.remote_bda[5],
                    param->disconnect.is_connected ? "true" : "false");
            break;

        case ESP_GATTS_OPEN_EVT:
            // no param?
            break;

        case ESP_GATTS_CANCEL_OPEN_EVT:
            // no param?
            break;

        case ESP_GATTS_CLOSE_EVT:
            // no param?
            break;

        case ESP_GATTS_LISTEN_EVT:
            // no param?
            break;

        case ESP_GATTS_CONGEST_EVT:
            NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    "conn_id = %04X"
                    ", congested = %s",
                    param->congest.conn_id,
                    param->congest.congested ? "true" : "false");
            break;

        case ESP_GATTS_RESPONSE_EVT:
            {
                PRINT_STATUS(param->rsp.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                        ", handle = %04X",
                        param->rsp.handle);
            }
            break;

        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            {
                PRINT_STATUS(param->add_attr_tab.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_uuid = ");
                dumpBuf((uint8_t*)&param->add_attr_tab.svc_uuid.uuid, param->add_attr_tab.svc_uuid.len);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", num_handle = %04X", param->add_attr_tab.num_handle);
                for(int i = 0; i < param->add_attr_tab.num_handle; i++) {
                    NETWORK_BLUETOOTH_DEBUG_PRINTF("%04X ", param->add_attr_tab.handles[i]);
                }
            }
            break;

        case ESP_GATTS_SET_ATTR_VAL_EVT:
            NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    "srvc_handle = %04X"
                    ", attr_handle = %04X",
                    param->set_attr_val.srvc_handle,
                    param->set_attr_val.attr_handle);
            PRINT_STATUS(param->set_attr_val.status);

            break;

    }
NETWORK_BLUETOOTH_DEBUG_PRINTF(")\n");
}

STATIC void gap_event_dump(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    const char * event_names[] = {
        "ADV_DATA_SET_COMPLETE",
        "SCAN_RSP_DATA_SET_COMPLETE",
        "SCAN_PARAM_SET_COMPLETE",
        "SCAN_RESULT",
        "ADV_DATA_RAW_SET_COMPLETE",
        "SCAN_RSP_DATA_RAW_SET_COMPLETE",
        "ADV_START_COMPLETE",
        "SCAN_START_COMPLETE",
        "AUTH_CMPL",
        "KEY",
        "SEC_REQ",
        "PASSKEY_NOTIF",
        "PASSKEY_REQ",
        "OOB_REQ",
        "LOCAL_IR",
        "LOCAL_ER",
        "NC_REQ",
        "ADV_STOP_COMPLETE",
        "SCAN_STOP_COMPLETE"};

    NETWORK_BLUETOOTH_DEBUG_PRINTF("network_bluetooth_gap_event_handler(event = %02X / %s", event, event_names[event]);
    NETWORK_BLUETOOTH_DEBUG_PRINTF( ", param = (");
    switch(event) {

        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            {
                PRINT_STATUS(param->adv_data_cmpl.status);
            }
            break;

        case ESP_GAP_BLE_SCAN_RESULT_EVT: 
            {
                const char * search_events[7] = {
                    "INQ_RES",
                    "INQ_CMPL",
                    "DISC_RES",
                    "DISC_BLE_RES",
                    "DISC_CMPL",
                    "DI_DISC_CMPL",
                    "SEARCH_CANCEL_CMPL"
                };

                const char * dev_types[]  = { "", "BREDR", "BLE", "DUMO" };
                const char * addr_types[] = { "PUBLIC", "RANDOM", "RPA_PUBLIC", "RPA_RANDOM" };


                    NETWORK_BLUETOOTH_DEBUG_PRINTF(
                            "search_evt = %s",
                            search_events[param->scan_rst.search_evt]);
                
                if ( param->scan_rst.dev_type <= 3 && param->scan_rst.ble_addr_type <=3 ) {

                    NETWORK_BLUETOOTH_DEBUG_PRINTF(
                            ", dev_type = %s"
                            ", ble_addr_type = %s"
                            ", rssi = %d" ,
                            dev_types[param->scan_rst.dev_type],
                            addr_types[param->scan_rst.ble_addr_type],
                            param->scan_rst.rssi

                            );
                }

                if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {

                    NETWORK_BLUETOOTH_DEBUG_PRINTF(
                            ", bda = %02X:%02X:%02X:%02X:%02X:%02X"
                            ", adv_data_len = %u"
                            ", scan_rsp_len = %u" ,
                            param->scan_rst.bda[0],
                            param->scan_rst.bda[1],
                            param->scan_rst.bda[2],
                            param->scan_rst.bda[3],
                            param->scan_rst.bda[4],
                            param->scan_rst.bda[5],
                            param->scan_rst.adv_data_len,
                            param->scan_rst.scan_rsp_len);

    
                    uint8_t * adv_name;
                    uint8_t adv_name_len = 0;
                    adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                            ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

                    NETWORK_BLUETOOTH_DEBUG_PRINTF(", adv_name = ");
                    for (int j = 0; j < adv_name_len; j++) {
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("%c", adv_name[j]);
                    }
                    NETWORK_BLUETOOTH_DEBUG_PRINTF("(%d bytes)", adv_name_len);
                }
            }
            break;

        case ESP_GAP_BLE_AUTH_CMPL_EVT: break;
        case ESP_GAP_BLE_KEY_EVT: break;
        case ESP_GAP_BLE_SEC_REQ_EVT: break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT: break;
        case ESP_GAP_BLE_OOB_REQ_EVT: break;
        case ESP_GAP_BLE_LOCAL_IR_EVT: break;
        case ESP_GAP_BLE_LOCAL_ER_EVT: break;
        case ESP_GAP_BLE_NC_REQ_EVT: break;
    }

NETWORK_BLUETOOTH_DEBUG_PRINTF(")\n");

}



#endif // EVENT_DEBUG

STATIC mp_obj_t network_bluetooth_callback_queue_handler (mp_obj_t arg) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        return mp_const_none;
    }

    callback_data_t cbdata;

    while(true) {
        bool got_data = cbq_pop(&cbdata);
        if (!got_data) {
            return mp_const_none;
        }

        switch (cbdata.event) {

            case NETWORK_BLUETOOTH_READ:
            case NETWORK_BLUETOOTH_WRITE:
                {
                    network_bluetooth_char_obj_t* self = network_bluetooth_item_op(bluetooth->services, NULL, cbdata.read.handle, NETWORK_BLUETOOTH_FIND_CHAR_IN_SERVICES);


                    if (self != MP_OBJ_NULL) {

                        mp_obj_t value = cbdata.event == NETWORK_BLUETOOTH_WRITE ? mp_obj_new_bytearray(cbdata.write.value_len, cbdata.write.value) : self->value;

                        if (self->callback != mp_const_none) {
                            mp_obj_t args[] = {self, MP_OBJ_NEW_SMALL_INT(cbdata.event), value, self->callback_userdata };
                            value = mp_call_function_n_kw(self->callback, MP_ARRAY_SIZE(args), 0, args); 
                        } else if (cbdata.event == NETWORK_BLUETOOTH_WRITE) {
                            self->value = value;
                        }

                        if (cbdata.read.need_rsp) {
                            esp_gatt_rsp_t rsp;
                            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));

                            mp_buffer_info_t buf;
                            memset(&buf, 0, sizeof(mp_buffer_info_t));

                            if(MP_OBJ_IS_BT_DATATYPE(value)) {
                                mp_get_buffer(value, &buf, MP_BUFFER_READ);
                            } 

                            size_t len = MIN(ESP_GATT_MAX_ATTR_LEN, buf.len);

                            rsp.attr_value.handle = cbdata.read.handle;
                            rsp.attr_value.len = len;
                            memcpy(rsp.attr_value.value, buf.buf, len);
                            esp_ble_gatts_send_response(bluetooth->gatts_interface, cbdata.read.conn_id, cbdata.read.trans_id, ESP_GATT_OK, &rsp);
                        }
                        if (cbdata.event == NETWORK_BLUETOOTH_WRITE) {
                            FREE(cbdata.write.value);
                        }
                    }
                }
                break;

            case NETWORK_BLUETOOTH_CONNECT:
            case NETWORK_BLUETOOTH_DISCONNECT:
                if (bluetooth->callback != mp_const_none) {
                    // items[0] is event
                    // items[1] is remote address
                    mp_obj_t args[] = {bluetooth, MP_OBJ_NEW_SMALL_INT(cbdata.event), mp_obj_new_bytearray(ESP_BD_ADDR_LEN, cbdata.connect.bda), bluetooth->callback_userdata };
                    mp_call_function_n_kw(bluetooth->callback, MP_ARRAY_SIZE(args), 0, args); 
                }
                break;
            case NETWORK_BLUETOOTH_SCAN_CMPL:
            case NETWORK_BLUETOOTH_SCAN_DATA:
                if (bluetooth->callback != mp_const_none) {
                    mp_obj_t data = mp_const_none;
                    if (cbdata.event == NETWORK_BLUETOOTH_SCAN_DATA) {
                        mp_obj_t scan_data_args[] = {mp_obj_new_bytearray(ESP_BD_ADDR_LEN, cbdata.scan_data.bda), mp_obj_new_bytearray(cbdata.scan_data.adv_data_len, cbdata.scan_data.adv_data) } ;
                        data = mp_obj_new_tuple(2, scan_data_args);
                        FREE(cbdata.scan_data.adv_data);
                    } 

                    mp_obj_t args[] = {bluetooth, MP_OBJ_NEW_SMALL_INT(cbdata.event), data, bluetooth->callback_userdata };
                    mp_call_function_n_kw(bluetooth->callback, MP_ARRAY_SIZE(args), 0, args); 
                }
                break;

            default:
                // do nothing, intentionally
                break;
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_callback_queue_handler_obj, network_bluetooth_callback_queue_handler);

STATIC void network_bluetooth_gattc_event_handler(
        esp_gattc_cb_event_t event, 
        esp_gatt_if_t gattc_if, 
        esp_ble_gattc_cb_param_t *param) {

    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        return;
    }
#ifdef EVENT_DEBUG_GATTC
    gattc_event_dump(event, gattc_if, param);
#endif
    switch(event) {
        case ESP_GATTC_REG_EVT:
            bluetooth->gattc_interface = gattc_if;
            break;

        case ESP_GATTC_OPEN_EVT:
            {
            }
            break;

        case ESP_GATTC_CLOSE_EVT:
            {
            }
            break;
        default:
            // do nothing, intentionally
            break;
    }
}

STATIC void network_bluetooth_gatts_event_handler(
        esp_gatts_cb_event_t event, 
        esp_gatt_if_t gatts_if, 
        esp_ble_gatts_cb_param_t *param) {

    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        return;
    }

#ifdef EVENT_DEBUG_GATTS
    gatts_event_dump(event, gatts_if, param);
#endif

    switch (event) {
        case ESP_GATTS_REG_EVT:
            bluetooth->gatts_interface = gatts_if;
            break;

        case ESP_GATTS_CREATE_EVT:
            {
                network_bluetooth_service_obj_t* service = network_bluetooth_item_op(bluetooth->services, &param->create.service_id.id.uuid, 0, NETWORK_BLUETOOTH_FIND_SERVICE);

                if (service != MP_OBJ_NULL) {
                    service->handle = param->create.service_handle;
                    service->service_id = param->create.service_id;
                    esp_ble_gatts_start_service(service->handle);
                }
            }
            break;

        case ESP_GATTS_START_EVT:
        case ESP_GATTS_STOP_EVT:
            {
                network_bluetooth_service_obj_t* service = network_bluetooth_item_op(bluetooth->services, NULL, param->start.service_handle, NETWORK_BLUETOOTH_FIND_SERVICE);
                if (service == MP_OBJ_NULL) {
                    break;
                }

                switch(event) {
                    case ESP_GATTS_START_EVT:
                        service->started = true;

                        ITEM_BEGIN();
                        size_t len;
                        mp_obj_t *items;
                        mp_obj_get_array(service->chars, &len, &items);

                        for (int i = 0; i < len; i++) {
                            network_bluetooth_char_obj_t* chr = (network_bluetooth_char_obj_t*) items[i];
                            esp_ble_gatts_add_char(service->handle, &chr->uuid, chr->perm, chr->prop, NULL, NULL);
                        }
                        ITEM_END();
                        break;
                    case ESP_GATTS_STOP_EVT:
                        service->started = false;
                        esp_ble_gatts_delete_service(service->handle);
                        break;
                    default:
                        // Nothing, intentionally
                        break;
                }

            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            {
                network_bluetooth_service_obj_t* service = network_bluetooth_item_op(bluetooth->services, NULL, param->add_char.service_handle, NETWORK_BLUETOOTH_FIND_SERVICE);
                if (service != MP_OBJ_NULL) {
                    network_bluetooth_char_obj_t* chr = network_bluetooth_item_op(service->chars, &param->add_char.char_uuid, 0, NETWORK_BLUETOOTH_FIND_CHAR);
                    if (chr != MP_OBJ_NULL) {
                        chr->handle = param->add_char.attr_handle;
                    }
                } else { 
                    NETWORK_BLUETOOTH_DEBUG_PRINTF("Service handle %d NOT FOUND\n", param->add_char.service_handle);
                }
            }

            break;

        case ESP_GATTS_READ_EVT:
        case ESP_GATTS_WRITE_EVT:
        case ESP_GATTS_CONNECT_EVT:
        case ESP_GATTS_DISCONNECT_EVT:
            {
                callback_data_t cbdata;

                if (event == ESP_GATTS_READ_EVT || event == ESP_GATTS_WRITE_EVT) {

                    cbdata.event = event == ESP_GATTS_READ_EVT ? NETWORK_BLUETOOTH_READ : NETWORK_BLUETOOTH_WRITE;

                    cbdata.read.handle      = param->read.handle;
                    cbdata.read.conn_id     = param->read.conn_id;
                    cbdata.read.trans_id    = param->read.trans_id;
                    cbdata.read.need_rsp    = event == ESP_GATTS_READ_EVT || (event == ESP_GATTS_WRITE_EVT && param->write.need_rsp);
                    if (event == ESP_GATTS_WRITE_EVT) {
                        cbdata.write.value = MALLOC(param->write.len); // Returns NULL when len == 0

                        if (cbdata.write.value != NULL) {
                            cbdata.write.value_len  = param->write.len;  
                            memcpy(cbdata.write.value, param->write.value, param->write.len);
                        } else {
                            cbdata.write.value_len  = 0;  
                        }
                    }
                } else if (event == ESP_GATTS_CONNECT_EVT || event == ESP_GATTS_DISCONNECT_EVT) {
                    cbdata.event = event == ESP_GATTS_CONNECT_EVT ? NETWORK_BLUETOOTH_CONNECT : NETWORK_BLUETOOTH_DISCONNECT;
                    cbdata.connect.conn_id = param->connect.conn_id;
                    memcpy(cbdata.connect.bda, param->connect.remote_bda, ESP_BD_ADDR_LEN);
                    bluetooth->conn_id = param->connect.conn_id;
                    bluetooth->gatts_connected = event == ESP_GATTS_CONNECT_EVT;
                }

                cbq_push(&cbdata, true);
                mp_sched_schedule(MP_OBJ_FROM_PTR(&network_bluetooth_callback_queue_handler_obj), MP_OBJ_NULL);
            }
            break;


        default:
            // Nothing, intentionally
            break;

    }

}



STATIC void network_bluetooth_gap_event_handler(
        esp_gap_ble_cb_event_t event, 
        esp_ble_gap_cb_param_t *param) {

#ifdef EVENT_DEBUG_GAP
    gap_event_dump(event, param);
#endif

    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        return;
    }

    switch (event) {
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                bluetooth->scanning = true;
            }
            break;

        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            // FIXME -- send event?
            if (param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                bluetooth->scanning = false;
            }
            break;

        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            {

                callback_data_t cbdata;
                bool nq = false;
                bool block = false;
                switch(param->scan_rst.search_evt) {
                    case ESP_GAP_SEARCH_INQ_RES_EVT:
                        {
                            uint8_t* adv_name;
                            uint8_t  adv_name_len;

                            cbdata.event = NETWORK_BLUETOOTH_SCAN_DATA;
                            adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

                            memcpy(cbdata.scan_data.bda, param->scan_rst.bda, ESP_BD_ADDR_LEN);
                            cbdata.scan_data.adv_data = MALLOC(adv_name_len);

                            if (cbdata.scan_data.adv_data != NULL) {
                                cbdata.scan_data.adv_data_len = adv_name_len;
                                memcpy(cbdata.scan_data.adv_data, adv_name, adv_name_len);
                            } else {
                                cbdata.scan_data.adv_data_len = 0;
                            }

                            nq = true;
                            break;
                        }

                    case ESP_GAP_SEARCH_INQ_CMPL_EVT: // scanning done
                        {
                            bluetooth->scanning = false;
                            cbdata.event = NETWORK_BLUETOOTH_SCAN_CMPL;
                            nq = true;
                            block = true;
                        }
                        break;

                    default:
                        // Do nothing, intentionally
                        break;
                }

                if (nq) {
                    cbq_push(&cbdata, block);
                    mp_sched_schedule(MP_OBJ_FROM_PTR(&network_bluetooth_callback_queue_handler_obj), MP_OBJ_NULL);
                }
            }
            break;

        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            if (bluetooth->advertising) {
                esp_ble_gap_start_advertising(&bluetooth->adv_params);
            }
            break;
        default: break; 
    }
}


STATIC void network_bluetooth_adv_updated() {
    network_bluetooth_obj_t* self = network_bluetooth_get_singleton();
    esp_ble_gap_stop_advertising();
    esp_ble_gap_config_adv_data(&self->adv_data);

    // Restart will be handled in network_bluetooth_gap_event_handler

}

/******************************************************************************/
// MicroPython bindings for network_bluetooth
//


STATIC void network_bluetooth_characteristic_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_bluetooth_char_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "BTChar(uuid = ");
    network_bluetooth_uuid_print(print, &self->uuid);
    mp_printf(print, ", perm = %02X, prop = %02X, handle = %04X, value = ", self->perm, self->prop, self->handle);
    mp_obj_print_helper(print, self->value, PRINT_REPR);
    mp_printf(print, ")");
}

STATIC void network_bluetooth_print_bda(const mp_print_t *print, esp_bd_addr_t bda) {
    mp_printf(print, "%02X:%02X:%02X:%02X:%02X:%02X",
            bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

STATIC void network_bluetooth_connection_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {

    network_bluetooth_connection_obj_t *connection = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "Connection(bda = ");
    network_bluetooth_print_bda(print, connection->bda);
    mp_printf(print, ", connected = %s", connection->connected ? "True" : "False");
    mp_printf(print, ")");
}


STATIC void network_bluetooth_service_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_bluetooth_service_obj_t *self = MP_OBJ_TO_PTR(self_in);
    // FIXME
    mp_printf(print, "Service(uuid = ");
    network_bluetooth_uuid_print(print, &self->service_id.id.uuid);
    mp_printf(print, ", handle = %04X)", self->handle);
}


STATIC void network_bluetooth_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_bluetooth_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "Bluetooth(params=())", self);
#define NETWORK_BLUETOOTH_LF "\n"
    NETWORK_BLUETOOTH_DEBUG_PRINTF(
            "Bluetooth(conn_id = %04X" NETWORK_BLUETOOTH_LF 
            ", gatts_connected = %s" NETWORK_BLUETOOTH_LF
            ", gatts_if = %u" NETWORK_BLUETOOTH_LF
            ", gattc_if = %u" NETWORK_BLUETOOTH_LF
            ", adv_params = ("
            "adv_int_min = %u, " NETWORK_BLUETOOTH_LF 
            "adv_int_max = %u, " NETWORK_BLUETOOTH_LF
            "adv_type = %u, " NETWORK_BLUETOOTH_LF
            "own_addr_type = %u, " NETWORK_BLUETOOTH_LF
            "peer_addr = %02X:%02X:%02X:%02X:%02X:%02X, " NETWORK_BLUETOOTH_LF
            "peer_addr_type = %u, " NETWORK_BLUETOOTH_LF
            "channel_map = %u, " NETWORK_BLUETOOTH_LF
            "adv_filter_policy = %u" NETWORK_BLUETOOTH_LF
            ")"
            ,
            self->conn_id,
            self->gatts_connected ? "True" : "False",
            self->gatts_interface,
            self->gattc_interface,
            (unsigned int)(self->adv_params.adv_int_min / 1.6),
            (unsigned int)(self->adv_params.adv_int_max / 1.6),
            self->adv_params.adv_type,
            self->adv_params.own_addr_type,
            self->adv_params.peer_addr[0],
            self->adv_params.peer_addr[1],
            self->adv_params.peer_addr[2],
            self->adv_params.peer_addr[3],
            self->adv_params.peer_addr[4],
            self->adv_params.peer_addr[5],
            self->adv_params.peer_addr_type,
            self->adv_params.channel_map,
            self->adv_params.adv_filter_policy
                );
            NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    ", data = ("
                    "set_scan_rsp = %s, " NETWORK_BLUETOOTH_LF
                    "include_name = %s, " NETWORK_BLUETOOTH_LF
                    "include_txpower = %s, " NETWORK_BLUETOOTH_LF
                    "min_interval = %d, " NETWORK_BLUETOOTH_LF
                    "max_interval = %d, " NETWORK_BLUETOOTH_LF
                    "appearance = %d, " NETWORK_BLUETOOTH_LF
                    "manufacturer_len = %d, " NETWORK_BLUETOOTH_LF
                    "p_manufacturer_data = %s, " NETWORK_BLUETOOTH_LF
                    "service_data_len = %d, " NETWORK_BLUETOOTH_LF
                    "p_service_data = ",
                    self->adv_data.set_scan_rsp ? "true" : "false",
                    self->adv_data.include_name ? "true" : "false",
                    self->adv_data.include_txpower ? "true" : "false",
                    self->adv_data.min_interval,
                    self->adv_data.max_interval,
                    self->adv_data.appearance,
                    self->adv_data.manufacturer_len,
                    self->adv_data.p_manufacturer_data ? (const char *)self->adv_data.p_manufacturer_data : "nil",
                    self->adv_data.service_data_len);
            if (self->adv_data.p_service_data != NULL) {
                dumpBuf(self->adv_data.p_service_data, self->adv_data.service_data_len);
            } else {
                NETWORK_BLUETOOTH_DEBUG_PRINTF("nil");
            }

            NETWORK_BLUETOOTH_DEBUG_PRINTF(", " NETWORK_BLUETOOTH_LF "flag = %d" NETWORK_BLUETOOTH_LF , self->adv_data.flag);


            NETWORK_BLUETOOTH_DEBUG_PRINTF(")\n");

}


STATIC mp_obj_t network_bluetooth_init(mp_obj_t self_in) {
    network_bluetooth_obj_t * self = (network_bluetooth_obj_t*)self_in;

    if (item_mut == NULL) {
        item_mut = xSemaphoreCreateMutex();
        xSemaphoreGive(item_mut);
    }

    if (callback_q == NULL) {
        callback_q = xQueueCreate(CALLBACK_QUEUE_SIZE, sizeof(callback_data_t));
        if (callback_q == NULL) {
            mp_raise_msg(&mp_type_OSError, "unable to create callback queue");
        } 
    } else {
        xQueueReset(callback_q);
    }
        
    if (self->state == NETWORK_BLUETOOTH_STATE_DEINIT) {
        NETWORK_BLUETOOTH_DEBUG_PRINTF("BT is deinit, initializing\n");

        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        esp_bt_controller_init(&bt_cfg); 

        // FIXME
        //
        // Looks like this needs to be globally idempotent
        // because:
        // 1) You can't disable BT once enabled
        // 2) You can't call this twice for enabling
        //
        // https://github.com/espressif/esp-idf/issues/405#issuecomment-299045124
        
        if (!network_bluetooth_bt_controller_enabled) {
            network_bluetooth_bt_controller_enabled = true;
            switch(esp_bt_controller_enable(ESP_BT_MODE_BTDM)) {
                case ESP_OK:
                    break;
                default:
                    mp_raise_msg(&mp_type_OSError, "esp_bt_controller_enable() failed");
                    break;
            }
        }

        switch(esp_bluedroid_init()) {
            case ESP_OK:
                break;
            default:
                mp_raise_msg(&mp_type_OSError, "esp_bluedroid_init() failed");
                break;

        }
        switch(esp_bluedroid_enable()) {
            case ESP_OK:
                break;
            default:
                mp_raise_msg(&mp_type_OSError, "esp_bluedroid_enable() failed");
                break;
        }

        esp_ble_gatts_register_callback(network_bluetooth_gatts_event_handler);
        esp_ble_gattc_register_callback(network_bluetooth_gattc_event_handler);
        esp_ble_gap_register_callback(network_bluetooth_gap_event_handler);

        // FIXME, this is hardcoded
        switch(esp_ble_gatts_app_register(0)) {
            case ESP_OK:
                break;

            default:
                mp_raise_msg(&mp_type_OSError, "esp_ble_gatts_app_register() failed");
                break;
        }
        // FIXME, this is hardcoded
        switch(esp_ble_gattc_app_register(1)) {
            case ESP_OK:
                break;

            default:
                mp_raise_msg(&mp_type_OSError, "esp_ble_gattc_app_register() failed");
                break;
        }
        self->state = NETWORK_BLUETOOTH_STATE_INIT;

    } else {
        NETWORK_BLUETOOTH_DEBUG_PRINTF("BT already initialized\n");
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_init_obj, network_bluetooth_init);

STATIC mp_obj_t network_bluetooth_ble_settings(size_t n_args, const mp_obj_t *pos_args, mp_map_t * kw_args) {
    network_bluetooth_obj_t *bluetooth = network_bluetooth_get_singleton();
    NETWORK_BLUETOOTH_DEBUG_PRINTF("Entering network_bluetooth_ble_settings(self = %p) n_args = %d\n", bluetooth, n_args);

    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        mp_raise_msg(&mp_type_OSError, "bluetooth is deinit");
    }


    bool changed = false;
    bool unset_adv_man_name = false;
    bool unset_adv_dev_name = false;
    bool unset_adv_uuid     = false;

    enum { 
        // params
        ARG_int_min,
        ARG_int_max,
        ARG_type,
        ARG_own_addr_type,
        ARG_peer_addr,
        ARG_peer_addr_type,
        ARG_channel_map,
        ARG_filter_policy,

        // data
        ARG_adv_is_scan_rsp,
        ARG_adv_dev_name,
        ARG_adv_man_name,
        ARG_adv_inc_txpower,
        ARG_adv_int_min,
        ARG_adv_int_max,
        ARG_adv_appearance,
        ARG_adv_uuid,
        ARG_adv_flags
    };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_int_min,              MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_int_max,              MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_type,                 MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1 }},
        { MP_QSTR_own_addr_type,        MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1 }},
        { MP_QSTR_peer_addr,            MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = NULL }},
        { MP_QSTR_peer_addr_type,       MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1 }},
        { MP_QSTR_channel_map,          MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1 }},
        { MP_QSTR_filter_policy,        MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1 }},

        { MP_QSTR_adv_is_scan_rsp,      MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_int = NULL}},
        { MP_QSTR_adv_dev_name,         MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = NULL }},
        { MP_QSTR_adv_man_name,         MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = NULL }},
        { MP_QSTR_adv_inc_tx_power,     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_int = NULL }},
        { MP_QSTR_adv_int_min,          MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1 }},
        { MP_QSTR_adv_int_max,          MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1 }},
        { MP_QSTR_adv_appearance,       MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1 }},
        { MP_QSTR_adv_uuid,             MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = NULL }},
        { MP_QSTR_adv_flags,            MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = NULL }},
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(0, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_buffer_info_t peer_addr_buf = { 0 };
    mp_buffer_info_t adv_man_name_buf = { 0 };
    mp_buffer_info_t adv_dev_name_buf = { 0 };
    mp_buffer_info_t adv_uuid_buf = { 0 };



    // pre-check complex types


    // peer_addr
    if (args[ARG_peer_addr].u_obj != NULL) {
        if(!MP_OBJ_IS_TYPE(args[ARG_peer_addr].u_obj, &mp_type_bytearray)) {
            goto NETWORK_BLUETOOTH_BAD_PEER;
        }

        mp_get_buffer(args[ARG_peer_addr].u_obj, &peer_addr_buf, MP_BUFFER_READ);
        if (peer_addr_buf.len != ESP_BD_ADDR_LEN) {
            goto NETWORK_BLUETOOTH_BAD_PEER;
        }
    }

    // adv_man_name
    if (args[ARG_adv_man_name].u_obj != NULL) {
        if (mp_obj_get_type(args[ARG_adv_man_name].u_obj) == mp_const_none) {
            unset_adv_man_name = true;
        } else if (!MP_OBJ_IS_STR_OR_BYTES(args[ARG_adv_man_name].u_obj)) {
            mp_raise_ValueError("adv_man_name must be type str or bytes");
        } 
        mp_obj_str_get_buffer(args[ARG_adv_man_name].u_obj, &adv_man_name_buf, MP_BUFFER_READ);
    }

    // adv_dev_name
    if (args[ARG_adv_dev_name].u_obj != NULL) {
        if (mp_obj_get_type(args[ARG_adv_dev_name].u_obj) == mp_const_none) {
            unset_adv_dev_name = true;
        } else if (!MP_OBJ_IS_STR_OR_BYTES(args[ARG_adv_dev_name].u_obj)) {
            mp_raise_ValueError("adv_dev_name must be type str or bytes");
        }
        mp_obj_str_get_buffer(args[ARG_adv_dev_name].u_obj, &adv_dev_name_buf, MP_BUFFER_READ);
    }

    // adv_uuid
    if (args[ARG_adv_uuid].u_obj != NULL) {
        if (mp_obj_get_type(args[ARG_adv_uuid].u_obj) == mp_const_none) {
            unset_adv_uuid = true;
        } else if (!MP_OBJ_IS_TYPE(mp_obj_get_type(args[ARG_adv_uuid].u_obj), &mp_type_bytearray)) {
            goto NETWORK_BLUETOOTH_BAD_UUID;
        }

        mp_get_buffer(args[ARG_adv_uuid].u_obj, &adv_uuid_buf, MP_BUFFER_READ);
        if (adv_uuid_buf.len != UUID_LEN) {
            // FIXME: Is length really a fixed amount?
            goto NETWORK_BLUETOOTH_BAD_UUID;
        }
    }


    // update esp_ble_adv_params_t 

    if (args[ARG_int_min].u_int != -1) {
        bluetooth->adv_params.adv_int_min = args[ARG_int_min].u_int * 1.6; // 0.625 msec per count
        changed = true;
    }

    if (args[ARG_int_max].u_int != -1) {
        bluetooth->adv_params.adv_int_max = args[ARG_int_max].u_int * 1.6;
        changed = true;
    }

    if (args[ARG_type].u_int != -1) {
        bluetooth->adv_params.adv_type = args[ARG_type].u_int;
        changed = true;
    }

    if (args[ARG_own_addr_type].u_int != -1) {
        bluetooth->adv_params.own_addr_type = args[ARG_own_addr_type].u_int;
        changed = true;
    }

    if (peer_addr_buf.buf != NULL) {
        memcpy(bluetooth->adv_params.peer_addr, peer_addr_buf.buf, ESP_BD_ADDR_LEN);
        changed = true;
    }

    if (args[ARG_peer_addr_type].u_int != -1) {
        bluetooth->adv_params.peer_addr_type = args[ARG_peer_addr_type].u_int;
        changed = true;
    }

    if (args[ARG_channel_map].u_int != -1) {
        bluetooth->adv_params.channel_map = args[ARG_channel_map].u_int;
        changed = true;
    }

    if (args[ARG_filter_policy].u_int != -1) {
        bluetooth->adv_params.adv_filter_policy = args[ARG_filter_policy].u_int;
        changed = true;
    }

    // update esp_ble_adv_data_t 
    //

    if (args[ARG_adv_is_scan_rsp].u_obj != NULL) {
        bluetooth->adv_data.set_scan_rsp = mp_obj_is_true(args[ARG_adv_is_scan_rsp].u_obj);
        changed = true;
    }

    if (unset_adv_dev_name) {
        esp_ble_gap_set_device_name("");
        bluetooth->adv_data.include_name = false;
    } else if (adv_dev_name_buf.buf != NULL) {
        esp_ble_gap_set_device_name(adv_dev_name_buf.buf);
        bluetooth->adv_data.include_name = adv_dev_name_buf.len > 0;
        changed = true;
    }

    if (unset_adv_man_name || adv_man_name_buf.buf != NULL) {
        if (bluetooth->adv_data.p_manufacturer_data != NULL) {
            NETWORK_BLUETOOTH_DEBUG_PRINTF("About to free p_manufacturer_data (2)\n");
            FREE(bluetooth->adv_data.p_manufacturer_data);
            bluetooth->adv_data.p_manufacturer_data = NULL;
        }

        bluetooth->adv_data.manufacturer_len = adv_man_name_buf.len;
        if (adv_man_name_buf.len > 0) {
            NETWORK_BLUETOOTH_DEBUG_PRINTF("About to call malloc for p_manufacturer_data\n");
            bluetooth->adv_data.p_manufacturer_data = MALLOC(adv_man_name_buf.len);
            memcpy(bluetooth->adv_data.p_manufacturer_data, adv_man_name_buf.buf, adv_man_name_buf.len);
        }
        changed = true;
    }

    if (args[ARG_adv_inc_txpower].u_obj != NULL) {
        bluetooth->adv_data.include_txpower = mp_obj_is_true(args[ARG_adv_inc_txpower].u_obj);
        changed = true;
    }

    if (args[ARG_adv_int_min].u_int != -1) {
        bluetooth->adv_data.min_interval = args[ARG_adv_int_min].u_int;
        changed = true;
    }

    if (args[ARG_adv_int_max].u_int != -1) {
        bluetooth->adv_data.max_interval = args[ARG_adv_int_max].u_int;
        changed = true;
    }

    if (args[ARG_adv_appearance].u_int != -1) {
        bluetooth->adv_data.appearance = args[ARG_adv_appearance].u_int;
        changed = true;
    }

    if (unset_adv_uuid || adv_uuid_buf.buf != NULL) {

        if (bluetooth->adv_data.p_service_uuid != NULL) {
            FREE(bluetooth->adv_data.p_service_uuid );
            bluetooth->adv_data.p_service_uuid = NULL;
        }

        bluetooth->adv_data.service_uuid_len = adv_uuid_buf.len;
        if (adv_uuid_buf.len > 0) {
            bluetooth->adv_data.p_service_uuid = MALLOC(adv_uuid_buf.len);
            memcpy(bluetooth->adv_data.p_service_uuid, adv_uuid_buf.buf, adv_uuid_buf.len);
        }
        changed = true;
    }

    if (args[ARG_adv_flags].u_int != -1) {
        bluetooth->adv_data.flag = args[ARG_adv_flags].u_int;
        changed = true;
    }

    if (changed) {
        network_bluetooth_adv_updated();
    }

    return mp_const_none;

NETWORK_BLUETOOTH_BAD_PEER:
    mp_raise_ValueError("peer_addr must be bytearray(" TOSTRING(ESP_BD_ADDR_LEN) ")");
NETWORK_BLUETOOTH_BAD_UUID:
    mp_raise_ValueError("adv_uuid must be bytearray(" TOSTRING(UUID_LEN ) ")");
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(network_bluetooth_ble_settings_obj, 1, network_bluetooth_ble_settings);

STATIC mp_obj_t network_bluetooth_ble_adv_enable(mp_obj_t self_in, mp_obj_t enable) {
    network_bluetooth_obj_t * self = MP_OBJ_TO_PTR(self_in);
    self->advertising = mp_obj_is_true(enable);
    network_bluetooth_adv_updated();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(network_bluetooth_ble_adv_enable_obj, network_bluetooth_ble_adv_enable);

STATIC mp_obj_t network_bluetooth_callback_helper(mp_obj_t* callback, mp_obj_t* callback_userdata, size_t n_args, const mp_obj_t *args) {
    if (n_args > 1) {
        if (mp_obj_is_callable(args[1]) || args[1] == mp_const_none) {
            *callback = args[1];
        } else {
            mp_raise_ValueError("parameter must be callable, or None");
        }

        *callback_userdata = n_args > 2 ? args[2] : mp_const_none;
    }

    mp_obj_t items[2] = {*callback, *callback_userdata};
    mp_obj_t tuple = mp_obj_new_tuple(2, items);

    return tuple;
}


STATIC mp_obj_t network_bluetooth_scan_start(mp_obj_t self_in, mp_obj_t timeout_arg) {
    network_bluetooth_obj_t *bluetooth = MP_OBJ_TO_PTR(self_in);
    
    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        mp_raise_msg(&mp_type_OSError, "bluetooth is deinit");
    }

    if (bluetooth->scanning) {
        mp_raise_msg(&mp_type_OSError, "already scanning");
    }
    mp_int_t timeout = mp_obj_get_int(timeout_arg);

    static esp_ble_scan_params_t params = {
        .scan_type              = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type          = ESP_PUBLIC_ADDR,
        .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval          = 0x50,
        .scan_window            = 0x30
    };

    assert(esp_ble_gap_set_scan_params(&params) == ESP_OK);
    assert(esp_ble_gap_start_scanning(timeout) == ESP_OK);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(network_bluetooth_scan_start_obj, network_bluetooth_scan_start);


STATIC mp_obj_t network_bluetooth_scan_stop(mp_obj_t self_in) {
    (void)self_in;
    assert(esp_ble_gap_stop_scanning() == ESP_OK);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_scan_stop_obj, network_bluetooth_scan_stop);

STATIC mp_obj_t network_bluetooth_connect(mp_obj_t self_in, mp_obj_t bda) {
    network_bluetooth_obj_t * bluetooth = (network_bluetooth_obj_t *)self_in;
    mp_buffer_info_t bda_buf = { 0 };

    if (!MP_OBJ_IS_TYPE(bda, &mp_type_bytearray)) {
        goto NETWORK_BLUETOOTH_connect_BAD_ADX;
    }
    mp_get_buffer(bda, &bda_buf, MP_BUFFER_READ);
    if (bda_buf.len != ESP_BD_ADDR_LEN) {
        goto NETWORK_BLUETOOTH_connect_BAD_ADX;
    }

    network_bluetooth_connection_obj_t* connection = m_new_obj(network_bluetooth_connection_obj_t);
    memset(connection, 0, sizeof(network_bluetooth_connection_obj_t));
    connection->base.type = &network_bluetooth_connection_type;
    memcpy(connection->bda, bda_buf.buf, ESP_BD_ADDR_LEN);


    NETWORK_BLUETOOTH_DEBUG_PRINTF("Calling open with IF=%u\n", bluetooth->gattc_interface);
    esp_ble_gattc_open(bluetooth->gattc_interface, connection->bda, true);
    
    return connection;

NETWORK_BLUETOOTH_connect_BAD_ADX:
    mp_raise_ValueError("bda must be bytearray(" TOSTRING(ESP_BD_ADDR_LEN) ")");
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(network_bluetooth_connect_obj, network_bluetooth_connect);


STATIC void network_bluetooth_connection_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    network_bluetooth_connection_obj_t* connection = (network_bluetooth_connection_obj_t*) self_in;
    if (!check_locals_dict(connection, attr, dest)) {
        switch(attr) {
            case MP_QSTR_is_connected:
                if (dest[0] == MP_OBJ_NULL) {  // load
                    dest[0] = connection->connected ? mp_const_true : mp_const_false;
                }  else if (dest[1] != MP_OBJ_NULL) { // store
                    dest[0] = MP_OBJ_NULL;
                }
        }
    }
}


STATIC mp_obj_t network_bluetooth_callback(size_t n_args, const mp_obj_t *args) {
    network_bluetooth_obj_t * self = network_bluetooth_get_singleton();
    return network_bluetooth_callback_helper(&self->callback, &self->callback_userdata, n_args, args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(network_bluetooth_callback_obj, 1, 3, network_bluetooth_callback);

STATIC mp_obj_t network_bluetooth_char_callback(size_t n_args, const mp_obj_t *args) {
    network_bluetooth_char_obj_t* self = (network_bluetooth_char_obj_t*) args[0];
    return network_bluetooth_callback_helper(&self->callback, &self->callback_userdata, n_args, args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(network_bluetooth_char_callback_obj, 1, 3, network_bluetooth_char_callback);

STATIC mp_obj_t network_bluetooth_char_indicate(size_t n_args, const mp_obj_t *pos_args, mp_map_t * kw_args) {
    NETWORK_BLUETOOTH_DEBUG_PRINTF("network_bluetooth_char_indicate()\n");

    network_bluetooth_obj_t *bluetooth = network_bluetooth_get_singleton();
    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        mp_raise_msg(&mp_type_OSError, "bluetooth is deinit");
    }
    network_bluetooth_char_obj_t* self = MP_OBJ_TO_PTR(pos_args[0]);

    enum {ARG_value, ARG_need_confirm};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_value,        MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL }},
        { MP_QSTR_need_confirm, MP_ARG_BOOL, { .u_bool = false } },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];

    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_value].u_obj != MP_OBJ_NULL) {
        if (!MP_OBJ_IS_BT_DATATYPE(args[ARG_value].u_obj)) {
            mp_raise_ValueError("value must be string, bytes, bytearray, or None");
        } 
    } else {
        args[ARG_value].u_obj = self->value;
    }

    NETWORK_BLUETOOTH_DEBUG_PRINTF("need confirmations: %s\n", args[ARG_need_confirm].u_bool ? "True" : "False");
    mp_buffer_info_t buf;
    mp_get_buffer(args[ARG_value].u_obj, &buf, MP_BUFFER_READ);
    esp_ble_gatts_send_indicate(bluetooth->gatts_interface, bluetooth->conn_id, self->handle, buf.len, buf.buf, args[ARG_need_confirm].u_bool);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(network_bluetooth_char_indicate_obj, 1, network_bluetooth_char_indicate);

STATIC mp_obj_t network_bluetooth_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *all_args) { 
    network_bluetooth_obj_t *self = network_bluetooth_get_singleton();
    if (n_args != 0 || n_kw != 0) {
        mp_raise_TypeError("Constructor takes no arguments");
    }

    network_bluetooth_init(self);
    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t network_bluetooth_service_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {

    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();

    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        mp_raise_msg(&mp_type_OSError, "bluetooth is deinit");
    }

    enum {ARG_uuid, ARG_primary};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uuid,         MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_primary,    MP_ARG_OBJ, {.u_obj = mp_const_true } },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    esp_bt_uuid_t uuid;
    parse_uuid(args[ARG_uuid].u_obj, &uuid);
    network_bluetooth_service_obj_t *self = network_bluetooth_item_op(bluetooth->services, &uuid, 0, NETWORK_BLUETOOTH_FIND_SERVICE);

    if (self != MP_OBJ_NULL) {
        return MP_OBJ_FROM_PTR(self);
    }
    self = m_new_obj(network_bluetooth_service_obj_t);
    self->base.type = type;
    self->service_id.id.uuid = uuid;
    self->valid = true;

    parse_uuid(args[ARG_uuid].u_obj, &self->service_id.id.uuid);
    self->service_id.is_primary = mp_obj_is_true(args[ARG_primary].u_obj);
    self->chars = mp_obj_new_list(0, NULL);

    ITEM_BEGIN();
    mp_obj_list_append(bluetooth->services, MP_OBJ_FROM_PTR(self));
    ITEM_END();

    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t network_bluetooth_characteristic_make_new(size_t n_args, const mp_obj_t *pos_args, mp_map_t * kw_args) {

    enum {ARG_uuid, ARG_value, ARG_perm, ARG_prop};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uuid,     MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_value,    MP_ARG_OBJ, {.u_obj = mp_const_none }},
        { MP_QSTR_perm,     MP_ARG_INT, {.u_int = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE } },
        { MP_QSTR_prop,     MP_ARG_INT, {.u_int = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    network_bluetooth_service_obj_t* service = MP_OBJ_TO_PTR(pos_args[0]);

    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    esp_bt_uuid_t uuid;
    parse_uuid(args[ARG_uuid].u_obj, &uuid);
    network_bluetooth_char_obj_t *self = network_bluetooth_item_op(service->chars, &uuid, 0, NETWORK_BLUETOOTH_FIND_CHAR);

    if (self != MP_OBJ_NULL) {
        return MP_OBJ_FROM_PTR(self);
    }

    self = m_new_obj(network_bluetooth_char_obj_t);
    self->base.type = &network_bluetooth_characteristic_type;

    if (!MP_OBJ_IS_BT_DATATYPE(args[ARG_value].u_obj)) {
        mp_raise_ValueError("value must be str, bytes, bytearray, or None");
    }

    self->callback = mp_const_none;

    self->uuid = uuid;
    self->value = args[ARG_value].u_obj;
    self->perm = args[ARG_perm].u_int;
    self->prop = args[ARG_prop].u_int;

    ITEM_BEGIN();
    mp_obj_list_append(service->chars, self);
    ITEM_END();

    return MP_OBJ_FROM_PTR(self);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(network_bluetooth_characteristic_make_new_obj, 1, network_bluetooth_characteristic_make_new);


STATIC mp_obj_t network_bluetooth_service_start(mp_obj_t self_in) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        mp_raise_msg(&mp_type_OSError, "bluetooth is deinit");
    }
    network_bluetooth_service_obj_t* self = (network_bluetooth_service_obj_t*) self_in;

    if (!self->valid) {
        mp_raise_msg(&mp_type_OSError, "service invalid");
    }

    if (self->started) {
        return mp_const_none;
    }

    mp_int_t numChars = mp_obj_get_int(mp_obj_len(self->chars));
    uint32_t then = mp_hal_ticks_ms();

    // Wait for registration
    while(bluetooth->gatts_interface == ESP_GATT_IF_NONE && mp_hal_ticks_ms() - then < 1000) {
        NETWORK_BLUETOOTH_DEBUG_PRINTF("Waiting for BT interface registration ...\n");
        mp_hal_delay_ms(100);
    }

    if (bluetooth->gatts_interface == ESP_GATT_IF_NONE) {
        mp_raise_msg(&mp_type_OSError, "esp_ble_gatts_app_register() has failed");
    }


    // Handle calculation:
    //
    // 1 for 2800/2801 Service declaration
    // 1x for each 2803 Char declaration 
    // 1x for each Char desc declaration --- not used 
    esp_ble_gatts_create_service(bluetooth->gatts_interface, &self->service_id, 1 + numChars * 2);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_service_start_obj, network_bluetooth_service_start);

STATIC mp_obj_t network_bluetooth_service_stop(mp_obj_t self_in) {
    network_bluetooth_service_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (!self->valid) {
        mp_raise_msg(&mp_type_OSError, "service invalid");
    }
    if (!self->started || self->handle == 0) {
        return mp_const_none;
    }
    esp_ble_gatts_stop_service(self->handle);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_service_stop_obj, network_bluetooth_service_stop);

STATIC mp_obj_t network_bluetooth_service_close(mp_obj_t self_in) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        mp_raise_msg(&mp_type_OSError, "bluetooth is deinit");
    }

    network_bluetooth_service_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->valid) {
        mp_raise_msg(&mp_type_OSError, "service invalid");
    }

    network_bluetooth_item_op(bluetooth->services, &self->service_id.id.uuid, 0, NETWORK_BLUETOOTH_DEL_SERVICE);
    self->valid = false;
    if (self->handle != 0) {
        esp_ble_gatts_delete_service(self->handle);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_service_close_obj, network_bluetooth_service_close);
// char attribute handler
STATIC void network_bluetooth_char_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    network_bluetooth_char_obj_t* self = (network_bluetooth_char_obj_t*) self_in;
    if (!check_locals_dict(self, attr, dest)) {

        switch(attr) {
            case MP_QSTR_value:
                if (dest[0] == MP_OBJ_NULL) {  // load
                    dest[0] = self->value;
                }  else if (dest[1] != MP_OBJ_NULL) { // store
                    if (!MP_OBJ_IS_BT_DATATYPE(dest[1])) {
                        mp_raise_ValueError("value must be string, bytes, bytearray, or None");
                    }
                    self->value = dest[1];
                    dest[0] = MP_OBJ_NULL;
                }
        }
    }
}

// service attribute handler
STATIC void network_bluetooth_service_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    network_bluetooth_service_obj_t* self = (network_bluetooth_service_obj_t*) self_in;
    if (!check_locals_dict(self, attr, dest)) {

        // ATTENTION: All the switches below are for read-only attribs!
        if (dest[0] == MP_OBJ_NULL) {
            switch(attr) {
                case MP_QSTR_Char:
                    dest[0] = MP_OBJ_FROM_PTR(&network_bluetooth_characteristic_make_new_obj);
                    dest[1] = self;
                    break;

                case MP_QSTR_chars:
                    dest[0] = self->chars;
                    break;
            }
        }
    }
}

// bluetooth attribute handler
STATIC void network_bluetooth_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    network_bluetooth_obj_t* self = (network_bluetooth_obj_t*) self_in;

    if (!check_locals_dict(self, attr, dest)) {
        switch(attr) {
            case MP_QSTR_is_scanning:
                if (dest[0] == MP_OBJ_NULL) {
                    dest[0] = self->scanning ? mp_const_true : mp_const_false;
                } else {
                    dest[0] = MP_OBJ_NULL;
                }
                break;
            case MP_QSTR_services:
                if (dest[0] == MP_OBJ_NULL) {  // load
                    dest[0] = self->services;
                } else {
                    dest[0] = MP_OBJ_NULL;
                }
                break;
        }
    }
}



STATIC mp_obj_t network_bluetooth_deinit(mp_obj_t self_in) {
    NETWORK_BLUETOOTH_DEBUG_PRINTF("Entering network_bluetooth_deinit\n");
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    
    if (bluetooth->state == NETWORK_BLUETOOTH_STATE_INIT) {

        bluetooth->state = NETWORK_BLUETOOTH_STATE_DEINIT;

        size_t len;
        mp_obj_t *items;
        mp_obj_get_array(bluetooth->services, &len, &items);
        for(int i = 0; i < len; i++) {
            network_bluetooth_service_obj_t* service = (network_bluetooth_service_obj_t*) items[i];
            if (service->handle != 0) {
                esp_ble_gatts_delete_service(service->handle);
            }
            service->valid = false;
        }
        bluetooth->services = mp_obj_new_list(0, NULL);

        if (bluetooth->gattc_interface != ESP_GATT_IF_NONE) {
            esp_ble_gattc_app_unregister(bluetooth->gattc_interface);
        }

        if (bluetooth->gatts_interface != ESP_GATT_IF_NONE) {
            esp_ble_gatts_app_unregister(bluetooth->gatts_interface);
        }

        switch(esp_bluedroid_disable()) {
            case ESP_OK:
                break;
            default:
                mp_raise_msg(&mp_type_OSError, "esp_bluedroid_disable() failed");
                break;
        }
        switch(esp_bluedroid_deinit()) {
            case ESP_OK:
                break;
            default:
                mp_raise_msg(&mp_type_OSError, "esp_bluedroid_deinit() failed");
                break;

        }
        // FIXME:  This fails, with the present IDF
        
#if 0
        mp_hal_delay_ms(300);
        switch(esp_bt_controller_enable(ESP_BT_MODE_IDLE)) {
            case ESP_OK:
                break;
            default:
                mp_raise_msg(&mp_type_OSError, "esp_bt_controller_enable(idle) failed");
                break;
        }
#endif
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_deinit_obj, network_bluetooth_deinit);

//
// BLUETOOTH OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR_ble_settings), MP_ROM_PTR(&network_bluetooth_ble_settings_obj) },
    { MP_ROM_QSTR(MP_QSTR_ble_adv_enable), MP_ROM_PTR(&network_bluetooth_ble_adv_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&network_bluetooth_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&network_bluetooth_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&network_bluetooth_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_Service), MP_ROM_PTR(&network_bluetooth_gatts_service_type) },
    { MP_ROM_QSTR(MP_QSTR_services), NULL }, // Dummy, handled by .attr 
    { MP_ROM_QSTR(MP_QSTR_callback), MP_ROM_PTR(&network_bluetooth_callback_obj) }, 
    { MP_ROM_QSTR(MP_QSTR_scan_start), MP_ROM_PTR(&network_bluetooth_scan_start_obj) }, 
    { MP_ROM_QSTR(MP_QSTR_scan_stop), MP_ROM_PTR(&network_bluetooth_scan_stop_obj) }, 
    { MP_ROM_QSTR(MP_QSTR_is_scanning), NULL },  // handle by attr

    // class constants

    // Callback types
    { MP_ROM_QSTR(MP_QSTR_CONNECT),                     MP_ROM_INT(NETWORK_BLUETOOTH_CONNECT) },
    { MP_ROM_QSTR(MP_QSTR_DISCONNECT),                  MP_ROM_INT(NETWORK_BLUETOOTH_DISCONNECT) },
    { MP_ROM_QSTR(MP_QSTR_READ),                        MP_ROM_INT(NETWORK_BLUETOOTH_READ) },
    { MP_ROM_QSTR(MP_QSTR_WRITE),                       MP_ROM_INT(NETWORK_BLUETOOTH_WRITE) },
    { MP_ROM_QSTR(MP_QSTR_SCAN_DATA),                   MP_ROM_INT(NETWORK_BLUETOOTH_SCAN_DATA) },
    { MP_ROM_QSTR(MP_QSTR_SCAN_CMPL),                   MP_ROM_INT(NETWORK_BLUETOOTH_SCAN_CMPL) },

    // esp_ble_adv_type_t
    { MP_ROM_QSTR(MP_QSTR_ADV_TYPE_IND),                MP_ROM_INT(ADV_TYPE_IND) },
    { MP_ROM_QSTR(MP_QSTR_ADV_TYPE_DIRECT_IND_HIGH),    MP_ROM_INT(ADV_TYPE_DIRECT_IND_HIGH) },
    { MP_ROM_QSTR(MP_QSTR_ADV_TYPE_SCAN_IND),           MP_ROM_INT(ADV_TYPE_SCAN_IND) },
    { MP_ROM_QSTR(MP_QSTR_ADV_TYPE_NONCONN_IND),        MP_ROM_INT(ADV_TYPE_NONCONN_IND) },
    { MP_ROM_QSTR(MP_QSTR_ADV_TYPE_DIRECT_IND_LOW),     MP_ROM_INT(ADV_TYPE_DIRECT_IND_LOW) },

    // esp_ble_addr_type_t
    { MP_ROM_QSTR(MP_QSTR_BLE_ADDR_TYPE_PUBLIC),        MP_ROM_INT(BLE_ADDR_TYPE_PUBLIC) },
    { MP_ROM_QSTR(MP_QSTR_BLE_ADDR_TYPE_RANDOM),        MP_ROM_INT(BLE_ADDR_TYPE_RANDOM) },
    { MP_ROM_QSTR(MP_QSTR_BLE_ADDR_TYPE_RPA_PUBLIC),    MP_ROM_INT(BLE_ADDR_TYPE_RPA_PUBLIC) }, { MP_ROM_QSTR(MP_QSTR_BLE_ADDR_TYPE_RPA_RANDOM),    MP_ROM_INT(BLE_ADDR_TYPE_RPA_RANDOM) },

    // esp_ble_adv_channel_t
    { MP_ROM_QSTR(MP_QSTR_ADV_CHNL_37),                 MP_ROM_INT(ADV_CHNL_37) },
    { MP_ROM_QSTR(MP_QSTR_ADV_CHNL_38),                 MP_ROM_INT(ADV_CHNL_38) },
    { MP_ROM_QSTR(MP_QSTR_ADV_CHNL_39),                 MP_ROM_INT(ADV_CHNL_39) },
    { MP_ROM_QSTR(MP_QSTR_ADV_CHNL_ALL),                MP_ROM_INT(ADV_CHNL_ALL) },

    // exp_gatt_perm_t
    { MP_ROM_QSTR(MP_QSTR_PERM_READ),                   MP_ROM_INT(ESP_GATT_PERM_READ) },
    { MP_ROM_QSTR(MP_QSTR_PERM_READ_ENCRYPTED),         MP_ROM_INT(ESP_GATT_PERM_READ_ENCRYPTED) },
    { MP_ROM_QSTR(MP_QSTR_PERM_READ_ENC_MITM),          MP_ROM_INT(ESP_GATT_PERM_READ_ENC_MITM) },
    { MP_ROM_QSTR(MP_QSTR_PERM_WRITE),                  MP_ROM_INT(ESP_GATT_PERM_WRITE) },
    { MP_ROM_QSTR(MP_QSTR_PERM_WRITE_ENCRYPTED),        MP_ROM_INT(ESP_GATT_PERM_WRITE_ENCRYPTED) },
    { MP_ROM_QSTR(MP_QSTR_PERM_WRITE_ENC_MITM),         MP_ROM_INT(ESP_GATT_PERM_WRITE_ENC_MITM) },
    { MP_ROM_QSTR(MP_QSTR_PERM_WRITE_SIGNED),           MP_ROM_INT(ESP_GATT_PERM_WRITE_SIGNED) },
    { MP_ROM_QSTR(MP_QSTR_PERM_WRITE_SIGNED_MITM),      MP_ROM_INT(ESP_GATT_PERM_WRITE_SIGNED_MITM) },

    // esp_gatt_char_prop_t

    { MP_ROM_QSTR(MP_QSTR_PROP_BROADCAST),              MP_ROM_INT(ESP_GATT_CHAR_PROP_BIT_BROADCAST) },
    { MP_ROM_QSTR(MP_QSTR_PROP_READ),                   MP_ROM_INT(ESP_GATT_CHAR_PROP_BIT_READ) },
    { MP_ROM_QSTR(MP_QSTR_PROP_WRITE_NR),               MP_ROM_INT(ESP_GATT_CHAR_PROP_BIT_WRITE_NR) },
    { MP_ROM_QSTR(MP_QSTR_PROP_WRITE),                  MP_ROM_INT(ESP_GATT_CHAR_PROP_BIT_WRITE) },
    { MP_ROM_QSTR(MP_QSTR_PROP_NOTIFY),                 MP_ROM_INT(ESP_GATT_CHAR_PROP_BIT_NOTIFY) },
    { MP_ROM_QSTR(MP_QSTR_PROP_INDICATE),               MP_ROM_INT(ESP_GATT_CHAR_PROP_BIT_INDICATE) },
    { MP_ROM_QSTR(MP_QSTR_PROP_AUTH),                   MP_ROM_INT(ESP_GATT_CHAR_PROP_BIT_AUTH) },
    { MP_ROM_QSTR(MP_QSTR_PROP_EXT_PROP),               MP_ROM_INT(ESP_GATT_CHAR_PROP_BIT_EXT_PROP) },

    // esp_ble_adv_filter_t
    { MP_ROM_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY),
        MP_ROM_INT(ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY) },
    { MP_ROM_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY),
        MP_ROM_INT(ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY) },
    { MP_ROM_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST),
        MP_ROM_INT(ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST) },
    { MP_ROM_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST),
        MP_ROM_INT(ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST) },
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_locals_dict, network_bluetooth_locals_dict_table);

const mp_obj_type_t network_bluetooth_type = {
    { &mp_type_type },
    .name = MP_QSTR_Bluetooth,
    .print = network_bluetooth_print,
    .make_new = network_bluetooth_make_new,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_locals_dict,
    .attr = network_bluetooth_attr,
};


// 
// CONNECTION OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_connection_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_chars), NULL },  // Handled by attr method
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_connection_locals_dict, network_bluetooth_connection_locals_dict_table);


const mp_obj_type_t network_bluetooth_connection_type = {
    { &mp_type_type },
    .name = MP_QSTR_Connection,
    .print = network_bluetooth_connection_print,
    //.make_new = network_bluetooth_service_make_new,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_connection_locals_dict,
    .attr = network_bluetooth_connection_attr,
};

// 
// GATTS SERVICE OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_gatts_service_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_Char), NULL },   // Handled by attr method
    { MP_ROM_QSTR(MP_QSTR_chars), NULL },  // Handled by attr method
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&network_bluetooth_service_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&network_bluetooth_service_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&network_bluetooth_service_close_obj) },
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_gatts_service_locals_dict, network_bluetooth_gatts_service_locals_dict_table);

const mp_obj_type_t network_bluetooth_gatts_service_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTSService,
    .print = network_bluetooth_service_print,
    .make_new = network_bluetooth_service_make_new,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_gatts_service_locals_dict,
    .attr = network_bluetooth_service_attr,
};


// 
// GATTC SERVICE OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_gattc_service_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_chars), NULL },  // Handled by attr method
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_gattc_service_locals_dict, network_bluetooth_gattc_service_locals_dict_table);


const mp_obj_type_t network_bluetooth_gattc_service_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTCService,
    .print = network_bluetooth_service_print,
    .make_new = network_bluetooth_service_make_new,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_gattc_service_locals_dict,
    .attr = network_bluetooth_service_attr,
};


// 
// CHARACTERISTIC OBJECTS
//


STATIC const mp_rom_map_elem_t network_bluetooth_characteristic_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_callback), MP_ROM_PTR(&network_bluetooth_char_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_indicate), MP_ROM_PTR(&network_bluetooth_char_indicate_obj) },
    { MP_ROM_QSTR(MP_QSTR_value), NULL }, // handled by attr handler
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_characteristic_locals_dict, network_bluetooth_characteristic_locals_dict_table);

const mp_obj_type_t network_bluetooth_characteristic_type = {
    { &mp_type_type },
    .name = MP_QSTR_Char,
    .print = network_bluetooth_characteristic_print,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_characteristic_locals_dict,
    .attr = network_bluetooth_char_attr,
};
