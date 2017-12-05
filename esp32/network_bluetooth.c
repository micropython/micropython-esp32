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

//#define EVENT_DEBUG

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

#define MP_OBJ_NEW_BYTES(len, data) mp_obj_new_str_of_type(&mp_type_bytes, data, len)

const mp_obj_type_t network_bluetooth_type;
const mp_obj_type_t network_bluetooth_connection_type;
const mp_obj_type_t network_bluetooth_gatts_service_type;
const mp_obj_type_t network_bluetooth_gattc_service_type;
const mp_obj_type_t network_bluetooth_gatts_char_type;
const mp_obj_type_t network_bluetooth_gattc_char_type;
const mp_obj_type_t network_bluetooth_gattc_descr_type;
const mp_obj_type_t network_bluetooth_gatts_descr_type;

STATIC SemaphoreHandle_t item_mut;

#define MP_OBJ_IS_BYTEARRAY_OR_BYTES(O) (MP_OBJ_IS_TYPE(O, &mp_type_bytes) || MP_OBJ_IS_TYPE(O, &mp_type_bytearray))

#define ITEM_BEGIN() assert(xSemaphoreTake(item_mut, portMAX_DELAY) == pdTRUE)
#define ITEM_END() xSemaphoreGive(item_mut)

// CONSTANTS

typedef enum {

    // GATTS
    NETWORK_BLUETOOTH_GATTS_CONNECT,
    NETWORK_BLUETOOTH_GATTS_DISCONNECT,

    NETWORK_BLUETOOTH_GATTS_CREATE,
    NETWORK_BLUETOOTH_GATTS_START,
    NETWORK_BLUETOOTH_GATTS_STOP,
    NETWORK_BLUETOOTH_GATTS_ADD_CHAR, // Add char
    NETWORK_BLUETOOTH_GATTS_ADD_CHAR_DESCR, // Add descriptor

    // GAP / GATTC events
    NETWORK_BLUETOOTH_GATTC_SCAN_RES,
    NETWORK_BLUETOOTH_GATTC_SCAN_CMPL, // Found GATT servers
    NETWORK_BLUETOOTH_GATTC_SEARCH_RES, // Found GATTS services

    NETWORK_BLUETOOTH_GATTC_GET_CHAR,
    NETWORK_BLUETOOTH_GATTC_GET_DESCR,

    NETWORK_BLUETOOTH_GATTC_OPEN,
    NETWORK_BLUETOOTH_GATTC_CLOSE,

    // characteristic events
    NETWORK_BLUETOOTH_READ,
    NETWORK_BLUETOOTH_WRITE,
    NETWORK_BLUETOOTH_NOTIFY,

} network_bluetooth_event_t;

typedef struct {
    network_bluetooth_event_t event;
    union {

        struct {
            uint16_t                handle;
            esp_gatt_srvc_id_t      service_id;
        } gatts_create;

        struct {
            uint16_t                service_handle;
        } gatts_start_stop;

        struct {
            uint16_t                service_handle;
            uint16_t                handle;
            esp_bt_uuid_t           uuid;
        } gatts_add_char_descr;

        struct {
            uint16_t                conn_id;
            esp_bd_addr_t           bda;
        } gatts_connect_disconnect;

        struct {
            esp_bd_addr_t           bda;
            uint8_t*                adv_data; // Need to free this!
            uint8_t                 adv_data_len;
            int                     rssi;
        } gattc_scan_res;

        struct {
            uint16_t                conn_id;
            esp_gatt_srvc_id_t      service_id;
        } gattc_search_res;

        struct {
            uint16_t                conn_id;
            esp_gatt_srvc_id_t      service_id;
            esp_gatt_id_t           char_id;
            esp_gatt_char_prop_t    props;
        } gattc_get_char;

        struct {
            uint16_t                conn_id;
            esp_gatt_srvc_id_t      service_id;
            esp_gatt_id_t           char_id;
            esp_gatt_id_t           descr_id;
        } gattc_get_descr;

        struct {
            uint16_t                conn_id;
            uint16_t                mtu;
            esp_bd_addr_t           bda;

        } gattc_open_close;

        struct {
            uint16_t                conn_id;
            uint16_t                handle;
            uint32_t                trans_id;
            bool                    need_rsp;
        } read;

        struct {
            uint16_t                conn_id;
            uint16_t                handle;
            uint32_t                trans_id;
            bool                    need_rsp;

            // Following fields _must_
            // come after the first four above,
            // which _must_ match the read struct

            uint8_t*                value; // Need to free this!
            size_t                  value_len;
        } write;

        struct {
            uint16_t                conn_id;

            esp_gatt_srvc_id_t      service_id;
            esp_gatt_id_t           char_id;
            esp_gatt_id_t           descr_id;
            bool                    need_rsp;

            uint8_t*                value; // Need to free this!
            size_t                  value_len;
        } notify;
    };

} callback_data_t;

typedef struct {
    uint8_t*                value; // Need to free this!
    size_t                  value_len;
} read_write_data_t;

// "Char" and "Descriptor" common structure

typedef struct {
    mp_obj_base_t           base;
    mp_obj_t                parent;

    esp_gatt_id_t           id; // common
    esp_gatt_char_prop_t    prop;    // common

    mp_obj_t                descrs; // GATTC

    esp_gatt_perm_t         perm;    // GATTS

    uint16_t                handle;  // GATTS
    mp_obj_t                value;   // GATTS

    mp_obj_t                callback;           // common
    mp_obj_t                callback_userdata;  // common

} network_bluetooth_char_descr_obj_t;

// "Service"
// Structure used for both GATTS and GATTC
typedef struct {
    mp_obj_base_t                       base;

    mp_obj_t                            connection;
    esp_gatt_srvc_id_t                  service_id;
    uint16_t                            handle;

    mp_obj_t                            chars;  // list
    bool                                started;
    bool                                valid;
    network_bluetooth_char_descr_obj_t* last_added_chr;

} network_bluetooth_service_obj_t;

// "Connection"

typedef struct {
    mp_obj_base_t           base;
    esp_bd_addr_t           bda;
    int32_t                 conn_id; // int32_t, so we can store -1 for disconnected.
    uint16_t                mtu;
    mp_obj_t                services;

} network_bluetooth_connection_obj_t;


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

    mp_obj_t                services;       // GATTS, implemented as a list

    mp_obj_t                connections;    // GATTC, implemented as a list

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
        network_bluetooth_singleton->adv_data.service_uuid_len = 0;
        network_bluetooth_singleton->adv_data.flag = 0;

        network_bluetooth_singleton->callback = mp_const_none;
        network_bluetooth_singleton->callback_userdata = mp_const_none;

        network_bluetooth_singleton->services = mp_obj_new_list(0, NULL);
        network_bluetooth_singleton->connections = mp_obj_new_list(0, NULL);
        memset(network_bluetooth_singleton->adv_params.peer_addr, 0, sizeof(network_bluetooth_singleton->adv_params.peer_addr));
    }
    return network_bluetooth_singleton;
}

#define CALLBACK_QUEUE_SIZE 10

STATIC QueueHandle_t callback_q = NULL;
STATIC QueueHandle_t read_write_q = NULL;

STATIC bool cbq_push(const callback_data_t* data) {
    return xQueueSend(callback_q, data, portMAX_DELAY) == pdPASS;
}

STATIC bool cbq_pop(callback_data_t* data) {
    return xQueueReceive(callback_q, data, 0) == pdPASS;
}

STATIC void dumpBuf(const uint8_t *buf, size_t len) {
    while (len--) {
        printf("%02X ", *buf++);
    }
}

STATIC bool mp_obj_is_bt_datatype(mp_obj_t o) {
    return MP_OBJ_IS_STR(o) || MP_OBJ_IS_BYTEARRAY_OR_BYTES(o) || o == mp_const_none;
}

STATIC void  mp_obj_is_bt_datatype_raise(mp_obj_t o) {
    if (!mp_obj_is_bt_datatype(o)) {
        mp_raise_ValueError("must be str, bytes, bytearray, or None");
    }
}

STATIC void parse_uuid(mp_obj_t src, esp_bt_uuid_t* target) {
    if (MP_OBJ_IS_INT(src)) {
        uint32_t arg_uuid = mp_obj_get_int(src);
        target->len = arg_uuid == (arg_uuid & 0xFFFF) ? sizeof(uint16_t) : sizeof(uint32_t);
        uint8_t * uuid_ptr = (uint8_t*)&target->uuid.uuid128;
        for (int i = 0; i < target->len; i++) {
            // LSB first
            *uuid_ptr++ = arg_uuid & 0xff;
            arg_uuid >>= 8;
        }
    } else if (!MP_OBJ_IS_BYTEARRAY_OR_BYTES(src)) {
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
    mp_raise_ValueError("uuid must be integer, bytearray(16), or bytes(16)");
}

static void uuid_to_uuid128(const esp_bt_uuid_t* in, esp_bt_uuid_t* out) {

    static const uint8_t base_uuid[] =
    {   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
        0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB
    };

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

STATIC void network_bluetooth_gatt_id_print(const mp_print_t *print, const esp_gatt_id_t* gatt_id) {
    esp_bt_uuid_t uuid128;
    uuid_to_uuid128(&gatt_id->uuid, &uuid128);
    for (int i = 0; i < uuid128.len; i++) {
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
    if (print != NULL) {
        mp_printf(print, ", inst_id=%02X", gatt_id->inst_id);
    } else  {
        NETWORK_BLUETOOTH_DEBUG_PRINTF(", inst_id=%02X", gatt_id->inst_id);
    }
}

typedef enum {
    NETWORK_BLUETOOTH_FIND_SERVICE,
    NETWORK_BLUETOOTH_FIND_CONNECTION,
    NETWORK_BLUETOOTH_DEL_CONNECTION,
    NETWORK_BLUETOOTH_FIND_CHAR_DESCR_IN,
    NETWORK_BLUETOOTH_FIND_CHAR_DESCR,
    NETWORK_BLUETOOTH_DEL_SERVICE,
} item_op_t;


// FIXME -- this could be more efficient.
STATIC mp_obj_t network_bluetooth_item_op(mp_obj_t list, esp_bt_uuid_t* uuid, uint16_t handle_or_conn_id,  esp_bd_addr_t bda, item_op_t kind) {
    ITEM_BEGIN();
    mp_obj_t ret = MP_OBJ_NULL;

    size_t len;
    mp_obj_t *items;
    mp_obj_get_array(list, &len, &items);
    for (int i = 0; i < len; i++) {
        switch (kind) {
            // These all search a services list
            case NETWORK_BLUETOOTH_FIND_SERVICE:
            case NETWORK_BLUETOOTH_FIND_CHAR_DESCR:
            case NETWORK_BLUETOOTH_DEL_SERVICE:
                {
                    network_bluetooth_service_obj_t* service = (network_bluetooth_service_obj_t*) items[i];

                    if (kind == NETWORK_BLUETOOTH_FIND_CHAR_DESCR) {
                        size_t char_len;
                        mp_obj_t *char_items;
                        mp_obj_get_array(service->chars, &char_len, &char_items);
                        for (int j = 0; j < char_len; j++) {
                            network_bluetooth_char_descr_obj_t* chr = (network_bluetooth_char_descr_obj_t*) char_items[j];
                            if ((uuid != NULL && uuid_eq(&chr->id.uuid, uuid)) || (uuid == NULL && chr->handle == handle_or_conn_id)) {
                                ret = chr;
                                goto NETWORK_BLUETOOTH_ITEM_END;
                            }
                            size_t descr_len;
                            mp_obj_t *descr_items;
                            mp_obj_get_array(chr->descrs, &descr_len, &descr_items);
                            for (int k = 0; k < descr_len; k++) {
                                network_bluetooth_char_descr_obj_t* descr = (network_bluetooth_char_descr_obj_t*) descr_items[k];
                                if ((uuid != NULL && uuid_eq(&descr->id.uuid, uuid)) || (uuid == NULL && descr->handle == handle_or_conn_id)) {
                                    ret = descr;
                                    goto NETWORK_BLUETOOTH_ITEM_END;
                                }
                            }
                        }

                    } else if ((uuid != NULL && uuid_eq(&service->service_id.id.uuid, uuid)) || (uuid == NULL && service->handle == handle_or_conn_id)) {
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

            case NETWORK_BLUETOOTH_FIND_CHAR_DESCR_IN:
                {
                    network_bluetooth_char_descr_obj_t* chr = (network_bluetooth_char_descr_obj_t*) items[i];
                    if ((uuid != NULL && uuid_eq(&chr->id.uuid, uuid)) || (uuid == NULL && chr->handle == handle_or_conn_id)) {
                        ret = chr;
                        goto NETWORK_BLUETOOTH_ITEM_END;
                    }
                    size_t descr_len;
                    mp_obj_t *descr_items;
                    mp_obj_get_array(chr->descrs, &descr_len, &descr_items);
                    for (int k = 0; k < descr_len; k++) {
                        network_bluetooth_char_descr_obj_t* descr = (network_bluetooth_char_descr_obj_t*) descr_items[k];
                        if ((uuid != NULL && uuid_eq(&descr->id.uuid, uuid)) || (uuid == NULL && descr->handle == handle_or_conn_id)) {
                            ret = descr;
                            goto NETWORK_BLUETOOTH_ITEM_END;
                        }
                    }
                }
                break;

            case NETWORK_BLUETOOTH_FIND_CONNECTION:
            case NETWORK_BLUETOOTH_DEL_CONNECTION:
                {
                    network_bluetooth_connection_obj_t* conn = (network_bluetooth_connection_obj_t*) items[i];
                    if ((bda != NULL && memcmp(conn->bda, bda, ESP_BD_ADDR_LEN) == 0) || (bda == NULL && conn->conn_id == handle_or_conn_id)) {
                        ret = conn;
                    }

                    if (kind == NETWORK_BLUETOOTH_DEL_CONNECTION) {
                        // a bit ineffecient, but
                        // list_pop() is static
                        mp_obj_list_remove(list, conn);
                    }
                    goto NETWORK_BLUETOOTH_ITEM_END;
                }
                break;
        }
    }

NETWORK_BLUETOOTH_ITEM_END:
    ITEM_END();
    return ret;
}

STATIC mp_obj_t network_bluetooth_find_service_by_handle(uint16_t handle) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    return network_bluetooth_item_op(bluetooth->services, NULL, handle, NULL, NETWORK_BLUETOOTH_FIND_SERVICE);
}

STATIC mp_obj_t network_bluetooth_find_connection_by_bda(esp_bd_addr_t bda) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    return network_bluetooth_item_op(bluetooth->connections, NULL, 0, bda, NETWORK_BLUETOOTH_FIND_CONNECTION);
}

STATIC mp_obj_t network_bluetooth_find_connection_by_conn_id(uint16_t conn_id) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    return network_bluetooth_item_op(bluetooth->connections, NULL, conn_id, NULL, NETWORK_BLUETOOTH_FIND_CONNECTION);
}

STATIC mp_obj_t network_bluetooth_find_service_by_uuid(esp_bt_uuid_t* uuid) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    return network_bluetooth_item_op(bluetooth->services, uuid, 0, NULL, NETWORK_BLUETOOTH_FIND_SERVICE);
}

STATIC mp_obj_t network_bluetooth_find_service_in_connection_by_uuid(network_bluetooth_connection_obj_t* connection, esp_bt_uuid_t* uuid) {
    return network_bluetooth_item_op(connection->services, uuid, 0, NULL, NETWORK_BLUETOOTH_FIND_SERVICE);
}

STATIC mp_obj_t network_bluetooth_find_char_descr_by_handle(uint16_t handle) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    return network_bluetooth_item_op(bluetooth->services, NULL, handle, NULL, NETWORK_BLUETOOTH_FIND_CHAR_DESCR);
}

