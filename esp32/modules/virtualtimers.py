# File: virtualtimers.py
# Version: 1
# Description: Uses one hardware timer to simulate timers
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import machine, sys

scheduler = []
timer = machine.Timer(0)
period = 0
debugEnabled = False

def debug(s):
    global debugEnabled
    debugEnabled = s

def new(target, callback, hfpm=False):
    ''' Creates new task. Arguments: time until callback is called, callback, hide from power management '''
    global scheduler
    item = {"pos":0, "target":target, "cb":callback, "hfpm":hfpm}
    scheduler.append(item)
    
def idle_time():
    ''' Returns time until next task in ms, ignores tasks hidden from power management '''
    global scheduler
    idleTime = 86400000 # One day (causes the badge to sleep forever)
    for i in range(0, len(scheduler)):
        timeUntilTaskExecution = scheduler[i]['target']-scheduler[i]['pos']
        
        global debugEnabled
        if debugEnabled:
            print("idle time for task "+str(i)+" = "+str(timeUntilTaskExecution)+" - ",scheduler[i])
            
        if not scheduler[i]["hfpm"]:
            if timeUntilTaskExecution<0:
                timeUntilTaskExecution = 0
            if timeUntilTaskExecution<idleTime:
                idleTime = timeUntilTaskExecution
    return idleTime

def pm_time():
    ''' Returns time until next pm task in ms '''
    global scheduler
    idleTime = 86400000 # One day (causes the badge to sleep forever)
    for i in range(0, len(scheduler)):
        if scheduler[i]["hfpm"]:
            timeUntilTaskExecution = scheduler[i]['target']-scheduler[i]['pos']
            if timeUntilTaskExecution<idleTime:
                idleTime = timeUntilTaskExecution
    return idleTime
    
    
def delete(callback):
    global scheduler
    newScheduler = []
    for task in scheduler:
        newScheduler.append(task)
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
            scheduler[i]["pos"] = 0
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
    newScheduler = []
    for task in scheduler:
        newScheduler.append(task)
    
    s = len(scheduler)
    for i in range(0, len(scheduler)):
        scheduler[i]["pos"] += period
        if scheduler[i]["pos"] > scheduler[i]["target"]:
            try:
                newTarget = scheduler[i]["cb"]()
            except BaseException as e:
                print("[ERROR] An error occured in a task. Task disabled.")
                sys.print_exception(e)
                newTarget = -1
            if newTarget > 0:
                newScheduler[i]["pos"] = 0
                newScheduler[i]["target"] = newTarget
            else:
                newScheduler.pop(i)
    scheduler = newScheduler
