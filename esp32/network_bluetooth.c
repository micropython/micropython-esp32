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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "py/objstr.h"
#include "modmachine.h"

#include "bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_gatts_api.h"

#define HCI_GRP_HOST_CONT_BASEBAND_CMDS    0x03
#define HCI_GRP_BLE_CMDS                   0x08

#define H4_TYPE_COMMAND 0x01
#define H4_TYPE_ACL     0x02
#define H4_TYPE_SCO     0x03
#define H4_TYPE_EVENT   0x04


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MAKE_OPCODE(OGF, OCF) (((OGF) << 10) | (OCF))
#define MAKE_OPCODE_BYTES(OGF, OCF) { (MAKE_OPCODE(OGF, OCF) & 0xff), (MAKE_OPCODE(OGF, OCF) >> 8) }

#define BD_ADDR_LEN     (6)                     /* Device address length */
typedef uint8_t bd_addr_t[BD_ADDR_LEN];         /* Device address */

#define UINT16_TO_STREAM(p, u16) {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT8_TO_STREAM(p, u8)   {*(p)++ = (uint8_t)(u8);}
#define BDADDR_TO_STREAM(p, a)   {int ijk; for (ijk = 0; ijk < BD_ADDR_LEN;  ijk++) *(p)++ = (uint8_t) a[BD_ADDR_LEN - 1 - ijk];}
#define ARRAY_TO_STREAM(p, a, len) {int ijk; for (ijk = 0; ijk < len;        ijk++) *(p)++ = (uint8_t) a[ijk];}

#define UUID_LEN 16

//#define CUTOUT

const mp_obj_type_t network_bluetooth_type;
const mp_obj_type_t network_bluetooth_service_type;
const mp_obj_type_t network_bluetooth_characteristic_type;


// SERVICE
typedef struct {
    mp_obj_base_t base;

    bool started;
    esp_gatt_srvc_id_t service_id;
    mp_obj_t chars;  // list
} network_bluetooth_service_obj_t;



// CHARACTERISTIC

typedef struct {
    mp_obj_base_t           base;
    esp_bt_uuid_t           uuid;
    esp_gatt_perm_t         perm;
    esp_gatt_char_prop_t    prop;
    esp_attr_control_t      control;

    mp_obj_t value;

} network_bluetooth_characteristic_obj_t;

// Declaration
typedef struct {
    mp_obj_base_t           base;
    enum {
        NETWORK_BLUETOOTH_STATE_DEINIT,
        NETWORK_BLUETOOTH_STATE_INIT
    }                       state;
    bool                    advertising;
    esp_gatt_if_t           interface;
    esp_ble_adv_params_t    params;
    esp_ble_adv_data_t      data;

} network_bluetooth_obj_t;

// Singleton
STATIC network_bluetooth_obj_t network_bluetooth_singleton = { 
    .base = { &network_bluetooth_type },
    .state = NETWORK_BLUETOOTH_STATE_DEINIT,
    .advertising = false,
    .interface = ESP_GATT_IF_NONE,
    .params = {
        .adv_int_min = 1280 * 1.6,
        .adv_int_max = 1280 * 1.6,
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .peer_addr = { 0,0,0,0,0,0 },
        .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL, 
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    },
    .data = {
        .set_scan_rsp = false,
        .include_name = false,
        .include_txpower = false,
        .min_interval = 1280 * 1.6,
        .max_interval = 1280 * 1.6,
        .appearance = 0,
        .p_manufacturer_data = NULL,
        .manufacturer_len = 0,
        .p_service_data = NULL,
        .service_data_len = 0,
        .p_service_uuid = 0,
        .flag = 0
    },
}; 


#define NETWORK_BLUETOOTH_DEBUG_PRINTF(args...) printf(args)
#define CREATE_HCI_HOST_COMMAND(cmd)\
    size_t param_size = hci_commands[(cmd)].param_size;\
    size_t buf_size = 1 + sizeof(hci_cmd_def_t) + param_size;\
    uint8_t buf[buf_size];\
    uint8_t *param = buf + buf_size - param_size;\
    memset(buf, 0, buf_size);\
    buf[0] = H4_TYPE_COMMAND;\
    memcpy(buf + 1, &hci_commands[(cmd)], sizeof(hci_cmd_def_t)); 