STATIC mp_obj_t network_bluetooth_find_char_descr_in_char_descr_by_uuid(network_bluetooth_char_descr_obj_t* chr, esp_bt_uuid_t* uuid) {
    return network_bluetooth_item_op(chr->descrs, uuid, 0, NULL, NETWORK_BLUETOOTH_FIND_CHAR_DESCR_IN);
}

STATIC mp_obj_t network_bluetooth_find_char_descr_in_service_by_uuid(network_bluetooth_service_obj_t* service, esp_bt_uuid_t* uuid) {
    return network_bluetooth_item_op(service->chars, uuid, 0, NULL, NETWORK_BLUETOOTH_FIND_CHAR_DESCR_IN);
}

STATIC mp_obj_t network_bluetooth_find_next_char_in_service(network_bluetooth_service_obj_t* service, network_bluetooth_char_descr_obj_t* chr) {

    network_bluetooth_char_descr_obj_t* ret = MP_OBJ_NULL;
    bool returnNext = false;
    size_t len;
    mp_obj_t *items;
    ITEM_BEGIN();
    mp_obj_get_array(service->chars, &len, &items);
    for (int i = 0; i < len; i++) {
        if (returnNext) {
            ret = items[i];
            break;
        }
        returnNext = items[i] == chr;
    }
    ITEM_END();
    return ret;
}

STATIC mp_obj_t network_bluetooth_del_service_by_uuid(esp_bt_uuid_t* uuid) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    return network_bluetooth_item_op(bluetooth->services, uuid, 0, NULL, NETWORK_BLUETOOTH_DEL_SERVICE);
}

STATIC mp_obj_t network_bluetooth_del_connection_by_bda(esp_bd_addr_t bda) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    return network_bluetooth_item_op(bluetooth->connections, NULL, 0, bda, NETWORK_BLUETOOTH_DEL_CONNECTION);
}

STATIC void network_bluetooth_print_bda(const mp_print_t *print, esp_bd_addr_t bda) {
    if (print != NULL) {
        mp_printf(print, "%02X:%02X:%02X:%02X:%02X:%02X", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    } else {
        printf("%02X:%02X:%02X:%02X:%02X:%02X", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    }
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
        "REG",              // 0x00
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
        "<RESERVED>",       // 0x10
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
        "SCAN_FLT_PARAM",   // 0x20
        "SCAN_FLT_STATUS",  // 0x21
        "ADV_VSC",          // 0x22
        "GET_CHAR",         // 0x23
        "GET_DESCR",
        "GET_INCL_SRVC",
        "REG_FOR_NOTIFY",
        "UNREG_FOR_NOTIFY",  // 0x27
        "CONNECT",
        "DISCONNECT"
    };

    NETWORK_BLUETOOTH_DEBUG_PRINTF(
        "network_bluetooth_gattc_event_handler("
        "event = %02X / %s"
        ", if = %02X",
        event, event_names[event], gattc_if
    );

    NETWORK_BLUETOOTH_DEBUG_PRINTF(", param = (");

    switch (event) {
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
                switch (param->close.reason) {
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
                    ", conn_id = %04X"
                    ", mtu = %04X",
                    param->cfg_mtu.conn_id,
                    param->cfg_mtu.mtu);
            }
            break;

        case ESP_GATTC_SEARCH_CMPL_EVT:
            {
                PRINT_STATUS(param->search_cmpl.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    ", conn_id = %04X",
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
                network_bluetooth_gatt_id_print(NULL, &param->search_res.srvc_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->search_res.srvc_id.is_primary ? "True" : "False");
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");
            }
            break;

        case ESP_GATTC_READ_CHAR_EVT:
        case ESP_GATTC_READ_DESCR_EVT:
            {
                PRINT_STATUS(param->read.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", conn_id = %04X", param->read.conn_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->read.srvc_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->read.srvc_id.is_primary ? "True" : "False");
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->read.char_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", descr_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                dumpBuf((uint8_t*)&param->read.descr_id.uuid.uuid, param->read.descr_id.uuid.len);

                network_bluetooth_gatt_id_print(NULL, &param->read.descr_id);

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
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", conn_id = %04X", param->read.conn_id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", service_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->write.srvc_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->write.srvc_id.is_primary ? "True" : "False");
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->write.char_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", descr_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->write.descr_id);
            }
            break;

        case ESP_GATTC_EXEC_EVT:
            {
                PRINT_STATUS(param->exec_cmpl.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", conn_id = %04X", param->exec_cmpl.conn_id);
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
                network_bluetooth_gatt_id_print(NULL, &param->notify.srvc_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->notify.srvc_id.is_primary ? "True" : "False");
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->notify.char_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", descr_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->notify.descr_id);

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
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", conn_id = %04X", param->get_char.conn_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->get_char.srvc_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->get_char.srvc_id.is_primary ? "True" : "False");
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->get_char.char_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_props = ");
                const char * char_props[] = { "BROADCAST", "READ", "WRITE_NR", "WRITE", "NOTIFY", "INDICATE", "AUTH", "EXT_PROP" };
                bool printed = false;
                for (int i = 0; i < 7; i ++) {
                    if (param->get_char.char_prop & (1 << i)) {
                        NETWORK_BLUETOOTH_DEBUG_PRINTF("%s%s", printed ? " | " : "", char_props[i]);
                        printed = true;
                    }
                }
            }
            break;

        case ESP_GATTC_GET_DESCR_EVT:
            {
                PRINT_STATUS(param->get_descr.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", conn_id = %04X", param->get_descr.conn_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->get_descr.srvc_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->get_descr.srvc_id.is_primary ? "True" : "False");
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->get_descr.char_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", descr_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->get_descr.descr_id);
            }
            break;

        case ESP_GATTC_GET_INCL_SRVC_EVT:
            {
                PRINT_STATUS(param->get_incl_srvc.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", conn_id = %04X", param->get_incl_srvc.conn_id);

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->get_incl_srvc.srvc_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->get_incl_srvc.srvc_id.is_primary ? "True" : "False");
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", incl_srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->get_incl_srvc.srvc_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->get_incl_srvc.srvc_id.is_primary ? "True" : "False");
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

            }
            break;

        case ESP_GATTC_REG_FOR_NOTIFY_EVT:
            {
                PRINT_STATUS(param->reg_for_notify.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->reg_for_notify.srvc_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->reg_for_notify.srvc_id.is_primary ? "True" : "False");
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->reg_for_notify.char_id);
            }
            break;

        case ESP_GATTC_UNREG_FOR_NOTIFY_EVT:
            {
                PRINT_STATUS(param->unreg_for_notify.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", srvc_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->unreg_for_notify.srvc_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->unreg_for_notify.srvc_id.is_primary ? "True" : "False");
                NETWORK_BLUETOOTH_DEBUG_PRINTF(")");

                NETWORK_BLUETOOTH_DEBUG_PRINTF(", char_id = (");
                NETWORK_BLUETOOTH_DEBUG_PRINTF("uuid = ");
                network_bluetooth_gatt_id_print(NULL, &param->unreg_for_notify.char_id);
            }

        case ESP_GATTC_CONNECT_EVT:
            {
                PRINT_STATUS(param->connect.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    ", conn_id = %04X"
                    ", addr = %02X:%02X:%02X:%02X:%02X:%02X",
                    param->connect.conn_id,
                    param->connect.remote_bda[0],
                    param->connect.remote_bda[1],
                    param->connect.remote_bda[2],
                    param->connect.remote_bda[3],
                    param->connect.remote_bda[4],
                    param->connect.remote_bda[5]);
            }
            break;

        case ESP_GATTC_DISCONNECT_EVT:
            {
                PRINT_STATUS(param->disconnect.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    ", conn_id = %04X"
                    ", addr = %02X:%02X:%02X:%02X:%02X:%02X",
                    param->disconnect.conn_id,
                    param->disconnect.remote_bda[0],
                    param->disconnect.remote_bda[1],
                    param->disconnect.remote_bda[2],
                    param->disconnect.remote_bda[3],
                    param->disconnect.remote_bda[4],
                    param->disconnect.remote_bda[5]);
            }
            break;


        // Not printed
        case ESP_GATTC_UNREG_EVT:
        case ESP_GATTC_ACL_EVT:
        case ESP_GATTC_CANCEL_OPEN_EVT:
        case ESP_GATTC_ENC_CMPL_CB_EVT:
        case ESP_GATTC_ADV_DATA_EVT:
        case ESP_GATTC_MULT_ADV_ENB_EVT:
        case ESP_GATTC_MULT_ADV_UPD_EVT:
        case ESP_GATTC_MULT_ADV_DATA_EVT:
        case ESP_GATTC_MULT_ADV_DIS_EVT:
        case ESP_GATTC_BTH_SCAN_ENB_EVT:
        case ESP_GATTC_BTH_SCAN_CFG_EVT:
        case ESP_GATTC_BTH_SCAN_RD_EVT:
        case ESP_GATTC_BTH_SCAN_THR_EVT:
        case ESP_GATTC_BTH_SCAN_PARAM_EVT:
        case ESP_GATTC_BTH_SCAN_DIS_EVT:
        case ESP_GATTC_SCAN_FLT_CFG_EVT:
        case ESP_GATTC_SCAN_FLT_PARAM_EVT:
        case ESP_GATTC_SCAN_FLT_STATUS_EVT:
        case ESP_GATTC_ADV_VSC_EVT:
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

    NETWORK_BLUETOOTH_DEBUG_PRINTF(", param = (");

    switch (event) {
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
                param->read.is_long ? "True" : "False",
                param->read.need_rsp ? "True" : "False");

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
                    param->write.need_rsp ? "True" : "False",
                    param->write.is_prep ? "True" : "False",
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
                network_bluetooth_gatt_id_print(NULL, &param->create.service_id.id);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(", is_primary = %s", param->create.service_id.is_primary ? "True" : "False");
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
                param->connect.is_connected ? "True" : "False");
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
                param->disconnect.is_connected ? "True" : "False");
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
                param->congest.congested ? "True" : "False");
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
                for (int i = 0; i < param->add_attr_tab.num_handle; i++) {
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
        "SCAN_STOP_COMPLETE",
        "SET_STATIC_RAND_ADDR_EVT",
        "UPDATE_CONN_PARAMS_EVT",
        "SET_PKT_LENGTH_COMPLETE_EVT"
    };

    NETWORK_BLUETOOTH_DEBUG_PRINTF("network_bluetooth_gap_event_handler(event = %02X / %s", event, event_names[event]);
    NETWORK_BLUETOOTH_DEBUG_PRINTF(", param = (");
    switch (event) {

        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        case ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT:
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


        case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
            {
                PRINT_STATUS(param->pkt_data_lenth_cmpl.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    ", rx_len = %04X"
                    ", tx_len = %04X",
                    param->pkt_data_lenth_cmpl.params.rx_len,
                    param->pkt_data_lenth_cmpl.params.tx_len
                );

            }
            break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            {
                PRINT_STATUS(param->update_conn_params.status);
                NETWORK_BLUETOOTH_DEBUG_PRINTF(
                    ", bda = %02X:%02X:%02X:%02X:%02X:%02X"
                    ", min_int = %04X"
                    ", max_int = %04X"
                    ", latency = %04X"
                    ", conn_int = %04X"
                    ", timeout = %04X",
                    param->update_conn_params.bda[0],
                    param->update_conn_params.bda[1],
                    param->update_conn_params.bda[2],
                    param->update_conn_params.bda[3],
                    param->update_conn_params.bda[4],
                    param->update_conn_params.bda[5],
                    param->update_conn_params.min_int,
                    param->update_conn_params.max_int,
                    param->update_conn_params.latency,
                    param->update_conn_params.conn_int,
                    param->update_conn_params.timeout
                );
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

                if (param->scan_rst.dev_type <= 3 && param->scan_rst.ble_addr_type <= 3) {

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

        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            break;
        case ESP_GAP_BLE_KEY_EVT:
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT:
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT:
            break;
        case ESP_GAP_BLE_OOB_REQ_EVT:
            break;
        case ESP_GAP_BLE_LOCAL_IR_EVT:
            break;
        case ESP_GAP_BLE_LOCAL_ER_EVT:
            break;
        case ESP_GAP_BLE_NC_REQ_EVT:
            break;
    }

    NETWORK_BLUETOOTH_DEBUG_PRINTF(")\n");

}
#endif // EVENT_DEBUG

STATIC void network_bluetooth_disconnect_helper(network_bluetooth_connection_obj_t* connection) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    if (connection != MP_OBJ_NULL && connection->conn_id != -1) {
        esp_ble_gattc_close(bluetooth->gattc_interface, connection->conn_id);
        network_bluetooth_del_connection_by_bda(connection->bda);
        connection->conn_id = -1;
        connection->services = mp_obj_new_list(0, NULL);
    }
}

