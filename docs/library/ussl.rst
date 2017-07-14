:mod:`ussl` -- SSL/TLS module
=============================

.. module:: ussl
   :synopsis: TLS/SSL wrapper for socket objects

|see_cpython_module| :mod:`python:ssl`.

This module provides access to Transport Layer Security (previously and
widely known as “Secure Sockets Layer”) encryption and peer authentication
facilities for network sockets, both client-side and server-side.

Functions
---------

.. function:: ssl.wrap_socket(sock, server_side=False, keyfile=None, certfile=None, cert_reqs=CERT_NONE, ca_certs=None)

   Takes a stream *sock* (usually usocket.socket instance of ``SOCK_STREAM`` type),
   and returns an instance of ssl.SSLSocket, which wraps the underlying stream in
   an SSL context. Returned object has the usual stream interface methods like
   `read()`, `write()`, etc. In MicroPython, the returned object does not expose
   socket interface and methods like `recv()`, `send()`. In particular, a
   server-side SSL socket should be created from a normal socket returned from
   `accept()` on a non-SSL listening server socket.

   Depending on the underlying module implementation for a particular board,
   some or all keyword arguments above may be not supported.

.. warning::

   Some implementations of ``ssl`` module do NOT validate server certificates,
   which makes an SSL connection established prone to man-in-the-middle attacks.

Exceptions
----------

.. data:: ssl.SSLError

   This exception does NOT exist. Instead its base class, OSError, is used.

Constants
---------

.. data:: ssl.CERT_NONE
          ssl.CERT_OPTIONAL
          ssl.CERT_REQUIRED

    Supported values for *cert_reqs* parameter.
