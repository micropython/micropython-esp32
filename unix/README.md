get all the submodules for micropython if you haven't already

    cd micropython

    git submodule update --init --recursive

Satisfy axtls generating its version.h

    cd lib/axtls
    make menuconfig

Build the binary

    cd unix
    make