STATIC mp_obj_t network_bluetooth_callback_queue_handler(mp_obj_t arg) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();

    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        xQueueReset(callback_q);
        return mp_const_none;
    }

    callback_data_t cbdata;

    while (true) {
        bool got_data = cbq_pop(&cbdata);
        if (!got_data) {
            return mp_const_none;
        }

        switch (cbdata.event) {

            case NETWORK_BLUETOOTH_GATTS_CREATE:
                {
                    network_bluetooth_service_obj_t* service = network_bluetooth_find_service_by_uuid(&cbdata.gatts_create.service_id.id.uuid);

                    if (service != MP_OBJ_NULL) {
                        service->handle = cbdata.gatts_create.handle;
                        service->service_id = cbdata.gatts_create.service_id;
                        esp_ble_gatts_start_service(service->handle);
                    }
                }
                break;

            case NETWORK_BLUETOOTH_GATTS_STOP:
            case NETWORK_BLUETOOTH_GATTS_START:
                {
                    network_bluetooth_service_obj_t* service = network_bluetooth_find_service_by_handle(cbdata.gatts_start_stop.service_handle);
                    if (service == MP_OBJ_NULL) {
                        break;
                    }

                    switch (cbdata.event) {
                        case NETWORK_BLUETOOTH_GATTS_START:
                            {
                                service->started = true;

                                ITEM_BEGIN();
                                size_t len;
                                mp_obj_t *items;

                                mp_obj_get_array(service->chars, &len, &items);

                                // Only start first characteristic
                                //
                                // This is because the IDF only allows adding
                                // descriptors to the last-added charactersitic,
                                // so we have to add new chars after adding descriptros


                                if (len > 0) {
                                    network_bluetooth_char_descr_obj_t* chr = (network_bluetooth_char_descr_obj_t*) items[0];
                                    esp_ble_gatts_add_char(service->handle, &chr->id.uuid, chr->perm, chr->prop, NULL, NULL);
                                    service->last_added_chr = chr;
                                } else {
                                    service->last_added_chr = MP_OBJ_NULL;
                                }
                                ITEM_END();
                            }
                            break;

                        case NETWORK_BLUETOOTH_GATTS_STOP:
                            service->started = false;
                            esp_ble_gatts_delete_service(service->handle);
                            break;
                        default:
                            // Nothing, intentionally
                            break;
                    }

                }
                break;

            case NETWORK_BLUETOOTH_GATTS_ADD_CHAR:
            case NETWORK_BLUETOOTH_GATTS_ADD_CHAR_DESCR:
                {
                    network_bluetooth_service_obj_t* service = network_bluetooth_find_service_by_handle(cbdata.gatts_add_char_descr.service_handle);
                    if (service != MP_OBJ_NULL) {
                        network_bluetooth_char_descr_obj_t* chr = MP_OBJ_NULL;
                        if (cbdata.event == NETWORK_BLUETOOTH_GATTS_ADD_CHAR) {
                            // this works because UUIDs for chars must be unique within services.
                            // chr = network_bluetooth_find_char_descr_in_service_by_uuid(service, &cbdata.gatts_add_char_descr.uuid);
                            chr = service->last_added_chr;
                        } else { // It's a descriptor
                            chr = network_bluetooth_find_char_descr_in_char_descr_by_uuid(service->last_added_chr, &cbdata.gatts_add_char_descr.uuid);
                        }


                        if (chr != MP_OBJ_NULL) {
                            chr->handle = cbdata.gatts_add_char_descr.handle;
                            bool addNextChar = true;

                            size_t len;
                            mp_obj_t *items;

                            if (cbdata.event == NETWORK_BLUETOOTH_GATTS_ADD_CHAR) {
                                // Now add all descriptors
                                // `chr` is a char in this block
                                ITEM_BEGIN();
                                mp_obj_get_array(chr->descrs, &len, &items);
                                for (int j = 0; j < len; j++) {
                                    network_bluetooth_char_descr_obj_t* descr = (network_bluetooth_char_descr_obj_t*) items[j];
                                    esp_ble_gatts_add_char_descr(service->handle, &descr->id.uuid, descr->perm, NULL, NULL);
                                }
                                ITEM_END();

                            } else {
                                // check to see if all descriptors are registered before adding next char
                                ITEM_BEGIN();
                                mp_obj_get_array(service->last_added_chr->descrs, &len, &items);
                                for (int j = 0; j < len; j++) {
                                    network_bluetooth_char_descr_obj_t* descr = (network_bluetooth_char_descr_obj_t*) items[j];
                                    if (descr->handle == 0) {
                                        addNextChar = false;
                                        break;
                                    }
                                }
                                ITEM_END();
                            }

                            if (addNextChar) {
                                chr = network_bluetooth_find_next_char_in_service(service, chr);

                                if (chr != MP_OBJ_NULL) {
                                    esp_ble_gatts_add_char(service->handle, &chr->id.uuid, chr->perm, chr->prop, NULL, NULL);
                                    service->last_added_chr = chr;
                                }
                            }
                        }
                    }
                }
                break;

            case NETWORK_BLUETOOTH_READ:
            case NETWORK_BLUETOOTH_WRITE:
            case NETWORK_BLUETOOTH_NOTIFY:
                {
                    network_bluetooth_char_descr_obj_t* self = MP_OBJ_NULL;

                    mp_obj_t value = mp_const_none;
                    bool     need_rsp = false;

                    switch (cbdata.event) {
                        case NETWORK_BLUETOOTH_NOTIFY:
                            value = MP_OBJ_NEW_BYTES(cbdata.notify.value_len, cbdata.notify.value);
                            FREE(cbdata.notify.value);
                            network_bluetooth_connection_obj_t* conn = network_bluetooth_find_connection_by_conn_id(cbdata.notify.conn_id);
                            if (conn != MP_OBJ_NULL) {
                                network_bluetooth_service_obj_t* service = network_bluetooth_find_service_in_connection_by_uuid(conn, &cbdata.notify.service_id.id.uuid);
                                if (service != MP_OBJ_NULL) {
                                    self = network_bluetooth_find_char_descr_in_service_by_uuid(service, &cbdata.notify.char_id.uuid);
                                }
                            }
                            need_rsp = cbdata.notify.need_rsp;
                            break;

                        case NETWORK_BLUETOOTH_READ:
                            self = network_bluetooth_find_char_descr_by_handle(cbdata.read.handle);
                            if (self != MP_OBJ_NULL) {
                                value = self->value;
                            }
                            need_rsp = true;
                            break;

                        case NETWORK_BLUETOOTH_WRITE:
                            self = network_bluetooth_find_char_descr_by_handle(cbdata.write.handle);
                            value = MP_OBJ_NEW_BYTES(cbdata.write.value_len, cbdata.write.value);
                            need_rsp = cbdata.write.need_rsp;
                            FREE(cbdata.write.value);
                            break;

                        default:
                            // nothing, intentionally
                            break;
                    }

                    if (self != MP_OBJ_NULL) {

                        if (self->callback != mp_const_none) {
                            mp_obj_t args[] = {self, MP_OBJ_NEW_SMALL_INT(cbdata.event), value, self->callback_userdata };
                            value = mp_call_function_n_kw(self->callback, MP_ARRAY_SIZE(args), 0, args);
                        } else if (cbdata.event == NETWORK_BLUETOOTH_WRITE) {
                            self->value = value;
                        }

                        if (need_rsp) {
                            esp_gatt_rsp_t rsp;
                            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));

                            mp_buffer_info_t buf;
                            memset(&buf, 0, sizeof(mp_buffer_info_t));

                            // If not correct return data type,
                            // silently send an empty response
                            if (mp_obj_is_bt_datatype(value)) {
                                mp_get_buffer(value, &buf, MP_BUFFER_READ);
                            }

                            size_t len = MIN(ESP_GATT_MAX_ATTR_LEN, buf.len);

                            switch (cbdata.event) {

                                case NETWORK_BLUETOOTH_WRITE:
                                case NETWORK_BLUETOOTH_READ:
                                    rsp.attr_value.handle = cbdata.read.handle;
                                    rsp.attr_value.len = len;
                                    memcpy(rsp.attr_value.value, buf.buf, len);
                                    esp_ble_gatts_send_response(bluetooth->gatts_interface, cbdata.read.conn_id, cbdata.read.trans_id, ESP_GATT_OK, &rsp);
                                    break;
                                case NETWORK_BLUETOOTH_NOTIFY:
                                    // FIXME, How do you respond to a notify??
                                    // Do you even need to?
                                    break;

                                default:
                                    // nothing, intentionally
                                    break;
                            }
                        }
                    }
                }
                break;

            case NETWORK_BLUETOOTH_GATTS_CONNECT:
            case NETWORK_BLUETOOTH_GATTS_DISCONNECT:
                if (bluetooth->callback != mp_const_none) {
                    // items[0] is event
                    // items[1] is remote address
                    mp_obj_t args[] = {bluetooth, MP_OBJ_NEW_SMALL_INT(cbdata.event), MP_OBJ_NEW_BYTES(ESP_BD_ADDR_LEN, cbdata.gatts_connect_disconnect.bda), bluetooth->callback_userdata };
                    mp_call_function_n_kw(bluetooth->callback, MP_ARRAY_SIZE(args), 0, args);
                }
                break;

            case NETWORK_BLUETOOTH_GATTC_SCAN_CMPL:
            case NETWORK_BLUETOOTH_GATTC_SCAN_RES:
                if (bluetooth->callback != mp_const_none) {
                    mp_obj_t data = mp_const_none;
                    if (cbdata.event == NETWORK_BLUETOOTH_GATTC_SCAN_RES) {
                        mp_obj_t scan_res_args[] = {
                            MP_OBJ_NEW_BYTES(ESP_BD_ADDR_LEN, cbdata.gattc_scan_res.bda),
                            MP_OBJ_NEW_BYTES(cbdata.gattc_scan_res.adv_data_len, cbdata.gattc_scan_res.adv_data),
                            mp_obj_new_int(cbdata.gattc_scan_res.rssi)
                        } ;
                        data = mp_obj_new_tuple(3, scan_res_args);
                        FREE(cbdata.gattc_scan_res.adv_data);
                    }

                    mp_obj_t args[] = {bluetooth, MP_OBJ_NEW_SMALL_INT(cbdata.event), data, bluetooth->callback_userdata };
                    mp_call_function_n_kw(bluetooth->callback, MP_ARRAY_SIZE(args), 0, args);
                }
                break;

            case NETWORK_BLUETOOTH_GATTC_CLOSE:
                {
                    network_bluetooth_disconnect_helper(network_bluetooth_find_connection_by_conn_id(cbdata.gattc_open_close.conn_id));
                }
                break;

            case NETWORK_BLUETOOTH_GATTC_OPEN:
                {
                    network_bluetooth_connection_obj_t* conn = network_bluetooth_find_connection_by_bda(cbdata.gattc_open_close.bda);

                    if (conn != MP_OBJ_NULL) {
                        conn->conn_id = cbdata.gattc_open_close.conn_id;
                        conn->mtu = cbdata.gattc_open_close.mtu;
                        esp_ble_gattc_search_service(bluetooth->gattc_interface, cbdata.gattc_open_close.conn_id, NULL);
                    }
                }
                break;

            case NETWORK_BLUETOOTH_GATTC_SEARCH_RES:
                {
                    network_bluetooth_connection_obj_t* conn = network_bluetooth_find_connection_by_conn_id(cbdata.gattc_search_res.conn_id);
                    if (conn != MP_OBJ_NULL) {
                        network_bluetooth_service_obj_t *service = m_new_obj(network_bluetooth_service_obj_t);
                        service->base.type = &network_bluetooth_gattc_service_type;
                        service->valid = true;
                        service->service_id = cbdata.gattc_search_res.service_id;
                        service->chars = mp_obj_new_list(0, NULL);
                        service->connection = conn;
                        mp_obj_list_append(conn->services, service);
                        esp_ble_gattc_get_characteristic(bluetooth->gattc_interface, conn->conn_id, &service->service_id, NULL);
                    }
                }
                break;

            case NETWORK_BLUETOOTH_GATTC_GET_CHAR:
                {
                    network_bluetooth_connection_obj_t* conn = network_bluetooth_find_connection_by_conn_id(cbdata.gattc_get_char.conn_id);
                    if (conn != MP_OBJ_NULL) {
                        network_bluetooth_service_obj_t* service = network_bluetooth_find_service_in_connection_by_uuid(conn, &cbdata.gattc_get_char.service_id.id.uuid);
                        if (service != MP_OBJ_NULL) {
                            network_bluetooth_char_descr_obj_t *chr = m_new_obj(network_bluetooth_char_descr_obj_t);
                            chr->base.type = &network_bluetooth_gattc_char_type;
                            chr->id = cbdata.gattc_get_char.char_id;
                            chr->prop = cbdata.gattc_get_char.props;
                            chr->parent = service;
                            chr->descrs = mp_obj_new_list(0, NULL);

                            mp_obj_list_append(service->chars, chr);
                            esp_ble_gattc_get_descriptor(bluetooth->gattc_interface, conn->conn_id, &service->service_id, &chr->id, NULL);
                        }
                    }
                }
                break;

            case NETWORK_BLUETOOTH_GATTC_GET_DESCR:
                {
                    network_bluetooth_connection_obj_t* conn = network_bluetooth_find_connection_by_conn_id(cbdata.gattc_get_descr.conn_id);
                    if (conn != MP_OBJ_NULL) {
                        network_bluetooth_service_obj_t* service = network_bluetooth_find_service_in_connection_by_uuid(conn, &cbdata.gattc_get_descr.service_id.id.uuid);
                        if (service != MP_OBJ_NULL) {
                            network_bluetooth_char_descr_obj_t *chr =  network_bluetooth_find_char_descr_in_service_by_uuid(service, &cbdata.gattc_get_descr.char_id.uuid);
                            if (chr != NULL) {
                                network_bluetooth_char_descr_obj_t* descr = m_new_obj(network_bluetooth_char_descr_obj_t);
                                descr->base.type = &network_bluetooth_gattc_descr_type;
                                descr->parent = chr;
                                descr->id = cbdata.gattc_get_descr.descr_id;
                                mp_obj_list_append(chr->descrs, descr);
                            }
                        }
                    }
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

    read_write_data_t read_data = { 0 };
    callback_data_t cbdata;
    bool enqueue = true;
    switch (event) {
        case ESP_GATTC_REG_EVT:
            bluetooth->gattc_interface = gattc_if;
            enqueue = false;
            break;

        case ESP_GATTC_UNREG_EVT:
            bluetooth->gattc_interface = ESP_GATT_IF_NONE;
            break;

        case ESP_GATTC_OPEN_EVT:
            {
                cbdata.event = NETWORK_BLUETOOTH_GATTC_OPEN;
                cbdata.gattc_open_close.conn_id = param->open.conn_id;
                cbdata.gattc_open_close.mtu = param->open.mtu;
                memcpy(cbdata.gattc_open_close.bda, param->open.remote_bda, ESP_BD_ADDR_LEN);
            }
            break;

        case ESP_GATTC_CLOSE_EVT:
            {
                cbdata.event = NETWORK_BLUETOOTH_GATTC_CLOSE;
                cbdata.gattc_open_close.conn_id = param->close.conn_id;
                memcpy(cbdata.gattc_open_close.bda, param->open.remote_bda, ESP_BD_ADDR_LEN);
            }
            break;

        case ESP_GATTC_SEARCH_RES_EVT:
            {
                cbdata.event = NETWORK_BLUETOOTH_GATTC_SEARCH_RES;
                cbdata.gattc_search_res.conn_id = param->search_res.conn_id;
                cbdata.gattc_search_res.service_id = param->search_res.srvc_id;
            }
            break;

        case ESP_GATTC_GET_CHAR_EVT:
            {
                cbdata.event = NETWORK_BLUETOOTH_GATTC_GET_CHAR;
                cbdata.gattc_get_char.conn_id = param->get_char.conn_id;
                cbdata.gattc_get_char.service_id = param->get_char.srvc_id;
                cbdata.gattc_get_char.char_id = param->get_char.char_id;
                cbdata.gattc_get_char.props = param->get_char.char_prop;
            }
            break;

        case ESP_GATTC_GET_DESCR_EVT:
            {
                if (param->get_descr.status == ESP_GATT_OK) {
                    cbdata.event = NETWORK_BLUETOOTH_GATTC_GET_DESCR;
                    cbdata.gattc_get_descr.conn_id = param->get_descr.conn_id;
                    cbdata.gattc_get_descr.service_id = param->get_descr.srvc_id;
                    cbdata.gattc_get_descr.char_id = param->get_descr.char_id;
                    cbdata.gattc_get_descr.descr_id = param->get_descr.descr_id;
                }
            }
            break;

        case ESP_GATTC_READ_DESCR_EVT:
        case ESP_GATTC_READ_CHAR_EVT:
            read_data.value = MALLOC(param->read.value_len);
            read_data.value_len = param->read.value_len;

            if (read_data.value != NULL) {
                memcpy(read_data.value, param->read.value, param->read.value_len);
            }

        // fallthrough intentional
        case ESP_GATTC_WRITE_DESCR_EVT:
        case ESP_GATTC_WRITE_CHAR_EVT:
            xQueueSend(read_write_q, &read_data, portMAX_DELAY);
            enqueue = false;
            break;

        case ESP_GATTC_NOTIFY_EVT:
            {
                cbdata.notify.value = MALLOC(param->notify.value_len); // Returns NULL when len == 0
                cbdata.notify.value_len  = param->notify.value_len;

                if (cbdata.notify.value != NULL) {
                    memcpy(cbdata.notify.value, param->notify.value, param->notify.value_len);
                }

                cbdata.event = NETWORK_BLUETOOTH_NOTIFY;

                cbdata.notify.conn_id     = param->notify.conn_id;
                cbdata.notify.service_id  = param->notify.srvc_id;
                cbdata.notify.char_id     = param->notify.char_id;
                cbdata.notify.need_rsp    = !param->notify.is_notify;
            }
            break;

        default:
            enqueue = false;
            break;
    }

    if (enqueue) {
        cbq_push(&cbdata);
        mp_sched_schedule(MP_OBJ_FROM_PTR(&network_bluetooth_callback_queue_handler_obj), MP_OBJ_NULL);
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

    callback_data_t cbdata;
    bool enqueue = true;

    switch (event) {
        case ESP_GATTS_REG_EVT:
            bluetooth->gatts_interface = gatts_if;
            enqueue = false;
            break;

        case ESP_GATTS_UNREG_EVT:
            bluetooth->gatts_interface = ESP_GATT_IF_NONE;
            break;

        case ESP_GATTS_CREATE_EVT:
            {
                cbdata.event = NETWORK_BLUETOOTH_GATTS_CREATE;
                cbdata.gatts_create.handle = param->create.service_handle;
                cbdata.gatts_create.service_id = param->create.service_id;
            }
            break;

        case ESP_GATTS_START_EVT:
        case ESP_GATTS_STOP_EVT:
            {
                cbdata.event = event == ESP_GATTS_STOP_EVT ? NETWORK_BLUETOOTH_GATTS_STOP : NETWORK_BLUETOOTH_GATTS_START;
                cbdata.gatts_start_stop.service_handle = param->start.service_handle;
            }
            break;

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        case ESP_GATTS_ADD_CHAR_EVT:
            {
                cbdata.event = event == ESP_GATTS_ADD_CHAR_DESCR_EVT ? NETWORK_BLUETOOTH_GATTS_ADD_CHAR_DESCR : NETWORK_BLUETOOTH_GATTS_ADD_CHAR;
                cbdata.gatts_add_char_descr.service_handle = param->add_char.service_handle;
                cbdata.gatts_add_char_descr.handle = param->add_char.attr_handle;
                cbdata.gatts_add_char_descr.uuid = param->add_char.char_uuid;
            }
            break;

        case ESP_GATTS_WRITE_EVT:
            cbdata.write.value = MALLOC(param->write.len); // Returns NULL when len == 0

            if (cbdata.write.value != NULL) {
                cbdata.write.value_len  = param->write.len;
                memcpy(cbdata.write.value, param->write.value, param->write.len);
            } else {
                cbdata.write.value_len  = 0;
            }

        // fallthrough intentional

        case ESP_GATTS_READ_EVT:
            cbdata.event = event == ESP_GATTS_READ_EVT ? NETWORK_BLUETOOTH_READ : NETWORK_BLUETOOTH_WRITE;

            cbdata.read.handle      = param->read.handle;
            cbdata.read.conn_id     = param->read.conn_id;
            cbdata.read.trans_id    = param->read.trans_id;
            cbdata.read.need_rsp    = event == ESP_GATTS_READ_EVT || (event == ESP_GATTS_WRITE_EVT && param->write.need_rsp);
            break;

        case ESP_GATTS_CONNECT_EVT:
        case ESP_GATTS_DISCONNECT_EVT:
            {
                cbdata.event = event == ESP_GATTS_CONNECT_EVT ? NETWORK_BLUETOOTH_GATTS_CONNECT : NETWORK_BLUETOOTH_GATTS_DISCONNECT;
                cbdata.gatts_connect_disconnect.conn_id = param->connect.conn_id;
                memcpy(cbdata.gatts_connect_disconnect.bda, param->connect.remote_bda, ESP_BD_ADDR_LEN);
                bluetooth->conn_id = param->connect.conn_id;
                bluetooth->gatts_connected = event == ESP_GATTS_CONNECT_EVT;
            }
            break;

        default:
            enqueue = false;
            break;
    }

    if (enqueue) {
        cbq_push(&cbdata);
        mp_sched_schedule(MP_OBJ_FROM_PTR(&network_bluetooth_callback_queue_handler_obj), MP_OBJ_NULL);
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
            // No event sent here, since
            // the stop action is initiated by
            // user code, just like start.
            if (param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                bluetooth->scanning = false;
            }
            break;

        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            {
                callback_data_t cbdata;
                bool enqueue = true;
                switch (param->scan_rst.search_evt) {
                    case ESP_GAP_SEARCH_INQ_RES_EVT:
                        {
                            uint8_t* adv_name;
                            uint8_t  adv_name_len;

                            cbdata.event = NETWORK_BLUETOOTH_GATTC_SCAN_RES;
                            adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);

                            memcpy(cbdata.gattc_scan_res.bda, param->scan_rst.bda, ESP_BD_ADDR_LEN);
                            cbdata.gattc_scan_res.rssi = param->scan_rst.rssi;
                            cbdata.gattc_scan_res.adv_data = MALLOC(adv_name_len);

                            if (cbdata.gattc_scan_res.adv_data != NULL) {
                                cbdata.gattc_scan_res.adv_data_len = adv_name_len;
                                memcpy(cbdata.gattc_scan_res.adv_data, adv_name, adv_name_len);
                            } else {
                                cbdata.gattc_scan_res.adv_data_len = 0;
                            }
                            break;
                        }

                    case ESP_GAP_SEARCH_INQ_CMPL_EVT: // scanning done
                        {
                            bluetooth->scanning = false;
                            cbdata.event = NETWORK_BLUETOOTH_GATTC_SCAN_CMPL;
                        }
                        break;

                    default:
                        enqueue = false;
                        break;
                }

                if (enqueue) {
                    cbq_push(&cbdata);
                    mp_sched_schedule(MP_OBJ_FROM_PTR(&network_bluetooth_callback_queue_handler_obj), MP_OBJ_NULL);
                }
            }
            break;

        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            if (bluetooth->advertising) {
                esp_ble_gap_start_advertising(&bluetooth->adv_params);
            }
            break;

        default:
            // do nothing, intentionally
            break;
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

STATIC void network_bluetooth_char_descr_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {

    network_bluetooth_char_descr_obj_t * self = MP_OBJ_TO_PTR(self_in);

    enum {
        GATTC_CHAR,
        GATTS_CHAR,
        GATTC_DESCR,
        GATTS_DESCR
    } type;

    if (MP_OBJ_IS_TYPE(self_in, &network_bluetooth_gattc_descr_type)) {
        type = GATTC_DESCR;
        mp_printf(print, "GATTCDescr");
    }  else if (MP_OBJ_IS_TYPE(self_in, &network_bluetooth_gatts_char_type)) {
        type = GATTS_CHAR;
        mp_printf(print, "GATTSChar");
    } else if (MP_OBJ_IS_TYPE(self_in, &network_bluetooth_gattc_char_type)) {
        type = GATTC_CHAR;
        mp_printf(print, "GATTCChar");
    } else {
        type = GATTS_DESCR;
        mp_printf(print, "GATTSDescr");
    }

    mp_printf(print, "(uuid=");
    network_bluetooth_gatt_id_print(print, &self->id);

    if (type != GATTC_DESCR) {
        mp_printf(print, ", handle=%04X, perm=%02X, value=", self->handle, self->perm);
        mp_obj_print_helper(print, self->value, PRINT_REPR);
        if (type == GATTC_CHAR) {
            mp_printf(print, ", prop=%02X", self->prop);
        }
    }

    mp_printf(print, ")");
}

STATIC void network_bluetooth_connection_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {

    network_bluetooth_connection_obj_t *connection = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "GATTCConn(bda=");
    network_bluetooth_print_bda(print, connection->bda);
    mp_printf(print, ", connected=%s", connection->conn_id == -1 ? "False" : "True");
    mp_printf(print, ")");
}

STATIC void network_bluetooth_service_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_bluetooth_service_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bool gatts = MP_OBJ_IS_TYPE(self, &network_bluetooth_gatts_service_type);
    if (gatts) {
        mp_printf(print, "GATTSService(uuid=");
    } else {
        mp_printf(print, "GATTCService(uuid=");
    }
    network_bluetooth_gatt_id_print(print, &self->service_id.id);

    mp_printf(print, ", primary=%s", self->service_id.is_primary ? "True" : "False");
    if (gatts) {
        mp_printf(print, ", handle=%04X", self->handle);
    }
    mp_printf(print, ")");
}