STATIC network_bluetooth_obj_t* network_bluetooth_get_singleton() {
    return &network_bluetooth_singleton;
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

    typedef struct {
        uint8_t id; 
        const char * name;
    } status_name_t;

    const status_name_t status_names[] = {
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

    NETWORK_BLUETOOTH_DEBUG_PRINTF(
            "network_bluetooth_gatts_event_handler("
            "event = %02X / %s"
            ", if = %02X",
            event, event_names[event], gatts_if
            );

    NETWORK_BLUETOOTH_DEBUG_PRINTF( ", param = (");

#define PRINT_STATUS(STATUS) { \
    bool found = false; \
    NETWORK_BLUETOOTH_DEBUG_PRINTF("status = %02X / ", (STATUS)); \
    for(int i = 0; status_names[i].name != NULL; i++) { \
        if (status_names[i].id == (STATUS)) { \
            found = true; \
            NETWORK_BLUETOOTH_DEBUG_PRINTF("%s", status_names[i].name); \
            break; \
        } \
    } \
    if (!found) { \
        NETWORK_BLUETOOTH_DEBUG_PRINTF("???"); \
    }  \
}

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

STATIC void network_bluetooth_gatts_event_handler(
        esp_gatts_cb_event_t event, 
        esp_gatt_if_t gatts_if, 
        esp_ble_gatts_cb_param_t *param) {

    network_bluetooth_obj_t* self = &network_bluetooth_singleton;

    gatts_event_dump(event, gatts_if, param);

    switch (event) {
        case ESP_GATTS_REG_EVT:
            self->interface = gatts_if;
            break;

        case ESP_GATTS_CREATE_EVT:
            

            NETWORK_BLUETOOTH_DEBUG_PRINTF("ESP_GATTS_CREATE_EVT: FIXME, what to do with service_handle?\n");

            esp_ble_gatts_start_service(param->create.service_handle);

            /*
               gl_profile_tab[PROFILE_B_APP_ID].service_handle = param->create.service_handle;
               gl_profile_tab[PROFILE_B_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
               gl_profile_tab[PROFILE_B_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_B;

               esp_ble_gatts_start_service(gl_profile_tab[PROFILE_B_APP_ID].service_handle);

               esp_ble_gatts_add_char(
               gl_profile_tab[PROFILE_B_APP_ID].service_handle, &gl_profile_tab[PROFILE_B_APP_ID].char_uuid,
               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
               NULL, NULL);
               */

        default:
            // Nothing, intentionally
            break;

    }

}

STATIC void network_bluetooth_gap_event_handler(
        esp_gap_ble_cb_event_t event, 
        esp_ble_gap_cb_param_t *param) {

    NETWORK_BLUETOOTH_DEBUG_PRINTF("entering network_bluetooth_gap_event_handler()\n");
    network_bluetooth_obj_t* self = &network_bluetooth_singleton;
#ifndef CUTOUT

    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            NETWORK_BLUETOOTH_DEBUG_PRINTF( "adv data setting complete\n");
            if (self->advertising) {
                NETWORK_BLUETOOTH_DEBUG_PRINTF( "calling esp_ble_gap_start_advertising()\n");
                esp_ble_gap_start_advertising(&self->params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            //advertising start complete event to indicate advertising start successfully or failed
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                NETWORK_BLUETOOTH_DEBUG_PRINTF( "Advertising start failed\n");
            } else {
                NETWORK_BLUETOOTH_DEBUG_PRINTF("Advertising start successful\n");
            }

            break;
        default:
            break;
    }
#endif
}


STATIC void network_bluetooth_adv_updated() {
    network_bluetooth_obj_t* self = network_bluetooth_get_singleton();
#ifndef CUTOUT
    esp_ble_gap_stop_advertising();
    esp_ble_gap_config_adv_data(&self->data);

    // Restart will be handled in network_bluetooth_gap_event_handler

#endif
}

/******************************************************************************/
// MicroPython bindings for network_bluetooth
//

STATIC void network_bluetooth_uuid_print(const mp_print_t *print, esp_bt_uuid_t* uuid) {
    for(int i = 0; i < uuid->len; i++) {
        mp_printf(print, "%02X", uuid->uuid.uuid128[i]);
        if (uuid->len > 4 && (i == 3 || i == 5 || i == 7 || i == 9)) {
            mp_printf(print, "-");
        }
    }
}

STATIC void network_bluetooth_characteristic_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_bluetooth_characteristic_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "BTChar(uuid = ");
    network_bluetooth_uuid_print(print, &self->uuid);
    mp_printf(print, ", perm = %02X, prop = %02X", self->perm, self->prop);
    mp_printf(print, ", value = ");

    mp_obj_type_t *type = mp_obj_get_type(self->value);
    if (type->print != NULL)  {
        type->print(print, self->value, kind);
    }
    mp_printf(print, ")");
}

