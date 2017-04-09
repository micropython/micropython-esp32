# this test ends while the 5 created threads are still running
# they should be deleted during the soft reset procedure, otherwise
# things will go south sooner rather than later.

import _thread
import time

def th_func(delay, id):
    while True:
        time.sleep(delay)

for i in range(5):
    _thread.start_new_thread(th_func, (i + 1, i))

time.sleep(1)
print("done")

raise SystemExit