STATIC void network_bluetooth_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_bluetooth_obj_t *self = MP_OBJ_TO_PTR(self_in);
    //#define NETWORK_BLUETOOTH_LF "\n"
#define NETWORK_BLUETOOTH_LF
    mp_printf(print,
              "Bluetooth(params=(conn_id=%04X" NETWORK_BLUETOOTH_LF
              ", gatts_connected=%s" NETWORK_BLUETOOTH_LF
              ", gatts_if=%u" NETWORK_BLUETOOTH_LF
              ", gattc_if=%u" NETWORK_BLUETOOTH_LF
              ", adv_params=("
              "adv_int_min=%u, " NETWORK_BLUETOOTH_LF
              "adv_int_max=%u, " NETWORK_BLUETOOTH_LF
              "adv_type=%u, " NETWORK_BLUETOOTH_LF
              "own_addr_type=%u, " NETWORK_BLUETOOTH_LF
              "peer_addr=%02X:%02X:%02X:%02X:%02X:%02X, " NETWORK_BLUETOOTH_LF
              "peer_addr_type=%u, " NETWORK_BLUETOOTH_LF
              "channel_map=%u, " NETWORK_BLUETOOTH_LF
              "adv_filter_policy=%u" NETWORK_BLUETOOTH_LF
              "state=%d" NETWORK_BLUETOOTH_LF
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
              self->adv_params.adv_filter_policy,
              self->state
             );
    mp_printf(print,
              ", data=("
              "set_scan_rsp=%s, " NETWORK_BLUETOOTH_LF
              "include_name=%s, " NETWORK_BLUETOOTH_LF
              "include_txpower=%s, " NETWORK_BLUETOOTH_LF
              "min_interval=%d, " NETWORK_BLUETOOTH_LF
              "max_interval=%d, " NETWORK_BLUETOOTH_LF
              "appearance=%d, " NETWORK_BLUETOOTH_LF
              "manufacturer_len=%d, " NETWORK_BLUETOOTH_LF
              "p_manufacturer_data=%s, " NETWORK_BLUETOOTH_LF
              "service_data_len=%d, " NETWORK_BLUETOOTH_LF
              "p_service_data=",
              self->adv_data.set_scan_rsp ? "True" : "False",
              self->adv_data.include_name ? "True" : "False",
              self->adv_data.include_txpower ? "True" : "False",
              self->adv_data.min_interval,
              self->adv_data.max_interval,
              self->adv_data.appearance,
              self->adv_data.manufacturer_len,
              self->adv_data.p_manufacturer_data ? (const char *)self->adv_data.p_manufacturer_data : "nil",
              self->adv_data.service_data_len);
    if (self->adv_data.p_service_data != NULL) {
        dumpBuf(self->adv_data.p_service_data, self->adv_data.service_data_len);
    }
    mp_printf(print, ", " NETWORK_BLUETOOTH_LF "flag=%d" NETWORK_BLUETOOTH_LF , self->adv_data.flag);
    mp_printf(print, ")");
}

