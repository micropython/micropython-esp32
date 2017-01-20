/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 * and Mnemote Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016, 2017 Nick Moore @mnemote
 *
 * Based on esp8266/modnetwork.c which is Copyright (c) 2015 Paul Sokolovsky
 * And the ESP IDF example code which is Public Domain / CC0
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

#include "py/nlr.h"
#include "py/objlist.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "netutils.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "lwip/dns.h"
#include "tcpip_adapter.h"

#define MODNETWORK_INCLUDE_CONSTANTS (1)

NORETURN void _esp_exceptions(esp_err_t e) {
   switch (e) {
      case ESP_ERR_WIFI_NOT_INIT: 
        mp_raise_msg(&mp_type_OSError, "Wifi Not Initialized");
      case ESP_ERR_WIFI_NOT_STARTED:
        mp_raise_msg(&mp_type_OSError, "Wifi Not Started");
      case ESP_ERR_WIFI_CONN:
        mp_raise_msg(&mp_type_OSError, "Wifi Internal Error");
      case ESP_ERR_WIFI_SSID:
        mp_raise_msg(&mp_type_OSError, "Wifi SSID Invalid");
      case ESP_ERR_WIFI_FAIL:
        mp_raise_msg(&mp_type_OSError, "Wifi Internal Failure");
      case ESP_ERR_WIFI_IF:
        mp_raise_msg(&mp_type_OSError, "Wifi Invalid Interface");
      case ESP_ERR_WIFI_MAC:
        mp_raise_msg(&mp_type_OSError, "Wifi Invalid MAC Address");
      case ESP_ERR_WIFI_ARG:
        mp_raise_msg(&mp_type_OSError, "Wifi Invalid Argument");
      case ESP_ERR_WIFI_MODE:
        mp_raise_msg(&mp_type_OSError, "Wifi Invalid Mode");
      case ESP_ERR_WIFI_PASSWORD:
        mp_raise_msg(&mp_type_OSError, "Wifi Invalid Password");
      case ESP_ERR_WIFI_NVS:
        mp_raise_msg(&mp_type_OSError, "Wifi Internal NVS Error");
      case ESP_ERR_TCPIP_ADAPTER_INVALID_PARAMS:
        mp_raise_msg(&mp_type_OSError, "TCP/IP Invalid Parameters");
      case ESP_ERR_TCPIP_ADAPTER_IF_NOT_READY:
        mp_raise_msg(&mp_type_OSError, "TCP/IP IF Not Ready");
      case ESP_ERR_TCPIP_ADAPTER_DHCPC_START_FAILED:
        mp_raise_msg(&mp_type_OSError, "TCP/IP DHCP Client Start Failed");
      case ESP_ERR_WIFI_TIMEOUT:
        mp_raise_OSError(MP_ETIMEDOUT);
      case ESP_ERR_TCPIP_ADAPTER_NO_MEM:
      case ESP_ERR_WIFI_NO_MEM:
        mp_raise_OSError(MP_ENOMEM); 
      default:
        nlr_raise(mp_obj_new_exception_msg_varg(
          &mp_type_RuntimeError, "Wifi Unknown Error 0x%04x", e
        ));
   }
}

static inline void esp_exceptions(esp_err_t e) {
    if (e != ESP_OK) _esp_exceptions(e);
}

#define ESP_EXCEPTIONS(x) do { esp_exceptions(x); } while (0);

typedef struct _wlan_if_obj_t {
    mp_obj_base_t base;
    int if_id;
} wlan_if_obj_t;

const mp_obj_type_t wlan_if_type;
STATIC const wlan_if_obj_t wlan_sta_obj = {{&wlan_if_type}, WIFI_IF_STA};
STATIC const wlan_if_obj_t wlan_ap_obj = {{&wlan_if_type}, WIFI_IF_AP};

//static wifi_config_t wifi_ap_config = {{{0}}};
static wifi_config_t wifi_sta_config = {{{0}}};

// Set to "true" if the STA interface is requested to be connected by the
// user, used for automatic reassociation.
static bool wifi_sta_connected = false;

