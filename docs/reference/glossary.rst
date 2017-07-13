Glossary
========

.. glossary::

    baremetal
        A system without (full-fledged) OS, like an :term:`MCU`. When
        running on a baremetal system, MicroPython effectively becomes
        its user-facing OS with a command interpreter (REPL).

    board
        A PCB board. Oftentimes, the term is used to denote a particular
        model of an :term:`MCU` system. Sometimes, it is used to actually
        refer to :term:`MicroPython port` to a particular board (and then
        may also refer to "boardless" ports like
        :term:`Unix port <MicroPython Unix port>`).

    CPython
        CPython is the reference implementation of Python programming
        language, and the most well-known one, which most of the people
        run. It is however one of many implementations (among which
        Jython, IronPython, PyPy, and many more, including MicroPython).
        As there is no formal specification of the Python language, only
        CPython documentation, it is not always easy to draw a line
        between Python the language and CPython its particular
        implementation. This however leaves more freedom for other
        implementations. For example, MicroPython does a lot of things
        differently than CPython, while still aspiring to be a Python
        language implementation.

    GPIO
        General-purpose input/output. The simplest means to control
        electrical signals. With GPIO, user can configure hardware
        signal pin to be either input or output, and set or get
        its digital signal value (logical "0" or "1"). MicroPython
        abstracts GPIO access using :class:`machine.Pin` and :class:`machine.Signal`
        classes.

    GPIO port
        A group of :term:`GPIO` pins, usually based on hardware
        properties of these pins (e.g. controllable by the same
        register).

    MCU
        Microcontroller. Microcontrollers usually have much less resources
        than a full-fledged computing system, but smaller, cheaper and
        require much less power. MicroPython is designed to be small and
        optimized enough to run on an average modern microcontroller.

    micropython-lib
        MicroPython is (usually) distributed as a single executable/binary
        file with just few builtin modules. There is no extensive standard
        library comparable with :term:`CPython`. Instead, there is a related, but
        separate project
        `micropython-lib <https://github.com/micropython/micropython-lib>`_
        which provides implementations for many modules from CPython's
        standard library. However, large subset of these modules required
        POSIX-like environment (Linux, MacOS, Windows may be partially
        supported), and thus would work or make sense only with MicroPython
        Unix port. Some subset of modules however usable for baremetal ports
        too.

        Unlike monolithic :term:`CPython` stdlib, micropython-lib modules
        are intended to be installed individually - either using manual
        copying or using :term:`upip`.

    MicroPython port
        MicroPython supports different :term:`boards <board>`, RTOSes,
        and OSes, and can be relatively easily adapted to new systems.
        MicroPython with support for a particular system is called a
        "port" to that system.

    MicroPython Unix port
        Unix port is one of the major :term:`MicroPython ports <MicroPython port>`.
        It is intended to run on POSIX-compatible operating systems, like
        Linux, MacOS, FreeBSD, Solaris, etc. It also serves as the basis
        of Windows port. The importance of Unix port lies in the fact
        that while there are many different :term:`boards <board>`, so
        two random users unlikely have the same board, almost all modern
        OSes have some level of POSIX compatibility, so Unix port serves
        as a kind of "common ground" to which any user can have access.
        So, Unix port is used for initial prototyping, different kinds
        of testing, development of machine-independent features, etc.
        All users of MicroPython, even those which are interested only
        in running MicroPython on :term:`MCU` systems, are recommended
        to be familiar with Unix (or Windows) port, as it is important
        productivity helper and a part of normal MicroPython workflow.

    port
        Either :term:`MicroPython port` or :term:`GPIO port`. If not clear
        from context, it's recommended to use full specification like one
        of the above.

    upip
        (Literally, "micro pip"). A package manage for MicroPython, inspired
        by :term:`CPython`'s pip, but much smaller and with reduced functionality.
        upip runs both on :term:`Unix port <MicroPython Unix port>` and on
        :term:`baremetal` ports (those which offer filesystem and networking
        support).