// bluetooth.init(...)

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

    if (read_write_q == NULL) {
        read_write_q = xQueueCreate(1, sizeof(read_write_data_t));
        if (read_write_q == NULL) {
            mp_raise_msg(&mp_type_OSError, "unable to create read queue");
        }
    } else {
        xQueueReset(read_write_q);
    }

    if (self->state == NETWORK_BLUETOOTH_STATE_DEINIT) {

        self->state = NETWORK_BLUETOOTH_STATE_INIT;

        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_bt_controller_init() failed");
        }

        if (esp_bt_controller_enable(ESP_BT_MODE_BTDM) != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_bt_controller_enable() failed");
        }

        if (esp_bluedroid_init() != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_bluedroid_init() failed");
        }

        if (esp_bluedroid_enable() != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_bluedroid_enable() failed");
        }

        if (esp_ble_gatts_register_callback(network_bluetooth_gatts_event_handler) != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_ble_gatts_register_callback() failed");
        }

        if (esp_ble_gattc_register_callback(network_bluetooth_gattc_event_handler) != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_ble_gattc_register_callback() failed");
        }

        if (esp_ble_gap_register_callback(network_bluetooth_gap_event_handler) != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_ble_gap_register_callback() failed");
        }

        if (esp_ble_gatts_app_register(0) != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_ble_gatts_app_register() failed");
        }

        if (esp_ble_gattc_app_register(1) != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_ble_gattc_app_register() failed");
        }

    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_init_obj, network_bluetooth_init);

// bluetooth.ble_settings(...)

STATIC mp_obj_t network_bluetooth_ble_settings(size_t n_args, const mp_obj_t *pos_args, mp_map_t * kw_args) {
    network_bluetooth_obj_t *bluetooth = network_bluetooth_get_singleton();

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
        { MP_QSTR_adv_type,             MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1 }},
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
        if (!MP_OBJ_IS_TYPE(args[ARG_peer_addr].u_obj, &mp_type_bytearray)) {
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
        } else if (!MP_OBJ_IS_BYTEARRAY_OR_BYTES(args[ARG_adv_uuid].u_obj)) {
            printf("there\n");
            goto NETWORK_BLUETOOTH_BAD_UUID;
        }

        mp_get_buffer(args[ARG_adv_uuid].u_obj, &adv_uuid_buf, MP_BUFFER_READ);
        if (adv_uuid_buf.len != UUID_LEN) {
            printf("here\n");
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
            FREE(bluetooth->adv_data.p_manufacturer_data);
            bluetooth->adv_data.p_manufacturer_data = NULL;
        }

        bluetooth->adv_data.manufacturer_len = adv_man_name_buf.len;
        if (adv_man_name_buf.len > 0) {
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
            FREE(bluetooth->adv_data.p_service_uuid);
            bluetooth->adv_data.p_service_uuid = NULL;
        }

        bluetooth->adv_data.service_uuid_len = adv_uuid_buf.len;
        if (adv_uuid_buf.len > 0) {
            bluetooth->adv_data.p_service_uuid = MALLOC(adv_uuid_buf.len);
            bluetooth->adv_data.service_uuid_len = adv_uuid_buf.len;
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
    mp_raise_ValueError("adv_uuid must be bytearray(" TOSTRING(UUID_LEN) ")");
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(network_bluetooth_ble_settings_obj, 1, network_bluetooth_ble_settings);

// bluetooth.ble_adv_enable(...)

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

// bluetooth.scan_start(...)

STATIC mp_obj_t network_bluetooth_scan_start(size_t n_args, const mp_obj_t *pos_args, mp_map_t * kw_args) {
    network_bluetooth_obj_t *bluetooth = MP_OBJ_TO_PTR(pos_args[0]);

    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        mp_raise_msg(&mp_type_OSError, "bluetooth is deinit");
    }

    enum {
        // params
        ARG_timeout,
        ARG_scan_type,
        ARG_own_addr_type,
        ARG_scan_filter_policy,
        ARG_scan_interval,
        ARG_scan_window
    };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_timeout,            MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_scan_type,          MP_ARG_INT, {.u_int = BLE_SCAN_TYPE_ACTIVE} },
        { MP_QSTR_own_addr_type,      MP_ARG_INT, {.u_int = BLE_ADDR_TYPE_PUBLIC} },
        { MP_QSTR_scan_filter_policy, MP_ARG_INT, {.u_int = BLE_SCAN_FILTER_ALLOW_ALL }},
        { MP_QSTR_scan_interval,      MP_ARG_INT, {.u_int = 0x50 }},
        { MP_QSTR_scan_window,        MP_ARG_INT, {.u_int = 0x30 }},
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (bluetooth->scanning) {
        mp_raise_msg(&mp_type_OSError, "already scanning");
    }

    esp_ble_scan_params_t params = {
        .scan_type              = args[ARG_scan_type].u_int,
        .own_addr_type          = args[ARG_own_addr_type].u_int,
        .scan_filter_policy     = args[ARG_scan_filter_policy].u_int,
        .scan_interval          = args[ARG_scan_interval].u_int,
        .scan_window            = args[ARG_scan_window].u_int
    };

    assert(esp_ble_gap_set_scan_params(&params) == ESP_OK);
    assert(esp_ble_gap_start_scanning((uint32_t)args[ARG_timeout].u_int) == ESP_OK);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(network_bluetooth_scan_start_obj, 1, network_bluetooth_scan_start);

