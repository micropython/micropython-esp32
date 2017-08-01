# File: virtualtimers.py
# Version: 1
# Description: Uses one hardware timer to simulate timers
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import machine

scheduler = []
timer = machine.Timer(0)
period = 0

def new(target, callback):
    global scheduler
    item = {"pos":0, "target":target, "cb":callback}
    scheduler.append(item)
    
def delete(callback):
    global scheduler
    newScheduler = scheduler
    found = False
    for i in range(0, len(scheduler)):
        if (scheduler[i]["cb"]==callback):
            newScheduler.pop(i)
            found = True
            break
    scheduler = newScheduler
    return found

def update(target, callback):
    global scheduler
    found = False
    for i in range(0, len(scheduler)):
        if (scheduler[i]["cb"]==callback):
            scheduler[i]["target"] = target
            found = True
    return found

def activate(p):
    global timer
    global period
    if p<1:
        return
    period = p
    timer.init(period=p, mode=machine.Timer.PERIODIC, callback=timer_callback)
    
def stop():
    global timer
    timer.deinit()
    
def timer_callback(tmr):
    global scheduler
    global period
    newScheduler = scheduler
    for i in range(0, len(scheduler)):
        scheduler[i]["pos"] += period
        if scheduler[i]["pos"] > scheduler[i]["target"]:
            newTarget = scheduler[i]["cb"]()
            if newTarget > 0:
                newScheduler[i]["pos"] = 0
                newScheduler[i]["target"] = newTarget
            else:
                newScheduler.pop(i)
    scheduler = newScheduler
