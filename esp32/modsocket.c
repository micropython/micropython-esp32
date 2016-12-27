/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Nick Moore
 * Based on extmod/modlwip.c
 * Copyright (c) 2013, 2014 Damien P. George
 * Copyright (c) 2015 Galen Hazelwood
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
#include <stdlib.h>
#include <string.h>

#include "py/nlr.h"
#include "py/objlist.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"

typedef struct _socket_obj_t {
    mp_obj_base_t base;
    int fd;
    uint8_t domain;
    uint8_t type;
} socket_obj_t;

STATIC mp_obj_t socket_close(const mp_obj_t arg0) {
    socket_obj_t *self = MP_OBJ_TO_PTR(arg0);
    lwip_close(self->fd);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(socket_close_obj, socket_close);

int _socket_getaddrinfo2(const mp_obj_t host, const mp_obj_t port, struct addrinfo **resp) {
    mp_uint_t hostlen, portlen;
    const char *shost = mp_obj_str_get_data(host, &hostlen);
    const char *sport = mp_obj_str_get_data(port, &portlen);
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    lwip_getaddrinfo(shost, sport, &hints, resp);
    return 0;
}

int _socket_getaddrinfo(const mp_obj_t addrtuple, struct addrinfo **resp) {
    mp_uint_t len = 0;
    mp_obj_t *elem;
    mp_obj_get_array(addrtuple, &len, &elem);
    if (len == 2) return _socket_getaddrinfo2(elem[0], elem[1], resp);
    else return -1;
}

STATIC mp_obj_t socket_bind(const mp_obj_t arg0, const mp_obj_t arg1) {
    socket_obj_t *self = MP_OBJ_TO_PTR(arg0);
    struct addrinfo *res;
    _socket_getaddrinfo(arg1, &res);
    lwip_bind(self->fd, res->ai_addr, res->ai_addrlen);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_bind_obj, socket_bind);
    
STATIC mp_obj_t socket_listen(const mp_obj_t arg0, const mp_obj_t arg1) {
    socket_obj_t *self = MP_OBJ_TO_PTR(arg0);
    int backlog = mp_obj_get_int(arg1);
    int x = lwip_listen(self->fd, backlog);
    return (x == 0) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_listen_obj, socket_listen);

STATIC mp_obj_t socket_accept(const mp_obj_t arg0) {
    socket_obj_t *self = MP_OBJ_TO_PTR(arg0);
    int x = lwip_accept(self->fd, NULL, NULL);
    if (x >= 0) { 
        socket_obj_t *sock = (socket_obj_t *)calloc(1, sizeof(socket_obj_t));
        sock->base.type = self->base.type;
        sock->fd = x;
        return MP_OBJ_FROM_PTR(sock);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(socket_accept_obj, socket_accept);

STATIC mp_obj_t socket_connect(const mp_obj_t arg0, const mp_obj_t arg1) {
    socket_obj_t *self = MP_OBJ_TO_PTR(arg0);
    struct addrinfo *res;
    _socket_getaddrinfo(arg1, &res);
    int r = lwip_connect(self->fd, res->ai_addr, res->ai_addrlen);
    lwip_freeaddrinfo(res);
    return mp_obj_new_int(r);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_connect_obj, socket_connect);

STATIC mp_obj_t socket_settimeout(const mp_obj_t arg0, const mp_obj_t arg1) {
    //socket_t *self = MP_OBJ_TO_PTR(arg0);
    //int timeout = mp_obj_get_int(arg1);
    //lwip_settimeout(self->fd, timeout);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_settimeout_obj, socket_settimeout);

STATIC mp_obj_t socket_read(const mp_obj_t arg0) {
    byte buf[1024];
    socket_obj_t *self = MP_OBJ_TO_PTR(arg0);
    int x = lwip_recvfrom(self->fd, buf, sizeof(buf), MSG_DONTWAIT, NULL, NULL);
    if (x >= 0) return mp_obj_new_bytes(buf, x);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(socket_read_obj, socket_read);

STATIC mp_obj_t socket_write(const mp_obj_t arg0, const mp_obj_t arg1) {
    socket_obj_t *self = MP_OBJ_TO_PTR(arg0);
    mp_uint_t datalen;
    const char *data = mp_obj_str_get_data(arg1, &datalen);
    int x = lwip_write(self->fd, data, datalen);
    return mp_obj_new_int(x);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_write_obj, socket_write);

STATIC mp_obj_t socket_fileno(const mp_obj_t arg0) {
    socket_obj_t *self = MP_OBJ_TO_PTR(arg0);
    return mp_obj_new_int(self->fd);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(socket_fileno_obj, socket_fileno);

STATIC mp_uint_t socket_stream_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    socket_obj_t *socket = self_in;
    int x = lwip_recvfrom(socket->fd, buf, size, MSG_DONTWAIT, NULL, NULL);
    if (x >= 0) return x;
    *errcode = x;
    return 0;
}

STATIC mp_uint_t socket_stream_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    socket_obj_t *socket = self_in;
    int x = lwip_write(socket->fd, buf, size);
    if (x >= 0) return x;
    *errcode = x;
    return 0;
}

STATIC mp_uint_t socket_stream_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    return 0;
}

STATIC const mp_map_elem_t socket_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__), (mp_obj_t)&socket_close_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_close), (mp_obj_t)&socket_close_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_bind), (mp_obj_t)&socket_bind_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_listen), (mp_obj_t)&socket_listen_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_accept), (mp_obj_t)&socket_accept_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect), (mp_obj_t)&socket_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_send), mp_const_none },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sendall), mp_const_none },
    { MP_OBJ_NEW_QSTR(MP_QSTR_recv), mp_const_none },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sendto), mp_const_none },
    { MP_OBJ_NEW_QSTR(MP_QSTR_recvfrom), mp_const_none },
    { MP_OBJ_NEW_QSTR(MP_QSTR_setsockopt), mp_const_none },
    { MP_OBJ_NEW_QSTR(MP_QSTR_settimeout), mp_const_none },
    { MP_OBJ_NEW_QSTR(MP_QSTR_setblocking), mp_const_none },
    { MP_OBJ_NEW_QSTR(MP_QSTR_makefile), mp_const_none },
    { MP_OBJ_NEW_QSTR(MP_QSTR_fileno), (mp_obj_t)&socket_fileno_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_read), (mp_obj_t)&mp_stream_read_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_readline), (mp_obj_t)&mp_stream_unbuffered_readline_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_write), (mp_obj_t)&mp_stream_write_obj },
};
STATIC MP_DEFINE_CONST_DICT(socket_locals_dict, socket_locals_dict_table);