// bluetooth.scan_stop()

STATIC mp_obj_t network_bluetooth_scan_stop(mp_obj_t self_in) {
    (void)self_in;
    assert(esp_ble_gap_stop_scanning() == ESP_OK);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_scan_stop_obj, network_bluetooth_scan_stop);

// conn.connect(...)

STATIC mp_obj_t network_bluetooth_connect(mp_obj_t self_in, mp_obj_t bda) {
    network_bluetooth_obj_t * bluetooth = (network_bluetooth_obj_t *)self_in;
    mp_buffer_info_t bda_buf = { 0 };

    if (!MP_OBJ_IS_BYTEARRAY_OR_BYTES(bda)) {
        goto NETWORK_BLUETOOTH_connect_BAD_ADX;
    }
    mp_get_buffer(bda, &bda_buf, MP_BUFFER_READ);
    if (bda_buf.len != ESP_BD_ADDR_LEN) {
        goto NETWORK_BLUETOOTH_connect_BAD_ADX;
    }

    network_bluetooth_connection_obj_t* connection = m_new_obj(network_bluetooth_connection_obj_t);
    memset(connection, 0, sizeof(network_bluetooth_connection_obj_t));
    connection->base.type = &network_bluetooth_connection_type;
    connection->services = mp_obj_new_list(0, NULL);
    connection->conn_id = -1;
    memcpy(connection->bda, bda_buf.buf, ESP_BD_ADDR_LEN);

    mp_obj_list_append(bluetooth->connections, connection);

    esp_ble_gattc_open(bluetooth->gattc_interface, connection->bda, true);

    return connection;

NETWORK_BLUETOOTH_connect_BAD_ADX:
    mp_raise_ValueError("bda must be bytearray(" TOSTRING(ESP_BD_ADDR_LEN) ")");
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(network_bluetooth_connect_obj, network_bluetooth_connect);

// conn.services()

STATIC mp_obj_t network_bluetooth_connection_services(mp_obj_t self_in) {
    network_bluetooth_connection_obj_t* connection = (network_bluetooth_connection_obj_t*) self_in;
    return connection->services;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_connection_services_obj, network_bluetooth_connection_services);

// conn.is_connected()

STATIC mp_obj_t network_bluetooth_connection_is_connected(mp_obj_t self_in) {
    network_bluetooth_connection_obj_t* connection = (network_bluetooth_connection_obj_t*) self_in;
    return connection->conn_id == -1 ? mp_const_false : mp_const_true;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_connection_is_connected_obj, network_bluetooth_connection_is_connected);

// conn.disconnect()

STATIC mp_obj_t network_bluetooth_connection_disconnect(mp_obj_t self_in) {
    network_bluetooth_disconnect_helper(self_in);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_connection_disconnect_obj, network_bluetooth_connection_disconnect);

// bluetooth.callback(...)

STATIC mp_obj_t network_bluetooth_callback(size_t n_args, const mp_obj_t *args) {
    network_bluetooth_obj_t * self = network_bluetooth_get_singleton();
    return network_bluetooth_callback_helper(&self->callback, &self->callback_userdata, n_args, args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(network_bluetooth_callback_obj, 1, 3, network_bluetooth_callback);

// char.callback(...)
// descr.callback(...)

STATIC mp_obj_t network_bluetooth_char_descr_callback(size_t n_args, const mp_obj_t *args) {
    mp_obj_t ret;
    network_bluetooth_char_descr_obj_t* self = (network_bluetooth_char_descr_obj_t*) args[0];
    network_bluetooth_service_obj_t *service = self->parent;
    network_bluetooth_connection_obj_t *connection = service->connection;
    network_bluetooth_obj_t *bluetooth = network_bluetooth_get_singleton();

    assert(self != MP_OBJ_NULL);
    assert(service != MP_OBJ_NULL);

    ret = network_bluetooth_callback_helper(&self->callback, &self->callback_userdata, n_args, args);
    if (n_args > 1 && MP_OBJ_IS_TYPE(self, &network_bluetooth_gattc_char_type)) {
        assert(connection != MP_OBJ_NULL);
        if (self->callback != mp_const_none) {
            if (esp_ble_gattc_register_for_notify(bluetooth->gattc_interface, connection->bda, &service->service_id, &self->id) != ESP_OK) {
                goto NETWORK_BLUETOOTH_CHAR_CALLBACK_ERROR;
            }
        } else {
            if (esp_ble_gattc_unregister_for_notify(bluetooth->gattc_interface, connection->bda, &service->service_id, &self->id) != ESP_OK) {
                goto NETWORK_BLUETOOTH_CHAR_CALLBACK_ERROR;
            }
        }
    }

    return ret;

NETWORK_BLUETOOTH_CHAR_CALLBACK_ERROR:
    mp_raise_msg(&mp_type_OSError, "esp_ble_gattc_(un)register_for_notify() call failed");
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(network_bluetooth_char_descr_callback_obj, 1, 3, network_bluetooth_char_descr_callback);

// char.read()
// descr.read()

STATIC mp_obj_t network_bluetooth_char_descr_read(size_t n_args, const mp_obj_t *args) {
    bool is_chr;
    network_bluetooth_char_descr_obj_t *chr;
    network_bluetooth_char_descr_obj_t *descr;
    esp_err_t result;

    if (MP_OBJ_IS_TYPE(args[0], &network_bluetooth_gattc_char_type)) {
        is_chr = true;
        chr = MP_OBJ_TO_PTR(args[0]);
        descr = MP_OBJ_NULL;
    } else {
        is_chr = false;
        descr = MP_OBJ_TO_PTR(args[0]);
        chr = descr->parent;
    }

    network_bluetooth_service_obj_t *service = chr->parent;
    network_bluetooth_connection_obj_t *connection = service->connection;
    network_bluetooth_obj_t *bluetooth = network_bluetooth_get_singleton();

    assert(chr != MP_OBJ_NULL);
    assert(service != MP_OBJ_NULL);
    assert(connection != MP_OBJ_NULL);

    read_write_data_t read_data;

    // clear out any stale data
    if (xQueueReceive(read_write_q, &read_data, 0) == pdTRUE) {
        FREE(read_data.value);
    }
    xQueueReset(read_write_q);

    if (is_chr) {
        result = esp_ble_gattc_read_char(bluetooth->gattc_interface, connection->conn_id, &service->service_id, &chr->id, ESP_GATT_AUTH_REQ_NONE);
    } else {
        result = esp_ble_gattc_read_char_descr(bluetooth->gattc_interface, connection->conn_id, &service->service_id, &chr->id, &descr->id, ESP_GATT_AUTH_REQ_NONE);
    }

    if (result != ESP_OK) {
        mp_raise_msg(&mp_type_OSError, "read call failed");
    }

    TickType_t ticks = portMAX_DELAY;
    if (n_args > 1) {
        mp_float_t timeout = mp_obj_get_float(args[1]);
        if (timeout >= 0) {
            ticks = (TickType_t)timeout * 1000.0 / portTICK_PERIOD_MS;
        }
    }

    if (xQueueReceive(read_write_q, &read_data, ticks) == pdTRUE) {

        mp_obj_t res = MP_OBJ_NEW_BYTES(read_data.value_len, read_data.value);
        FREE(read_data.value);
        return res;
    }
    mp_raise_msg(&mp_type_OSError, "timed out");
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(network_bluetooth_char_descr_read_obj, 1, 2, network_bluetooth_char_descr_read);

// char.write(...)
// descr.write(...)

STATIC mp_obj_t network_bluetooth_char_descr_write(mp_obj_t self_in, mp_obj_t value) {
    bool is_chr;
    network_bluetooth_char_descr_obj_t *chr;
    network_bluetooth_char_descr_obj_t *descr;
    esp_err_t result;

    if (MP_OBJ_IS_TYPE(self_in, &network_bluetooth_gattc_char_type)) {
        is_chr = true;
        chr = MP_OBJ_TO_PTR(self_in);
        descr = MP_OBJ_NULL;
    } else {
        is_chr = false;
        descr = MP_OBJ_TO_PTR(self_in);
        chr = descr->parent;
    }

    network_bluetooth_service_obj_t *service = chr->parent;
    network_bluetooth_connection_obj_t *connection = service->connection;
    network_bluetooth_obj_t *bluetooth = network_bluetooth_get_singleton();

    assert(chr != MP_OBJ_NULL);
    assert(service != MP_OBJ_NULL);
    assert(connection != MP_OBJ_NULL);

    read_write_data_t read_data;

    // clear out any stale data
    if (xQueueReceive(read_write_q, &read_data, 0) == pdTRUE) {
        FREE(read_data.value);
    }

    xQueueReset(read_write_q);

    mp_buffer_info_t buf;
    mp_get_buffer_raise(value, &buf, MP_BUFFER_READ);

    if (is_chr) {
        result =
            esp_ble_gattc_write_char(
                bluetooth->gattc_interface,
                connection->conn_id,
                &service->service_id,
                &chr->id,
                buf.len,
                buf.buf,
                ESP_GATT_WRITE_TYPE_NO_RSP,
                ESP_GATT_AUTH_REQ_NONE);
    } else {
        result =
            esp_ble_gattc_write_char_descr(
                bluetooth->gattc_interface,
                connection->conn_id,
                &service->service_id,
                &chr->id,
                &descr->id,
                buf.len,
                buf.buf,
                ESP_GATT_WRITE_TYPE_NO_RSP,
                ESP_GATT_AUTH_REQ_NONE);
    }

    if (result != ESP_OK) {
        mp_raise_msg(&mp_type_OSError, "esp_ble_gattc_read_char() call failed");
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(network_bluetooth_char_descr_write_obj, network_bluetooth_char_descr_write);

STATIC mp_obj_t network_bluetooth_char_notify_helper(size_t n_args, const mp_obj_t *pos_args, mp_map_t * kw_args, bool need_confirm) {

    network_bluetooth_obj_t *bluetooth = network_bluetooth_get_singleton();
    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        mp_raise_msg(&mp_type_OSError, "bluetooth is deinit");
    }
    network_bluetooth_char_descr_obj_t* self = MP_OBJ_TO_PTR(pos_args[0]);

    enum {ARG_value};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_value,        MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL }},
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];

    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_value].u_obj != MP_OBJ_NULL) {
        mp_obj_is_bt_datatype_raise(args[ARG_value].u_obj);
    } else {
        args[ARG_value].u_obj = self->value;
    }

    mp_buffer_info_t buf = {0};
    mp_get_buffer(args[ARG_value].u_obj, &buf, MP_BUFFER_READ);
    esp_ble_gatts_send_indicate(bluetooth->gatts_interface, bluetooth->conn_id, self->handle, buf.len, buf.buf, need_confirm);
    return mp_const_none;
}

// char.notify(...)

STATIC mp_obj_t network_bluetooth_char_notify(size_t n_args, const mp_obj_t *pos_args, mp_map_t * kw_args) {
    return network_bluetooth_char_notify_helper(n_args, pos_args, kw_args, false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(network_bluetooth_char_notify_obj, 1, network_bluetooth_char_notify);

// char.indicate(...)

STATIC mp_obj_t network_bluetooth_char_indicate(size_t n_args, const mp_obj_t *pos_args, mp_map_t * kw_args) {
    return network_bluetooth_char_notify_helper(n_args, pos_args, kw_args, true);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(network_bluetooth_char_indicate_obj, 1, network_bluetooth_char_indicate);

// network.bluetooth(...)

STATIC mp_obj_t network_bluetooth_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    network_bluetooth_obj_t *self = network_bluetooth_get_singleton();
    if (n_args != 0 || n_kw != 0) {
        mp_raise_TypeError("Constructor takes no arguments");
    }

    network_bluetooth_init(self);
    return MP_OBJ_FROM_PTR(self);
}

// bluetooth.Service(...)

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
    network_bluetooth_service_obj_t *self = network_bluetooth_find_service_by_uuid(&uuid);

    if (self != MP_OBJ_NULL) {
        return MP_OBJ_FROM_PTR(self);
    }
    self = m_new_obj(network_bluetooth_service_obj_t);
    self->base.type = type;
    self->valid = true;
    self->service_id.id.uuid = uuid;

    self->service_id.is_primary = mp_obj_is_true(args[ARG_primary].u_obj);
    self->chars = mp_obj_new_list(0, NULL);
    self->connection = MP_OBJ_NULL;
    self->last_added_chr = MP_OBJ_NULL;

    ITEM_BEGIN();
    mp_obj_list_append(bluetooth->services, MP_OBJ_FROM_PTR(self));
    ITEM_END();

    return MP_OBJ_FROM_PTR(self);
}

// char.Descr(...)

STATIC mp_obj_t network_bluetooth_char_descr_make_new(size_t n_args, const mp_obj_t *pos_args, mp_map_t * kw_args) {

    enum {ARG_uuid, ARG_value, ARG_perm, ARG_prop};
    mp_arg_t allowed_args[] = {
        { MP_QSTR_uuid,     MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_value,    MP_ARG_OBJ, {.u_obj = mp_const_none }},
        { MP_QSTR_perm,     MP_ARG_INT, {.u_int = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE } },
        // "prop" is disabled for descriptors, below
        // Ensure the index of "prop" is updated if this list changes
        { MP_QSTR_prop,     MP_ARG_INT, {.u_int = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];

    network_bluetooth_service_obj_t* service = MP_OBJ_NULL;
    network_bluetooth_char_descr_obj_t* chr = MP_OBJ_NULL;

    if (MP_OBJ_IS_TYPE(pos_args[0], &network_bluetooth_gatts_service_type)) { // Make new char
        service = MP_OBJ_TO_PTR(pos_args[0]);
    } else { // Make new descriptor
        chr = MP_OBJ_TO_PTR(pos_args[0]);
        // Disable the "prop" argument
        allowed_args[3].qst = MP_QSTR_;
    }

    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_obj_is_bt_datatype_raise(args[ARG_value].u_obj);

    esp_bt_uuid_t uuid;
    parse_uuid(args[ARG_uuid].u_obj, &uuid);

    network_bluetooth_char_descr_obj_t *self = MP_OBJ_NULL;
    if (service != MP_OBJ_NULL) {
        self = network_bluetooth_find_char_descr_in_service_by_uuid(service, &uuid);
    } else {
        self = network_bluetooth_find_char_descr_in_char_descr_by_uuid(chr, &uuid);
    }

    if (self != MP_OBJ_NULL) {
        return MP_OBJ_FROM_PTR(self);
    }

    self = m_new_obj(network_bluetooth_char_descr_obj_t);

    self->callback = mp_const_none;

    self->id.uuid = uuid;
    self->value = args[ARG_value].u_obj;
    self->perm = args[ARG_perm].u_int;
    self->prop = args[ARG_prop].u_int;
    self->descrs = mp_obj_new_list(0, NULL);

    ITEM_BEGIN();
    if (service != MP_OBJ_NULL) {
        self->base.type = &network_bluetooth_gatts_char_type;
        self->parent = service;
        mp_obj_list_append(service->chars, self);
    } else {
        self->base.type = &network_bluetooth_gatts_descr_type;
        self->parent = chr;
        mp_obj_list_append(chr->descrs, self);
    }
    ITEM_END();

    return MP_OBJ_FROM_PTR(self);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(network_bluetooth_char_descr_make_new_obj, 1, network_bluetooth_char_descr_make_new);

// service.start()

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

    size_t numDescrs = 0;

    size_t index;
    mp_obj_t *items;
    mp_obj_get_array(self->chars, &index, &items);

    size_t numChars = index;

    while (index--) {
        network_bluetooth_char_descr_obj_t* chr = (network_bluetooth_char_descr_obj_t*)items[index];
        numDescrs += mp_obj_get_int(mp_obj_len(chr->descrs));
    }


    uint32_t then = mp_hal_ticks_ms();

    // Wait for registration
    while (bluetooth->gatts_interface == ESP_GATT_IF_NONE && mp_hal_ticks_ms() - then < 1000) {
        mp_hal_delay_ms(100);
    }

    if (bluetooth->gatts_interface == ESP_GATT_IF_NONE) {
        mp_raise_msg(&mp_type_OSError, "esp_ble_gatts_app_register() has timed out");
    }

    // Handle calculation:
    //
    // 1 for 2800/2801 Service declaration
    // 1x for each 2803 Char declaration
    // 1x for each Char desc declaration --- not used
    // 1x for each descriptor? Not sure
    esp_ble_gatts_create_service(bluetooth->gatts_interface, &self->service_id, 1 + numChars * 2 + numDescrs);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_service_start_obj, network_bluetooth_service_start);

// service.stop()

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

// service.close()

STATIC mp_obj_t network_bluetooth_service_close(mp_obj_t self_in) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    if (bluetooth->state != NETWORK_BLUETOOTH_STATE_INIT) {
        mp_raise_msg(&mp_type_OSError, "bluetooth is deinit");
    }

    network_bluetooth_service_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->valid) {
        mp_raise_msg(&mp_type_OSError, "service invalid");
    }

    network_bluetooth_del_service_by_uuid(&self->service_id.id.uuid);
    self->valid = false;
    if (self->handle != 0) {
        esp_ble_gatts_delete_service(self->handle);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_service_close_obj, network_bluetooth_service_close);

// char.service()
// descr.char()

STATIC mp_obj_t network_bluetooth_char_desc_parent(mp_obj_t self_in) {
    network_bluetooth_char_descr_obj_t* self = (network_bluetooth_char_descr_obj_t*) self_in;
    return self->parent;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_char_desc_parent_obj, network_bluetooth_char_desc_parent);

// char.value([value])
// descr.value([value])

STATIC mp_obj_t network_bluetooth_char_descr_value(size_t n_args, const mp_obj_t *args) {
    network_bluetooth_char_descr_obj_t* self = (network_bluetooth_char_descr_obj_t*) args[0];
    mp_obj_t value = self->value;
    if (n_args > 1) {
        self->value = args[1];
    }
    return value;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(network_bluetooth_char_descr_value_obj, 1, 2, network_bluetooth_char_descr_value);

// char.uuid()
// descr.uuid()

STATIC mp_obj_t network_bluetooth_char_descr_uuid(mp_obj_t self_in) {
    network_bluetooth_char_descr_obj_t* self = (network_bluetooth_char_descr_obj_t*) self_in;
    esp_bt_uuid_t uuid128;
    uuid_to_uuid128(&self->id.uuid, &uuid128);
    return MP_OBJ_NEW_BYTES(uuid128.len, uuid128.uuid.uuid128);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_char_descr_uuid_obj, network_bluetooth_char_descr_uuid);

// char.descrs()

STATIC mp_obj_t network_bluetooth_char_descrs(mp_obj_t self_in) {
    network_bluetooth_char_descr_obj_t* self = (network_bluetooth_char_descr_obj_t*) self_in;
    return self->descrs;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_char_descrs_obj, network_bluetooth_char_descrs);

// service.chars()

STATIC mp_obj_t network_bluetooth_service_chars(mp_obj_t self_in) {
    network_bluetooth_service_obj_t* self = (network_bluetooth_service_obj_t*) self_in;
    return self->chars;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_service_chars_obj, network_bluetooth_service_chars);

// service.uuid()

STATIC mp_obj_t network_bluetooth_service_uuid(mp_obj_t self_in) {
    network_bluetooth_service_obj_t* self = (network_bluetooth_service_obj_t*) self_in;
    esp_bt_uuid_t uuid128;
    uuid_to_uuid128(&self->service_id.id.uuid, &uuid128);
    return MP_OBJ_NEW_BYTES(uuid128.len, uuid128.uuid.uuid128);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_service_uuid_obj, network_bluetooth_service_uuid);

// service.is_primary()

STATIC mp_obj_t network_bluetooth_service_is_primary(mp_obj_t self_in) {
    network_bluetooth_service_obj_t* self = (network_bluetooth_service_obj_t*) self_in;
    return self->service_id.is_primary ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_service_is_primary_obj, network_bluetooth_service_is_primary);

// bluetooth.services()

STATIC mp_obj_t network_bluetooth_services(mp_obj_t self_in) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    return bluetooth ->services;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_services_obj, network_bluetooth_services);

// bluetooth.conns()

STATIC mp_obj_t network_bluetooth_conns(mp_obj_t self_in) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    return bluetooth->connections;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_conns_obj, network_bluetooth_conns);

// bluetooth.is_scanning()

STATIC mp_obj_t network_bluetooth_is_scanning(mp_obj_t self_in) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    return bluetooth ->scanning ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_is_scanning_obj, network_bluetooth_is_scanning);

// bluetooth.deinit()

STATIC mp_obj_t network_bluetooth_deinit(mp_obj_t self_in) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();

    if (bluetooth->state == NETWORK_BLUETOOTH_STATE_INIT) {

        bluetooth->state = NETWORK_BLUETOOTH_STATE_DEINIT;

        size_t len;
        mp_obj_t *items;
        mp_obj_get_array(bluetooth->services, &len, &items);
        for (int i = 0; i < len; i++) {
            network_bluetooth_service_obj_t* service = (network_bluetooth_service_obj_t*) items[i];
            if (service->handle != 0) {
                esp_ble_gatts_delete_service(service->handle);
            }
            service->valid = false;
        }

        if (bluetooth->gattc_interface != ESP_GATT_IF_NONE) {
            if (esp_ble_gattc_app_unregister(bluetooth->gattc_interface) != ESP_OK) {
                mp_raise_msg(&mp_type_OSError, "esp_ble_gattc_app_unregister() failed");
            }
        }

        if (bluetooth->gatts_interface != ESP_GATT_IF_NONE) {
            if (esp_ble_gatts_app_unregister(bluetooth->gatts_interface) != ESP_OK) {
                mp_raise_msg(&mp_type_OSError, "esp_ble_gattc_app_unregister() failed");
            }
        }

        if (esp_bluedroid_disable() != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_bluedroid_disable() failed");
        }

        if (esp_bluedroid_deinit() != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_bluedroid_deinit() failed");
        }

        if (esp_bt_controller_disable(ESP_BT_MODE_BTDM) != ESP_OK) {
            mp_raise_msg(&mp_type_OSError, "esp_bt_controller_disable(ESP_BT_MODE_BTDM) failed");
        }
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
    { MP_ROM_QSTR(MP_QSTR_services), MP_ROM_PTR(&network_bluetooth_services_obj) },
    { MP_ROM_QSTR(MP_QSTR_conns), MP_ROM_PTR(&network_bluetooth_conns_obj) },
    { MP_ROM_QSTR(MP_QSTR_callback), MP_ROM_PTR(&network_bluetooth_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_scan_start), MP_ROM_PTR(&network_bluetooth_scan_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_scan_stop), MP_ROM_PTR(&network_bluetooth_scan_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_scanning), MP_ROM_PTR(&network_bluetooth_is_scanning_obj) },

    // class constants

    // Callback types
    { MP_ROM_QSTR(MP_QSTR_CONNECT),                     MP_ROM_INT(NETWORK_BLUETOOTH_GATTS_CONNECT) },
    { MP_ROM_QSTR(MP_QSTR_DISCONNECT),                  MP_ROM_INT(NETWORK_BLUETOOTH_GATTS_DISCONNECT) },
    { MP_ROM_QSTR(MP_QSTR_READ),                        MP_ROM_INT(NETWORK_BLUETOOTH_READ) },
    { MP_ROM_QSTR(MP_QSTR_WRITE),                       MP_ROM_INT(NETWORK_BLUETOOTH_WRITE) },
    { MP_ROM_QSTR(MP_QSTR_NOTIFY),                      MP_ROM_INT(NETWORK_BLUETOOTH_NOTIFY) },
    { MP_ROM_QSTR(MP_QSTR_SCAN_RES),                    MP_ROM_INT(NETWORK_BLUETOOTH_GATTC_SCAN_RES) },
    { MP_ROM_QSTR(MP_QSTR_SCAN_CMPL),                   MP_ROM_INT(NETWORK_BLUETOOTH_GATTC_SCAN_CMPL) },

    // esp_ble_adv_type_t
    { MP_ROM_QSTR(MP_QSTR_ADV_TYPE_IND),                MP_ROM_INT(ADV_TYPE_IND) },
    { MP_ROM_QSTR(MP_QSTR_ADV_TYPE_DIRECT_IND_HIGH),    MP_ROM_INT(ADV_TYPE_DIRECT_IND_HIGH) },
    { MP_ROM_QSTR(MP_QSTR_ADV_TYPE_SCAN_IND),           MP_ROM_INT(ADV_TYPE_SCAN_IND) },
    { MP_ROM_QSTR(MP_QSTR_ADV_TYPE_NONCONN_IND),        MP_ROM_INT(ADV_TYPE_NONCONN_IND) },
    { MP_ROM_QSTR(MP_QSTR_ADV_TYPE_DIRECT_IND_LOW),     MP_ROM_INT(ADV_TYPE_DIRECT_IND_LOW) },

    // esp_ble_addr_type_t
    { MP_ROM_QSTR(MP_QSTR_BLE_ADDR_TYPE_PUBLIC),        MP_ROM_INT(BLE_ADDR_TYPE_PUBLIC) },
    { MP_ROM_QSTR(MP_QSTR_BLE_ADDR_TYPE_RANDOM),        MP_ROM_INT(BLE_ADDR_TYPE_RANDOM) },
    { MP_ROM_QSTR(MP_QSTR_BLE_ADDR_TYPE_RPA_PUBLIC),    MP_ROM_INT(BLE_ADDR_TYPE_RPA_PUBLIC) },
    { MP_ROM_QSTR(MP_QSTR_BLE_ADDR_TYPE_RPA_RANDOM),    MP_ROM_INT(BLE_ADDR_TYPE_RPA_RANDOM) },

    // esp_ble_adv_channel_t
    { MP_ROM_QSTR(MP_QSTR_ADV_CHNL_37),                 MP_ROM_INT(ADV_CHNL_37) },
    { MP_ROM_QSTR(MP_QSTR_ADV_CHNL_38),                 MP_ROM_INT(ADV_CHNL_38) },
    { MP_ROM_QSTR(MP_QSTR_ADV_CHNL_39),                 MP_ROM_INT(ADV_CHNL_39) },
    { MP_ROM_QSTR(MP_QSTR_ADV_CHNL_ALL),                MP_ROM_INT(ADV_CHNL_ALL) },

    // BLE_ADV_DATA_FLAG

    { MP_ROM_QSTR(MP_QSTR_BLE_ADV_FLAG_LIMIT_DISC),         MP_ROM_INT(ESP_BLE_ADV_FLAG_LIMIT_DISC) },
    { MP_ROM_QSTR(MP_QSTR_BLE_ADV_FLAG_GEN_DISC),           MP_ROM_INT(ESP_BLE_ADV_FLAG_GEN_DISC) },
    { MP_ROM_QSTR(MP_QSTR_BLE_ADV_FLAG_BREDR_NOT_SPT),      MP_ROM_INT(ESP_BLE_ADV_FLAG_BREDR_NOT_SPT) },
    { MP_ROM_QSTR(MP_QSTR_BLE_ADV_FLAG_DMT_CONTROLLER_SPT), MP_ROM_INT(ESP_BLE_ADV_FLAG_DMT_CONTROLLER_SPT) },
    { MP_ROM_QSTR(MP_QSTR_BLE_ADV_FLAG_DMT_HOST_SPT),       MP_ROM_INT(ESP_BLE_ADV_FLAG_DMT_HOST_SPT) },
    { MP_ROM_QSTR(MP_QSTR_BLE_ADV_FLAG_NON_LIMIT_DISC),     MP_ROM_INT(ESP_BLE_ADV_FLAG_NON_LIMIT_DISC) },

    // Scan param constants
    { MP_ROM_QSTR(MP_QSTR_SCAN_TYPE_PASSIVE),               MP_ROM_INT(BLE_SCAN_TYPE_PASSIVE) },
    { MP_ROM_QSTR(MP_QSTR_SCAN_TYPE_ACTIVE),                MP_ROM_INT(BLE_SCAN_TYPE_ACTIVE) },

    { MP_ROM_QSTR(MP_QSTR_SCAN_FILTER_ALLOW_ALL),           MP_ROM_INT(BLE_SCAN_FILTER_ALLOW_ALL) },
    { MP_ROM_QSTR(MP_QSTR_SCAN_FILTER_ALLOW_ONLY_WLST),     MP_ROM_INT(BLE_SCAN_FILTER_ALLOW_ONLY_WLST) },
    { MP_ROM_QSTR(MP_QSTR_SCAN_FILTER_ALLOW_UND_RPA_DIR),   MP_ROM_INT(BLE_SCAN_FILTER_ALLOW_UND_RPA_DIR) },
    { MP_ROM_QSTR(MP_QSTR_SCAN_FILTER_ALLOW_WLIST_PRA_DIR), MP_ROM_INT(BLE_SCAN_FILTER_ALLOW_WLIST_PRA_DIR) },


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
    {   MP_ROM_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY),
        MP_ROM_INT(ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY)
    },
    {   MP_ROM_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY),
        MP_ROM_INT(ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY)
    },
    {   MP_ROM_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST),
        MP_ROM_INT(ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST)
    },
    {   MP_ROM_QSTR(MP_QSTR_ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST),
        MP_ROM_INT(ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST)
    },
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_locals_dict, network_bluetooth_locals_dict_table);