STATIC void network_bluetooth_service_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_bluetooth_service_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "Service(FIXME)", self);
}

STATIC void network_bluetooth_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_bluetooth_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "Bluetooth(params=())", self);
#define NETWORK_BLUETOOTH_LF "\n"
    NETWORK_BLUETOOTH_DEBUG_PRINTF(
            "Bluetooth(params = ("
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
            (unsigned int)(self->params.adv_int_min / 1.6),
            (unsigned int)(self->params.adv_int_max / 1.6),
            self->params.adv_type,
            self->params.own_addr_type,
            self->params.peer_addr[0],
            self->params.peer_addr[1],
            self->params.peer_addr[2],
            self->params.peer_addr[3],
            self->params.peer_addr[4],
            self->params.peer_addr[5],
            self->params.peer_addr_type,
            self->params.channel_map,
            self->params.adv_filter_policy
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
                    self->data.set_scan_rsp ? "true" : "false",
                    self->data.include_name ? "true" : "false",
                    self->data.include_txpower ? "true" : "false",
                    self->data.min_interval,
                    self->data.max_interval,
                    self->data.appearance,
                    self->data.manufacturer_len,
                    self->data.p_manufacturer_data ? (const char *)self->data.p_manufacturer_data : "nil",
                    self->data.service_data_len);
            if (self->data.p_service_data != NULL) {
                dumpBuf(self->data.p_service_data, self->data.service_data_len);
            } else {
                NETWORK_BLUETOOTH_DEBUG_PRINTF("nil");
            }

            NETWORK_BLUETOOTH_DEBUG_PRINTF(", " NETWORK_BLUETOOTH_LF "flag = %d" NETWORK_BLUETOOTH_LF , self->data.flag);


            NETWORK_BLUETOOTH_DEBUG_PRINTF(")\n");

}