STATIC const mp_stream_p_t socket_stream_p = {
    .read = socket_stream_read,
    .write = socket_stream_write,
    .ioctl = socket_stream_ioctl
};

STATIC const mp_obj_type_t socket_type = {
    { &mp_type_type },
    .name = MP_QSTR_socket,
    .protocol = &socket_stream_p,
    .locals_dict = (mp_obj_t)&socket_locals_dict,
};

STATIC mp_obj_t get_socket(mp_uint_t n_args, const mp_obj_t *args) {
    socket_obj_t *sock = (socket_obj_t *)calloc(1, sizeof(socket_obj_t));
    sock->base.type = &socket_type;
    sock->fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    return MP_OBJ_FROM_PTR(sock);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(get_socket_obj, 0, 3, get_socket);

STATIC mp_obj_t esp_socket_getaddrinfo(const mp_obj_t host, const mp_obj_t port) {
    struct addrinfo *res;
    _socket_getaddrinfo2(host, port, &res);
    return mp_obj_new_bytes((const byte *)res->ai_addr, res->ai_addrlen);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(esp_socket_getaddrinfo_obj, esp_socket_getaddrinfo);

STATIC const mp_map_elem_t mp_module_socket_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_socket) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_socket), (mp_obj_t)&get_socket_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_getaddrinfo), (mp_obj_t)&esp_socket_getaddrinfo_obj },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_socket_globals, mp_module_socket_globals_table);

const mp_obj_module_t mp_module_socket = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_socket_globals,
};