const mp_obj_type_t network_bluetooth_type = {
    { &mp_type_type },
    .name = MP_QSTR_Bluetooth,
    .print = network_bluetooth_print,
    .make_new = network_bluetooth_make_new,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_locals_dict,
};

//
// CONNECTION OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_connection_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_services), MP_ROM_PTR(&network_bluetooth_connection_services_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_connected), MP_ROM_PTR(&network_bluetooth_connection_is_connected_obj) },
    { MP_ROM_QSTR(MP_QSTR_disconnect), MP_ROM_PTR(&network_bluetooth_connection_disconnect_obj) },
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_connection_locals_dict, network_bluetooth_connection_locals_dict_table);

const mp_obj_type_t network_bluetooth_connection_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTCConn,
    .print = network_bluetooth_connection_print,
    //.make_new = network_bluetooth_service_make_new,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_connection_locals_dict,
};

//
// GATTS SERVICE OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_gatts_service_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_Char), MP_ROM_PTR(&network_bluetooth_char_descr_make_new_obj) },
    { MP_ROM_QSTR(MP_QSTR_chars), MP_ROM_PTR(&network_bluetooth_service_chars_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_primary), MP_ROM_PTR(&network_bluetooth_service_is_primary_obj) },
    { MP_ROM_QSTR(MP_QSTR_uuid), MP_ROM_PTR(&network_bluetooth_service_uuid_obj) },
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
};