STATIC mp_obj_t network_bluetooth_init(mp_obj_t self_in) {
    network_bluetooth_obj_t * self = (network_bluetooth_obj_t*)self_in;
    if (self->state == NETWORK_BLUETOOTH_STATE_DEINIT) {
        NETWORK_BLUETOOTH_DEBUG_PRINTF("BT is deinit, initializing\n");

#ifndef CUTOUT
        esp_bt_controller_config_t config = {
            .hci_uart_no = 1,
            .hci_uart_baudrate = 115200,
        };
        esp_bt_controller_init(&config); 

        switch(esp_bt_controller_enable(ESP_BT_MODE_BTDM)) {
            case ESP_OK:
                NETWORK_BLUETOOTH_DEBUG_PRINTF("esp_bt_controller_enable() OK\n");
                break;
            default:
                mp_raise_msg(&mp_type_OSError, "esp_bt_controller_enable() failed");
                break;
        }

        switch(esp_bluedroid_init()) {
            case ESP_OK:
                NETWORK_BLUETOOTH_DEBUG_PRINTF("esp_bluedroid_init() OK\nn");
                break;
            default:
                mp_raise_msg(&mp_type_OSError, "esp_bluedroid_init() failed");
                break;

        }
        switch(esp_bluedroid_enable()) {
            case ESP_OK:
                NETWORK_BLUETOOTH_DEBUG_PRINTF("esp_bluedroid_enable() OK\nn");
                break;
            default:
                mp_raise_msg(&mp_type_OSError, "esp_bluedroid_enable() failed");
                break;
        }

        esp_ble_gatts_register_callback(network_bluetooth_gatts_event_handler);
        esp_ble_gap_register_callback(network_bluetooth_gap_event_handler);

        // FIXME, this is hardcoded
        switch(esp_ble_gatts_app_register(0)) {
            case ESP_OK:
                NETWORK_BLUETOOTH_DEBUG_PRINTF("esp_ble_gatts_app_register success\n");
                break;

            default:
                NETWORK_BLUETOOTH_DEBUG_PRINTF("esp_ble_gatts_app_register ERROR\n");
                break;
        }
        self->state = NETWORK_BLUETOOTH_STATE_INIT;
#endif

    } else {
        NETWORK_BLUETOOTH_DEBUG_PRINTF("BT already initialized\n");
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_init_obj, network_bluetooth_init);

STATIC mp_obj_t network_bluetooth_ble_settings(size_t n_args, const mp_obj_t *pos_args, mp_map_t * kw_args) {
    network_bluetooth_obj_t *self = network_bluetooth_get_singleton();
    NETWORK_BLUETOOTH_DEBUG_PRINTF("Entering network_bluetooth_ble_settings(self = %p) n_args = %d\n", self, n_args);

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
        self->params.adv_int_min = args[ARG_int_min].u_int * 1.6; // 0.625 msec per count
        changed = true;
    }

    if (args[ARG_int_max].u_int != -1) {
        self->params.adv_int_max = args[ARG_int_max].u_int * 1.6;
        changed = true;
    }

    if (args[ARG_type].u_int != -1) {
        self->params.adv_type = args[ARG_type].u_int;
        changed = true;
    }

    if (args[ARG_own_addr_type].u_int != -1) {
        self->params.own_addr_type = args[ARG_own_addr_type].u_int;
        changed = true;
    }

    if (peer_addr_buf.buf != NULL) {
        memcpy(self->params.peer_addr, peer_addr_buf.buf, ESP_BD_ADDR_LEN);
        changed = true;
    }

    if (args[ARG_peer_addr_type].u_int != -1) {
        self->params.peer_addr_type = args[ARG_peer_addr_type].u_int;
        changed = true;
    }

    if (args[ARG_channel_map].u_int != -1) {
        self->params.channel_map = args[ARG_channel_map].u_int;
        changed = true;
    }

    if (args[ARG_filter_policy].u_int != -1) {
        self->params.adv_filter_policy = args[ARG_filter_policy].u_int;
        changed = true;
    }

    // update esp_ble_adv_data_t 
    //

    if (args[ARG_adv_is_scan_rsp].u_obj != NULL) {
        self->data.set_scan_rsp = mp_obj_is_true(args[ARG_adv_is_scan_rsp].u_obj);
        changed = true;
    }

#ifndef CUTOUT
    if (unset_adv_dev_name) {
        esp_ble_gap_set_device_name("");
        self->data.include_name = false;
    } else if (adv_dev_name_buf.buf != NULL) {
        esp_ble_gap_set_device_name(adv_dev_name_buf.buf);
        self->data.include_name = adv_dev_name_buf.len > 0;
        changed = true;
    }
#endif

    if (unset_adv_man_name || adv_man_name_buf.buf != NULL) {
        if (self->data.p_manufacturer_data != NULL) {
            NETWORK_BLUETOOTH_DEBUG_PRINTF("About to free p_manufacturer_data (2)\n");
            m_free(self->data.p_manufacturer_data);
            self->data.p_manufacturer_data = NULL;
        }

        self->data.manufacturer_len = adv_man_name_buf.len;
        if (adv_man_name_buf.len > 0) {
            NETWORK_BLUETOOTH_DEBUG_PRINTF("About to call malloc for p_manufacturer_data\n");
            self->data.p_manufacturer_data = m_malloc(adv_man_name_buf.len);
            memcpy(self->data.p_manufacturer_data, adv_man_name_buf.buf, adv_man_name_buf.len);
        }
        changed = true;
    }

    if (args[ARG_adv_inc_txpower].u_obj != NULL) {
        self->data.include_txpower = mp_obj_is_true(args[ARG_adv_inc_txpower].u_obj);
        changed = true;
    }

    if (args[ARG_adv_int_min].u_int != -1) {
        self->data.min_interval = args[ARG_adv_int_min].u_int;
        changed = true;
    }

    if (args[ARG_adv_int_max].u_int != -1) {
        self->data.max_interval = args[ARG_adv_int_max].u_int;
        changed = true;
    }

    if (args[ARG_adv_appearance].u_int != -1) {
        self->data.appearance = args[ARG_adv_appearance].u_int;
        changed = true;
    }

    if (unset_adv_uuid || adv_uuid_buf.buf != NULL) {

        if (self->data.p_service_uuid != NULL) {
            m_free(self->data.p_service_uuid );
            self->data.p_service_uuid = NULL;
        }

        self->data.service_uuid_len = adv_uuid_buf.len;
        if (adv_uuid_buf.len > 0) {
            self->data.p_service_uuid = m_malloc(adv_uuid_buf.len);
            memcpy(self->data.p_service_uuid, adv_uuid_buf.buf, adv_uuid_buf.len);
        }
        changed = true;
    }

    if (args[ARG_adv_flags].u_int != -1) {
        self->data.flag = args[ARG_adv_flags].u_int;
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

STATIC mp_obj_t network_bluetooth_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *all_args) { 
    network_bluetooth_obj_t *self = network_bluetooth_get_singleton();
    NETWORK_BLUETOOTH_DEBUG_PRINTF("Entering network_bluetooth_make_new, self = %p, n_args = %d, n_kw = %d\n", self, n_args, n_kw );
    if (n_args != 0 || n_kw != 0) {
        mp_raise_TypeError("Constructor takes no arguments");
    }

    network_bluetooth_init(self);
    return MP_OBJ_FROM_PTR(self);
}

mp_obj_t network_bluetooth_service_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {ARG_uuid, ARG_isPrimary};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uuid,         MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_isPrimary,    MP_ARG_OBJ, {.u_obj = mp_const_true } },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    network_bluetooth_service_obj_t *self = m_new_obj(network_bluetooth_service_obj_t );
    self->base.type = type;

    parse_uuid(args[ARG_uuid].u_obj, &self->service_id.id.uuid);
    self->service_id.is_primary = mp_obj_is_true(args[ARG_isPrimary].u_obj);
    self->chars = mp_obj_new_list(0, NULL);
    self->started = false;

    return MP_OBJ_FROM_PTR(self);
}

