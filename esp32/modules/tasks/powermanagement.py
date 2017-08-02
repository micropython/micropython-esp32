# File: powermanagement.py
# Version: 1
# Description: Power management task, puts the badge to sleep when idle
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import virtualtimers, deepsleep, badge, sys



requestedStandbyTime = 0
onSleepCallback = None

userResponseTime = badge.nvs_get_u16('splash', 'urt', 5000)

def pm_task():
    ''' The power management task [internal function] '''
    global requestedStandbyTime
    
    idleTime = virtualtimers.idle_time()
    print("[Power management] Next task wants to run in "+str(idleTime)+" ms.")
        
    if idleTime>30000 or idleTime<0:
        global onSleepCallback
        if not onSleepCallback==None:
            print("[Power management] Running onSleepCallback...")
            try:
                onSleepCallback(idleTime)
            except BaseException as e:
                print("[ERROR] An error occured in the on sleep callback.")
                sys.print_exception(e)

        if idleTime<0:
            print("[Power management] Sleeping forever...")
            deepsleep.start_sleeping()
        else:
            print("[Power management] Sleeping for "+str(idleTime)+" ms...")
            deepsleep.start_sleeping(idleTime)
    
    global userResponseTime
    return userResponseTime

def feed():
    ''' Start / resets the power management task '''
    global userResponseTime
    if not virtualtimers.update(userResponseTime, pm_task):
        virtualtimers.new(userResponseTime, pm_task)

def kill():
    ''' Kills the power management task '''
    virtualtimers.delete(pm_task)
    
def callback(cb):
    ''' Set a callback which is run before sleeping '''
    global onSleepCallback
    onSleepCallback = cb
