try:
    try:
        import ubinascii as binascii
    except ImportError:
        import binascii
except ImportError:
    import sys
    print("SKIP")
    sys.exit()

print(binascii.b2a_base64(b''))
print(binascii.b2a_base64(b'f'))
print(binascii.b2a_base64(b'fo'))
print(binascii.b2a_base64(b'foo'))
print(binascii.b2a_base64(b'foob'))
print(binascii.b2a_base64(b'fooba'))
print(binascii.b2a_base64(b'foobar'))

print(binascii.b2a_base64(b'\x00\x01\x02\x03\x04\x05\x06\x07'))
print(binascii.b2a_base64(b'\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f'))
print(binascii.b2a_base64(b'\x7f\x80\xff'))
print(binascii.b2a_base64(b'1234ABCDabcd'))
print(binascii.b2a_base64(b'\x00\x00>')) # convert into '+'