mp_obj_t network_bluetooth_characteristic_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {ARG_uuid, ARG_value, ARG_perm, ARG_prop};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_uuid,     MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_value,    MP_ARG_OBJ, {.u_obj = mp_const_none }},
        { MP_QSTR_perm,     MP_ARG_INT, {.u_int = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE } },
        { MP_QSTR_prop,     MP_ARG_INT, {.u_int = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    network_bluetooth_characteristic_obj_t *self = m_new_obj(network_bluetooth_characteristic_obj_t );
    self->base.type = type;

    if (args[ARG_value].u_obj != mp_const_none && !MP_OBJ_IS_STR_OR_BYTES(args[ARG_value].u_obj)) {
        mp_raise_ValueError("value must be string, bytearray or None");
    }
    parse_uuid(args[ARG_uuid].u_obj, &self->uuid);

    self->value = args[ARG_value].u_obj;
    self->perm = args[ARG_perm].u_int;
    self->prop = args[ARG_prop].u_int;

    return MP_OBJ_FROM_PTR(self);
}


STATIC mp_obj_t network_bluetooth_service_start(mp_obj_t self_in) {
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    network_bluetooth_service_obj_t* self = (network_bluetooth_service_obj_t*) self_in;
    if (self->started) {
        return mp_const_none;
    }

    mp_obj_t item;
    mp_obj_t iterable = mp_getiter(self->chars, NULL);
    while ((item = mp_iternext(iterable)) != MP_OBJ_STOP_ITERATION) {
     if (!MP_OBJ_IS_TYPE(item, &network_bluetooth_characteristic_type)){
         mp_raise_ValueError("chars must contain only Char objects");
     }
    }
    
    esp_ble_gatts_create_service(bluetooth->interface, &self->service_id, mp_obj_get_int(mp_obj_len(self->chars)) + 1);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_service_start_obj, network_bluetooth_service_start);

STATIC mp_obj_t network_bluetooth_service_stop(mp_obj_t self_in) {
    NETWORK_BLUETOOTH_DEBUG_PRINTF("network_bluetooth_service_stop() FIXME unimplemented\n");
    network_bluetooth_obj_t* bluetooth = network_bluetooth_get_singleton();
    network_bluetooth_service_obj_t* self = (network_bluetooth_service_obj_t*) self_in;

    if (!self->started) {
        return mp_const_none;
    }

    //FIXME
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_service_stop_obj, network_bluetooth_service_stop);


// service attribute handler
STATIC void network_bluetooth_service__attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    network_bluetooth_service_obj_t* self = (network_bluetooth_service_obj_t*) self_in;
    switch(attr) {
        case MP_QSTR_chars:
            if (dest[0] == MP_OBJ_NULL) {  // load
                dest[0] = self->chars;
            } else if (dest[1] != MP_OBJ_NULL) { // store
                if (MP_OBJ_IS_TYPE(dest[1], &mp_type_list) || MP_OBJ_IS_TYPE(dest[1], &mp_type_tuple)) {
                    self->chars = dest[1];
                    dest[0] = MP_OBJ_NULL;
                } else {
                    mp_raise_ValueError("value must be list or tuple");
                }
            } 
            break;
        case MP_QSTR_start:
            if (dest[0] == MP_OBJ_NULL) {
                dest[0] = mp_obj_new_bound_meth(MP_OBJ_FROM_PTR(&network_bluetooth_service_start_obj), self);
            }
            break;
        case MP_QSTR_stop:
            if (dest[0] == MP_OBJ_NULL) {
                dest[0] = mp_obj_new_bound_meth(MP_OBJ_FROM_PTR(&network_bluetooth_service_stop_obj), self);
            }
            break;
    }
}