// This function is called by the system-event task and so runs in a different
// thread to the main MicroPython task.  It must not raise any Python exceptions.
static esp_err_t event_handler(void *ctx, system_event_t *event) {
   switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI("wifi", "STA_START");
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI("wifi", "GOT_IP");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED: {
        // This is a workaround as ESP32 WiFi libs don't currently
        // auto-reassociate.
        system_event_sta_disconnected_t *disconn = &event->event_info.disconnected;
        ESP_LOGI("wifi", "STA_DISCONNECTED, reason:%d", disconn->reason);
        switch (disconn->reason) {
            case WIFI_REASON_AUTH_FAIL:
                mp_printf(MP_PYTHON_PRINTER, "authentication failed");
                wifi_sta_connected = false;
                break;
            default:
                // Let other errors through and try to reconnect.
                break;
        }
        if (wifi_sta_connected) {
            wifi_mode_t mode;
            if (esp_wifi_get_mode(&mode) == ESP_OK) {
                if (mode & WIFI_MODE_STA) {
                    // STA is active so attempt to reconnect.
                    esp_err_t e = esp_wifi_connect();
                    if (e != ESP_OK) {
                        mp_printf(MP_PYTHON_PRINTER, "error attempting to reconnect: 0x%04x", e);
                    }
                }
            }
        }
        break;
    }
    default:
        ESP_LOGI("wifi", "event %d", event->event_id);
        break;
    }
    return ESP_OK;
}

/*void error_check(bool status, const char *msg) {
    if (!status) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, msg));
    }
}
*/

STATIC void require_if(mp_obj_t wlan_if, int if_no) {
    wlan_if_obj_t *self = MP_OBJ_TO_PTR(wlan_if);
    if (self->if_id != if_no) {
        mp_raise_msg(&mp_type_OSError, if_no == WIFI_IF_STA ? "STA required" : "AP required");
    }
}