//
// GATTC SERVICE OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_gattc_service_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_chars), MP_ROM_PTR(&network_bluetooth_service_chars_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_primary), MP_ROM_PTR(&network_bluetooth_service_is_primary_obj) },
    { MP_ROM_QSTR(MP_QSTR_uuid), MP_ROM_PTR(&network_bluetooth_service_uuid_obj) },
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_gattc_service_locals_dict, network_bluetooth_gattc_service_locals_dict_table);

const mp_obj_type_t network_bluetooth_gattc_service_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTCService,
    .print = network_bluetooth_service_print,
    .make_new = network_bluetooth_service_make_new,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_gattc_service_locals_dict,
};

//
// GATTS CHAR OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_gatts_char_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_Descr), MP_ROM_PTR(&network_bluetooth_char_descr_make_new_obj) },
    { MP_ROM_QSTR(MP_QSTR_callback), MP_ROM_PTR(&network_bluetooth_char_descr_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_indicate), MP_ROM_PTR(&network_bluetooth_char_indicate_obj) },
    { MP_ROM_QSTR(MP_QSTR_notify), MP_ROM_PTR(&network_bluetooth_char_notify_obj) },
    { MP_ROM_QSTR(MP_QSTR_uuid), MP_ROM_PTR(&network_bluetooth_char_descr_uuid_obj) },
    { MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&network_bluetooth_char_descr_value_obj) },
    { MP_ROM_QSTR(MP_QSTR_service), MP_ROM_PTR(&network_bluetooth_char_desc_parent_obj) },
    { MP_ROM_QSTR(MP_QSTR_descrs), MP_ROM_PTR(&network_bluetooth_char_descrs_obj) },
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_gatts_char_locals_dict, network_bluetooth_gatts_char_locals_dict_table);

const mp_obj_type_t network_bluetooth_gatts_char_type = {
    { &mp_type_type },
    .name = MP_QSTR_Char,
    .print = network_bluetooth_char_descr_print,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_gatts_char_locals_dict,
};

//
// GATTC CHAR OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_gattc_char_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_callback), MP_ROM_PTR(&network_bluetooth_char_descr_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&network_bluetooth_char_descr_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&network_bluetooth_char_descr_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_service), MP_ROM_PTR(&network_bluetooth_char_desc_parent_obj) },
    { MP_ROM_QSTR(MP_QSTR_uuid), MP_ROM_PTR(&network_bluetooth_char_descr_uuid_obj) },
    { MP_ROM_QSTR(MP_QSTR_descrs), MP_ROM_PTR(&network_bluetooth_char_descrs_obj) },
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_gattc_char_locals_dict, network_bluetooth_gattc_char_locals_dict_table);

const mp_obj_type_t network_bluetooth_gattc_char_type = {
    { &mp_type_type },
    .name = MP_QSTR_Char,
    .print = network_bluetooth_char_descr_print,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_gattc_char_locals_dict,
};

//
// GATTC DESCR OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_gattc_descr_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&network_bluetooth_char_descr_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&network_bluetooth_char_descr_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_char), MP_ROM_PTR(&network_bluetooth_char_desc_parent_obj) },
    { MP_ROM_QSTR(MP_QSTR_uuid), MP_ROM_PTR(&network_bluetooth_char_descr_uuid_obj) },
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_gattc_descr_locals_dict, network_bluetooth_gattc_descr_locals_dict_table);

const mp_obj_type_t network_bluetooth_gattc_descr_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTCDescr,
    .print = network_bluetooth_char_descr_print,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_gattc_descr_locals_dict,
};

//
// GATTS DESCR OBJECTS
//

STATIC const mp_rom_map_elem_t network_bluetooth_gatts_descr_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_callback), MP_ROM_PTR(&network_bluetooth_char_descr_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_char), MP_ROM_PTR(&network_bluetooth_char_desc_parent_obj) },
    { MP_ROM_QSTR(MP_QSTR_uuid), MP_ROM_PTR(&network_bluetooth_char_descr_uuid_obj) },
    { MP_ROM_QSTR(MP_QSTR_value), MP_ROM_PTR(&network_bluetooth_char_descr_value_obj) },
};
STATIC MP_DEFINE_CONST_DICT(network_bluetooth_gatts_descr_locals_dict, network_bluetooth_gatts_descr_locals_dict_table);

const mp_obj_type_t network_bluetooth_gatts_descr_type = {
    { &mp_type_type },
    .name = MP_QSTR_GATTSDescr,
    .print = network_bluetooth_char_descr_print,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_gatts_descr_locals_dict,
};

