Get all the submodules for micropython if you haven't already

    cd micropython

    git submodule update --init --recursive

If you are missing mbedtls

    git clone https://github.com/ARMmbed/mbedtls.git lib/mbedtls

Build binary

    cd unix
    make

Run

    ./micropython