STATIC mp_obj_t get_wlan(mp_uint_t n_args, const mp_obj_t *args) {
    int idx = (n_args > 0) ? mp_obj_get_int(args[0]) : WIFI_IF_STA;
    if (idx == WIFI_IF_STA) {
        return MP_OBJ_FROM_PTR(&wlan_sta_obj);
    } else if (idx == WIFI_IF_AP) {
        return MP_OBJ_FROM_PTR(&wlan_ap_obj);
    } else {
        mp_raise_msg(&mp_type_ValueError, "invalid WLAN interface identifier");
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(get_wlan_obj, 0, 1, get_wlan);

STATIC mp_obj_t esp_initialize() {
    static int initialized = 0;
    if (!initialized) {
        ESP_LOGI("modnetwork", "Initializing TCP/IP");
        tcpip_adapter_init();
        ESP_LOGI("modnetwork", "Initializing Event Loop");
        ESP_EXCEPTIONS( esp_event_loop_init(event_handler, NULL) );
        ESP_LOGI("modnetwork", "esp_event_loop_init done");
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_LOGI("modnetwork", "Initializing WiFi");
        ESP_EXCEPTIONS( esp_wifi_init(&cfg) );
        ESP_EXCEPTIONS( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
        ESP_LOGI("modnetwork", "Initialized");
        ESP_EXCEPTIONS( esp_wifi_start() );
        ESP_LOGI("modnetwork", "Started");
        initialized = 1;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_initialize_obj, esp_initialize);

#if (WIFI_MODE_STA & WIFI_MODE_AP != WIFI_MODE_NULL || WIFI_MODE_STA | WIFI_MODE_AP != WIFI_MODE_APSTA)
#error WIFI_MODE_STA and WIFI_MODE_AP are supposed to be bitfields!
#endif

STATIC mp_obj_t esp_active(mp_uint_t n_args, const mp_obj_t *args) {

    wlan_if_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    wifi_mode_t mode;
    ESP_EXCEPTIONS( esp_wifi_get_mode(&mode) );
    int bit = (self->if_id == WIFI_IF_STA) ? WIFI_MODE_STA : WIFI_MODE_AP;

    if (n_args > 1) {
      bool active = mp_obj_is_true(args[1]);
      mode = active ? (mode | bit) : (mode & ~bit);
      ESP_EXCEPTIONS( esp_wifi_set_mode(mode) );
    }

    return (mode & bit) ? mp_const_true : mp_const_false;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp_active_obj, 1, 2, esp_active);

STATIC mp_obj_t esp_connect(mp_uint_t n_args, const mp_obj_t *args) {

    mp_uint_t len;
    const char *p;
    if (n_args > 1) {
        memset(&wifi_sta_config, 0, sizeof(wifi_sta_config));
        p = mp_obj_str_get_data(args[1], &len);
        memcpy(wifi_sta_config.sta.ssid, p, MIN(len, sizeof(wifi_sta_config.sta.ssid)));
        p = (n_args > 2) ? mp_obj_str_get_data(args[2], &len) : "";
        memcpy(wifi_sta_config.sta.password, p, MIN(len, sizeof(wifi_sta_config.sta.password)));
        ESP_EXCEPTIONS( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config) );
    }
    ESP_EXCEPTIONS( esp_wifi_connect() );
    wifi_sta_connected = true;

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp_connect_obj, 1, 7, esp_connect);

STATIC mp_obj_t esp_disconnect(mp_obj_t self_in) {
    wifi_sta_connected = false;
    ESP_EXCEPTIONS( esp_wifi_disconnect() );
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_disconnect_obj, esp_disconnect);

STATIC mp_obj_t esp_status(mp_obj_t self_in) {
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_status_obj, esp_status);

STATIC mp_obj_t esp_scan(mp_obj_t self_in) {
    // check that STA mode is active
    wifi_mode_t mode;
    ESP_EXCEPTIONS(esp_wifi_get_mode(&mode));
    if ((mode & WIFI_MODE_STA) == 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "STA must be active"));
    }

    mp_obj_t list = mp_obj_new_list(0, NULL);
    wifi_scan_config_t config = { 0 };
    // XXX how do we scan hidden APs (and if we can scan them, are they really hidden?)
    esp_err_t status = esp_wifi_scan_start(&config, 1);
    if (status == 0) {
        uint16_t count = 0;
        ESP_EXCEPTIONS( esp_wifi_scan_get_ap_num(&count) );
        wifi_ap_record_t *wifi_ap_records = calloc(count, sizeof(wifi_ap_record_t));
        ESP_EXCEPTIONS( esp_wifi_scan_get_ap_records(&count, wifi_ap_records) );
        for (uint16_t i = 0; i < count; i++) {
            mp_obj_tuple_t *t = mp_obj_new_tuple(6, NULL);
            uint8_t *x = memchr(wifi_ap_records[i].ssid, 0, sizeof(wifi_ap_records[i].ssid));
            int ssid_len = x ? x - wifi_ap_records[i].ssid : sizeof(wifi_ap_records[i].ssid);
            t->items[0] = mp_obj_new_bytes(wifi_ap_records[i].ssid, ssid_len);
            t->items[1] = mp_obj_new_bytes(wifi_ap_records[i].bssid, sizeof(wifi_ap_records[i].bssid));
            t->items[2] = MP_OBJ_NEW_SMALL_INT(wifi_ap_records[i].primary);
            t->items[3] = MP_OBJ_NEW_SMALL_INT(wifi_ap_records[i].rssi);
            t->items[4] = MP_OBJ_NEW_SMALL_INT(wifi_ap_records[i].authmode);
            t->items[5] = mp_const_false; // XXX hidden?
            mp_obj_list_append(list, MP_OBJ_FROM_PTR(t));
        }
        free(wifi_ap_records);
    }
    return list;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_scan_obj, esp_scan);

STATIC mp_obj_t esp_isconnected(mp_obj_t self_in) {
    wlan_if_obj_t *self = MP_OBJ_TO_PTR(self_in);
    require_if(self_in, WIFI_IF_STA);
    tcpip_adapter_ip_info_t info;
    tcpip_adapter_get_ip_info(self->if_id, &info);
    return mp_obj_new_bool(info.ip.addr != 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_isconnected_obj, esp_isconnected);

STATIC mp_obj_t esp_ifconfig(size_t n_args, const mp_obj_t *args) {
    wlan_if_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    tcpip_adapter_ip_info_t info;
    ip_addr_t dns_addr;
    tcpip_adapter_get_ip_info(self->if_id, &info);
    if (n_args == 1) {
        // get
        dns_addr = dns_getserver(0);
        mp_obj_t tuple[4] = {
            netutils_format_ipv4_addr((uint8_t*)&info.ip, NETUTILS_BIG),
            netutils_format_ipv4_addr((uint8_t*)&info.netmask, NETUTILS_BIG),
            netutils_format_ipv4_addr((uint8_t*)&info.gw, NETUTILS_BIG),
            netutils_format_ipv4_addr((uint8_t*)&dns_addr, NETUTILS_BIG),
        };
        return mp_obj_new_tuple(4, tuple);
    } else {
        // set
        mp_obj_t *items;
        mp_obj_get_array_fixed_n(args[1], 4, &items);
        netutils_parse_ipv4_addr(items[0], (void*)&info.ip, NETUTILS_BIG);
        if (mp_obj_is_integer(items[1])) {
            // allow numeric netmask, i.e.:
            // 24 -> 255.255.255.0
            // 16 -> 255.255.0.0
            // etc...
            uint32_t* m = (uint32_t*)&info.netmask;
            *m = htonl(0xffffffff << (32 - mp_obj_get_int(items[1])));
        } else {
            netutils_parse_ipv4_addr(items[1], (void*)&info.netmask, NETUTILS_BIG);
        }
        netutils_parse_ipv4_addr(items[2], (void*)&info.gw, NETUTILS_BIG);
        netutils_parse_ipv4_addr(items[3], (void*)&dns_addr, NETUTILS_BIG);
        // To set a static IP we have to disable DHCP first
        if (self->if_id == WIFI_IF_STA) {
            esp_err_t e = tcpip_adapter_dhcpc_stop(WIFI_IF_STA);
            if (e != ESP_OK && e != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED) _esp_exceptions(e);
            ESP_EXCEPTIONS(tcpip_adapter_set_ip_info(WIFI_IF_STA, &info));
        } else if (self->if_id == WIFI_IF_AP) {
            esp_err_t e = tcpip_adapter_dhcps_stop(WIFI_IF_AP);
            if (e != ESP_OK && e != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED) _esp_exceptions(e);
            ESP_EXCEPTIONS(tcpip_adapter_set_ip_info(WIFI_IF_AP, &info));
            ESP_EXCEPTIONS(tcpip_adapter_dhcps_start(WIFI_IF_AP));
        }
        return mp_const_none;
    }
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp_ifconfig_obj, 1, 2, esp_ifconfig);

STATIC mp_obj_t esp_config(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    if (n_args != 1 && kwargs->used != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError,
            "either pos or kw args are allowed"));
    }

    wlan_if_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    // get the config for the interface
    wifi_config_t cfg;
    ESP_EXCEPTIONS(esp_wifi_get_config(self->if_id, &cfg));

    if (kwargs->used != 0) {

        for (size_t i = 0; i < kwargs->alloc; i++) {
            if (MP_MAP_SLOT_IS_FILLED(kwargs, i)) {
                int req_if = -1;

                #define QS(x) (uintptr_t)MP_OBJ_NEW_QSTR(x)
                switch ((uintptr_t)kwargs->table[i].key) {
                    case QS(MP_QSTR_mac): {
                        mp_buffer_info_t bufinfo;
                        mp_get_buffer_raise(kwargs->table[i].value, &bufinfo, MP_BUFFER_READ);
                        if (bufinfo.len != 6) {
                            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError,
                                "invalid buffer length"));
                        }
                        ESP_EXCEPTIONS(esp_wifi_set_mac(self->if_id, bufinfo.buf));
                        break;
                    }
                    case QS(MP_QSTR_essid): {
                        req_if = WIFI_IF_AP;
                        mp_uint_t len;
                        const char *s = mp_obj_str_get_data(kwargs->table[i].value, &len);
                        len = MIN(len, sizeof(cfg.ap.ssid));
                        memcpy(cfg.ap.ssid, s, len);
                        cfg.ap.ssid_len = len;
                        break;
                    }
                    case QS(MP_QSTR_hidden): {
                        req_if = WIFI_IF_AP;
                        cfg.ap.ssid_hidden = mp_obj_is_true(kwargs->table[i].value);
                        break;
                    }
                    case QS(MP_QSTR_authmode): {
                        req_if = WIFI_IF_AP;
                        cfg.ap.authmode = mp_obj_get_int(kwargs->table[i].value);
                        break;
                    }
                    case QS(MP_QSTR_password): {
                        req_if = WIFI_IF_AP;
                        mp_uint_t len;
                        const char *s = mp_obj_str_get_data(kwargs->table[i].value, &len);
                        len = MIN(len, sizeof(cfg.ap.password) - 1);
                        memcpy(cfg.ap.password, s, len);
                        cfg.ap.password[len] = 0;
                        break;
                    }
                    case QS(MP_QSTR_channel): {
                        req_if = WIFI_IF_AP;
                        cfg.ap.channel = mp_obj_get_int(kwargs->table[i].value);
                        break;
                    }
                    default:
                        goto unknown;
                }
                #undef QS

                // We post-check interface requirements to save on code size
                if (req_if >= 0) {
                    require_if(args[0], req_if);
                }
            }
        }

        ESP_EXCEPTIONS(esp_wifi_set_config(self->if_id, &cfg));

        return mp_const_none;
    }

    // Get config

    if (n_args != 2) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError,
            "can query only one param"));
    }

    int req_if = -1;
    mp_obj_t val;

    #define QS(x) (uintptr_t)MP_OBJ_NEW_QSTR(x)
    switch ((uintptr_t)args[1]) {
        case QS(MP_QSTR_mac): {
            uint8_t mac[6];
            ESP_EXCEPTIONS(esp_wifi_get_mac(self->if_id, mac));
            return mp_obj_new_bytes(mac, sizeof(mac));
        }
        case QS(MP_QSTR_essid):
            req_if = WIFI_IF_AP;
            val = mp_obj_new_str((char*)cfg.ap.ssid, cfg.ap.ssid_len, false);
            break;
        case QS(MP_QSTR_hidden):
            req_if = WIFI_IF_AP;
            val = mp_obj_new_bool(cfg.ap.ssid_hidden);
            break;
        case QS(MP_QSTR_authmode):
            req_if = WIFI_IF_AP;
            val = MP_OBJ_NEW_SMALL_INT(cfg.ap.authmode);
            break;
        case QS(MP_QSTR_channel):
            req_if = WIFI_IF_AP;
            val = MP_OBJ_NEW_SMALL_INT(cfg.ap.channel);
            break;
        default:
            goto unknown;
    }
    #undef QS

    // We post-check interface requirements to save on code size
    if (req_if >= 0) {
        require_if(args[0], req_if);
    }

    return val;