STATIC mp_obj_t network_bluetooth_deinit(mp_obj_t self_in) {
    NETWORK_BLUETOOTH_DEBUG_PRINTF("Entering network_bluetooth_deinit\n");
    // FIXME
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_deinit_obj, network_bluetooth_deinit);

// BLUETOOTH OBJECTS

STATIC const mp_rom_map_elem_t network_bluetooth_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR_ble_settings), MP_ROM_PTR(&network_bluetooth_ble_settings_obj) },
    { MP_ROM_QSTR(MP_QSTR_ble_adv_enable), MP_ROM_PTR(&network_bluetooth_ble_adv_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_ble_adv_enable), MP_ROM_PTR(&network_bluetooth_ble_adv_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&network_bluetooth_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&network_bluetooth_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_Service), MP_ROM_PTR(&network_bluetooth_service_type) },
    { MP_ROM_QSTR(MP_QSTR_Char), MP_ROM_PTR(&network_bluetooth_characteristic_type) },

    // class constants

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
};


// SERVICE OBJECTS

STATIC const mp_rom_map_elem_t network_bluetooth_service_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&network_bluetooth_service_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&network_bluetooth_service_stop_obj) },
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_service_locals_dict, network_bluetooth_service_locals_dict_table);

const mp_obj_type_t network_bluetooth_service_type = {
    { &mp_type_type },
    .name = MP_QSTR_Service,
    .print = network_bluetooth_service_print,
    .make_new = network_bluetooth_service_make_new,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_service_locals_dict,
    .attr = network_bluetooth_service__attr,
};

// CHARACTERISTIC OBJECTS


STATIC const mp_rom_map_elem_t network_bluetooth_characteristic_locals_dict_table[] = {
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_characteristic_locals_dict, network_bluetooth_characteristic_locals_dict_table);

const mp_obj_type_t network_bluetooth_characteristic_type = {
    { &mp_type_type },
    .name = MP_QSTR_Char,
    .print = network_bluetooth_characteristic_print,
    .make_new = network_bluetooth_characteristic_make_new,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_characteristic_locals_dict,
};
