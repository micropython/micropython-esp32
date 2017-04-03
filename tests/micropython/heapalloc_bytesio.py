try:
    import uio
except ImportError:
    import sys
    print("SKIP")
    sys.exit()

import micropython

data = b"1234" * 16
buf = uio.BytesIO(64)

micropython.heap_lock()

buf.write(data)

micropython.heap_unlock()

print(buf.getvalue())