unknown:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError,
        "unknown config param"));
}

STATIC MP_DEFINE_CONST_FUN_OBJ_KW(esp_config_obj, 1, esp_config);

STATIC const mp_map_elem_t wlan_if_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_active), (mp_obj_t)&esp_active_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect), (mp_obj_t)&esp_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect), (mp_obj_t)&esp_disconnect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_status), (mp_obj_t)&esp_status_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_scan), (mp_obj_t)&esp_scan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected), (mp_obj_t)&esp_isconnected_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_config), (mp_obj_t)&esp_config_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ifconfig), (mp_obj_t)&esp_ifconfig_obj },
};

STATIC MP_DEFINE_CONST_DICT(wlan_if_locals_dict, wlan_if_locals_dict_table);

const mp_obj_type_t wlan_if_type = {
    { &mp_type_type },
    .name = MP_QSTR_WLAN,
    .locals_dict = (mp_obj_t)&wlan_if_locals_dict,
};

STATIC mp_obj_t esp_phy_mode(mp_uint_t n_args, const mp_obj_t *args) {
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp_phy_mode_obj, 0, 1, esp_phy_mode);


STATIC const mp_map_elem_t mp_module_network_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_network) },
    { MP_OBJ_NEW_QSTR(MP_QSTR___init__), (mp_obj_t)&esp_initialize_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WLAN), (mp_obj_t)&get_wlan_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_phy_mode), (mp_obj_t)&esp_phy_mode_obj },

#if MODNETWORK_INCLUDE_CONSTANTS
    { MP_OBJ_NEW_QSTR(MP_QSTR_STA_IF),
        MP_OBJ_NEW_SMALL_INT(WIFI_IF_STA)},
    { MP_OBJ_NEW_QSTR(MP_QSTR_AP_IF),
        MP_OBJ_NEW_SMALL_INT(WIFI_IF_AP)},

    { MP_OBJ_NEW_QSTR(MP_QSTR_MODE_11B),
        MP_OBJ_NEW_SMALL_INT(WIFI_PROTOCOL_11B) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MODE_11G),
        MP_OBJ_NEW_SMALL_INT(WIFI_PROTOCOL_11G) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MODE_11N),
        MP_OBJ_NEW_SMALL_INT(WIFI_PROTOCOL_11N) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_OPEN),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_OPEN) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_WEP),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WEP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_WPA_PSK),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_WPA2_PSK),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA2_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_WPA_WPA2_PSK),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_WPA_WPA2_PSK) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AUTH_MAX),
        MP_OBJ_NEW_SMALL_INT(WIFI_AUTH_MAX) },
#endif
};

STATIC MP_DEFINE_CONST_DICT(mp_module_network_globals, mp_module_network_globals_table);

const mp_obj_module_t mp_module_network = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_network_globals,
};
