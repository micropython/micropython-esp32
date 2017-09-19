
### Abbreviations

* GATT: Generic Attribute Profile
* GAP: Generic Access Profile (advertising, mostly)
* GATTS: GATT Server
* GATTC: GATT Client


### Brief introduction:

**GATTS** is a BLE device like a heart rate monitor.  It can have a single connection from a GATTC device.
**GATTS** is a BLE "central device" (e.g. a phone, tablet, or computer).
**GAP** is used for GATTS devices to advertise their presence.

## General

### Bluetooth objects

The Bluetooth object is a global singleton.

```python
import network
bluetooth = network.Bluetooth()
```


Note that these defaults are set when you create the Bluetooth object.  You needn't call `ble_settings` unless you need to change one of these.

```python
bluetooth.ble_settings(int_min = 1280, int_max = 1280,
    adv_type = bluetooth.ADV_TYPE_IND,
    own_addr_type = bluetooth.BLE_ADDR_TYPE_PUBLIC,
    peer_addr = bytes([0] * 6),
    peer_addr_type = bluetooth.BLE_ADDR_TYPE_PUBLIC,
    channel_map = bluetooth.ADV_CHNL_ALL,
    filter_policy = blueooth.ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    adv_is_scan_rsp = False,
    adv_dev_name = None,
    adv_man_name = None,
    adv_inc_tx_power = False,
    adv_int_min = 1280,
    adv_int_max = 1280,
    adv_appearance = 0,
    adv_uuid = None,
    adv_flags = 0)
```

`adv_type` is one of:

 * `bluetooth.ADV_TYPE_IND`
 * `bluetooth.ADV_TYPE_DIRECT_IND_HIGH`
 * `bluetooth.ADV_TYPE_SCAN_IND`
 * `bluetooth.ADV_TYPE_NONCONN_IND`
 * `bluetooth.ADV_TYPE_DIRECT_IND_LOW`

`own_addr_type` and `peer_addr_type` is one of:
* `bluetooth.BLE_ADDR_TYPE_PUBLIC`
* `bluetooth.BLE_ADDR_TYPE_RANDOM`
* `bluetooth.BLE_ADDR_TYPE_RPA_PUBLIC`
* `bluetooth.BLE_ADDR_TYPE_RPA_RANDOM`

`peer_addr` is a `bytes(6)` or `bytearray(6)` representing the peer address.

`channel_map` is a binary OR of:

* `bluetooth.ADV_CHNL_37`
* `bluetooth.ADV_CHNL_38`
* `bluetooth.ADV_CHNL_39`
* `bluetooth.ADV_CHNL_ALL`

`filter_policy` is one of:

* `bluetooth.ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY`
* `bluetooth.ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY`
* `bluetooth.ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST`
* `bluetooth.ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST`

`adv_is_scan_rsp` indicates in the advertising information is a scan response.

`adv_dev_name` is the advertised device name.

`adv_man_name` is the advertised manufacturer name.

`adv_inc_tx_power` to include the transmission power in the advertisement.

`adv_int_min` and `adv_int_max` for the advertised intervals, in milliseconds.

`adv_appearance` is an integer representing the appearance of the device.

`adv_uuid` the advertising uuid.

`adv_flags` is a binary or of:

* `bluetooth.BLE_ADV_FLAG_LIMIT_DISC`
* `bluetooth.BLE_ADV_FLAG_GEN_DISC`
* `bluetooth.BLE_ADV_FLAG_BREDR_NOT_SPT`
* `bluetooth.BLE_ADV_FLAG_DMT_CONTROLLER_SPT`
* `bluetooth.BLE_ADV_FLAG_DMT_HOST_SPT`
* `bluetooth.BLE_ADV_FLAG_NON_LIMIT_DISC`

`bluetooth.init()` enable the bluetooth subsystem.  The first call to the Bluetooth constructor will call this.  You only need to call `init()` if you've called `deinit()`.

`bluetooth.deinit()` shutdown Bluetooth.  Due to present limitations in the IDF, this does _not_ return the BT stack to a lower powered state.

`bluetooth.connect(bda)` GATTC - connect to a remote GATTS server.  BDA is the remote address, as a `bytes(6)`.  Returns a [GATTCConn](#gattcconn-objects) object.

`bluetooth.Service(uuid, is_primary = True)` GATTS - create a new GATTSService object. `uuid` is either an integer or a `bytes(16)`. UUIDs are globally unique with in GATTS.  If you attempt to create a service with a UUID that is the same as an existing (but not closed) service, you will receive the same service object, and no new service will be created.

`bluetooth.services()` GATTS - returns the existing GATTS services.

`bluetooth.conns()` GATTC - Returns all the current client connections.

`bluetooth.callback(<callback>, <callback_data>)` used to set the callback function for bluetooth-object-level callbacks.  `<callback>` can be set to None.  If `<callback_data>` is not specified, it will be set to None.  Always returns a 2-tuple of the present `<callback>` and `<callback_data>`.  If called with no parameters, the values remain unchanged.  `<callback>` will be called with 4 parameters:

1. The `bluetooth` object
2. The event, which is one of:
    * `bluetooth.CONNECT` when a GATTS or GATTC connection occurs
    * `bluetooth.DISCONNECT` when a GATTS or GATTC disconnect occurs
    * `bluetooth.SCAN_RES` for GATTC scan results
    * `bluetooth.SCAN_CMPL` when GATTC scan is complete
3. Event data:
    * For `bluetooth.CONNECT`, `bluetooth.DISCONNECT` events, this will be a `bytes(6)` represeting the remote address.
    * For `bluetooth.SCAN_RES` this will be a 3-tuple of `(<remote BDA>, <advertised name>, <rssi>)`
    * For `bluetooth.SCAN_CMPL` this will be `None`.
4. The `<callback_data` parameter from the `bluetooth.callback()` call.

`bluetooth.scan_start(timeout, scan_type = bluetooth.SCAN_TYPE_ACTIVE, own_addr_type = bluetooth.BLE_ADDR_TYPE_PUBLIC, scan_filter_policy = SCAN_FILTER_ALLOW_ALL, scan_interval = 0x50, scan_window = 0x30)` GATTC - begin scanning for hosts.

`timeout` is in seconds.

`scan_type` is one of:
* `bluetooth.SCAN_TYPE_ACTIVE`
* `bluetooth.SCAN_TYPE_PASSIVE`

`own_addr_type` is one of:
* `bluetooth.BLE_ADDR_TYPE_PUBLIC`
* `bluetooth.BLE_ADDR_TYPE_RANDOM`
* `bluetooth.BLE_ADDR_TYPE_RPA_PUBLIC`
* `bluetooth.BLE_ADDR_TYPE_RPA_RANDOM`

`scan_filter_policy` is one of:
* `bluetooth.SCAN_FILTER_ALLOW_ALL`
* `bluetooth.SCAN_FILTER_ALLOW_ONLY_WLST`
* `bluetooth.SCAN_FILTER_ALLOW_UND_RPA_DIR`
* `bluetooth.SCAN_FILTER_ALLOW_WLIST_PRA_DIR`



`bluetooth.scan_stop()` GATTC - terminate scanning early.  If called before the scan timeout, you will _not_ receive a `bluetooth.SCAN_CMPL` event.

`bluetooth.is_scanning()` GATTC - returns `True` if the scan is still active

## GAP

### Start and stop advertising

`bluetooth.ble_adv_enable(True)`

`bluetooth.ble_adv_enable(False)`

When a GATTC connects, advertising ends.  After disconnect, then advertising must be restarted.

## GATTS

### GATTSService objects

GATTSService objects are created by calling the `bluetooth.Service()` constructor.


`service.Char(uuid, value = None, perm = bluetooth.PERM_READ | bluetooth.PERM_WRITE, prop = bluetooth.PROP_READ | bluetooth.PROP_WRITE | bluetooth.PROPR_NOTIFY)` Create a new characteristic for this service. Characteristic UUIDS are unique for a particular service, and an attempt to create another characteristic with the same UUID will simply return the existing `GATTSChar` object.

`perm` is a binary OR of the following:

* `bluetooth.PERM_READ`
* `bluetooth.PERM_READ_ENCRYPTED`
* `bluetooth.PERM_READ_ENC_MITM`
* `bluetooth.PERM_WRITE`
* `bluetooth.PERM_WRITE_ENCRYPTED`
* `bluetooth.PERM_WRITE_ENC_MITM`
* `bluetooth.PERM_WRITE_SIGNED`
* `bluetooth.PERM_WRITE_SIGNED_MITM`

`prop` is a binary OR of the following:

* `bluetooth.PROP_BROADCAST`
* `bluetooth.PROP_READ`
* `bluetooth.PROP_WRITE_NR`
* `bluetooth.PROP_WRITE`
* `bluetooth.PROP_NOTIFY`
* `bluetooth.PROP_INDICATE`
* `bluetooth.PROP_AUTH`
* `bluetooth.PROP_EXT_PROP`

`service.chars()` Get the characteristics attached to this service.

`service.is_primary()` Get the value of the srvice `primary` flag.

`char.uuid()` Get the service UUID.

`service.start()` Start the service; it will be visible to any connecting GATTC.

`service.stop()` Stop the service.

`service.close()` Delete this service from the GATTS.  You can no longer use this service object.

### GATTSChar objects

GATTSChar objects are created by calling the `service.Char()` constructor, and they are associated with that service.

`char.callback(<callback>, <callback_data>)` Sets the callback function for this service.  It operates the same way as the `bluetooth.callback()` function. `<callback>` will be called with 4 parameters:

1. The `GATTSChar` object
2. The event that occured, which is one of:
    * `bluetooth.READ` when a GATTC issues a read command for that characteristic.
    * `bluetooth.WRITE` when a GATTC issues a write command for that characteristic.
3. The value.  For `bluetooth.READ`, this is the current stored value of the characteristic.  For `bluetooth.WRITE` this is the value written by the GATTC.
4. the `<callback_data>`

For `bluetooth.READ` events, the return value of the callback is what is returned to the GATTC.  When there is a callback for a characteristic, the characteristic value is not modified or otherwise used.  It's up to the callback to do whatever is necessary with the write data, or to return the proper data for a read.

In the absence of a callback, then the characteristic value is return for a read, and it is updated for a write.

`char.indicate(<value>)` Send an indicate value.  `<value>` is `string`, `bytearray`, `bytes` or `None`.

`char.notify(<value>)` Send a notify value.  `<value>` is `string`, `bytearray`, `bytes` or `None`.

`char.uuid()` Get the characteristic UUID.

`char.value([value])` Get or set the characteristic value.

`char.service()` Get the parent service.

`char.Descr(uuid, value = None, perm = bluetooth.PERM_READ | bluetooth.PERM_WRITE)` Create a new descriptor for a characteristic.  

`char.descrs` Get the descriptors associated with the characteristic

### GATTSDescr objects

`descr.uuid()` Get the descriptor UUID.

`descr.value([value])` Get or set the value.

`descr.char()` Get the parent characteristic.

`descr.callback(<callback>, <callback_data>)` See `char.callback()`, above.

## GATTC

Use `bluetooth.scan_start()`, to find GATTS devices.  You'll need to set up a Bluetooth object callback to get scan results. You can then use `bluetooth.connect()` to connect to a GATTS device:

`bluetooth.connect(<bda>)` Returns a `GATTCConn` object.

### GATTCConn objects

`conn.services()` Returns the services associated with the connection.  This is a list of [`GATTCService`](#gattcservice-objects) objects.

`conn.is_connected()` Returns whether the connection is active or not.

`conn.disconnect()` Disconnect

### GATTCService objects

`service.is_primary()` Returns a boolean indicating if the service is a primary service or not.

`service.uuid()` Returns the service UUID

`service.chars()` Returns a list of [`GATTCChar`](#gattcchar-objects) objects associated with this service

### GATTCChar objects

`char.callback(<callback>, <callback_data)` This works the same as the Bluetooth and ['GATTSChar`](#gattschar-objects) callback functions. The purpose of the GATTCChar callback is to receive notifications and indications from the connected GATTS device.  Note that just setting a callback is not usually enough to receive callbacks; often, the [`GATTCDescr`](#gattcdescr-objects) associated with the `GATTCChar` object must be written to to enable notifications/indications.

When the callback is called, it will be called with 4 parameters:

1. The `GATTCChar` objects
2. The event that occured, which is:
    * `bluetooth.NOTIFY` when a GATTS issues a notification for the characteristic
3. The value of the notify/indicate
4. the `<callback_data>`

`char.descrs()` Returns a list of [`GATTCDescr`](#gattcdescr-objects) associated with this characteristic.

`char.read()` Read the characteristic value

`char.write(<value>)` Write a value to the characteristic.  `<value>` can be `str`, `bytearray`, `bytes`, or `None`

### GATTCDescr objects

`GATTCChar` objects often have associated BLE desriptor objects.  These can be obtained by calling `char.descrs()` function of [`GATTCChar`](#gattcchar-objects) objects.

`descr.read()` Read from the descriptor.

`descr.write(<value>)` Write a value to the characteristic.  `<value>` can be `str`, `bytearray`, `bytes`, or `None`

`descr.uuid()` Get the descriptor UUID.

`descr.char()` Get the [`GATTCChar`](#gattcchar-objects) the descriptor is attached to

## Examples

### Heart Rate and GATTS

In the example below, this script is meant to be installed as your `boot.py`.  

From the µPy prompt, you can enter `gatts()` to make the ESP32 a gatts server that can then be connected to by a smartphone or PC.

Alternatively, the `hr(<bda>)` function will connect to a heart rate monitor at address `<bda>`, and configure it to send heartrate notifications; the callback function will then print the heart rate.

```python

import gc
import sys
import network as n
import gc
import time

b = n.Bluetooth()

found = {}
complete = True

def bcb(b,e,d,u):
    global complete
    global found
    if e == b.CONNECT:
	print("CONNECT")
    elif e == b.DISCONNECT:
	print("DISCONNECT")
    elif e == b.SCAN_RES:
        if complete:
            complete = False
            found = {}

        adx, name, rssi = d
        if adx not in found:
            found[adx] = name

    elif e == b.SCAN_CMPL:
	print("Scan Complete")
        complete = True
        print ('\nFinal List:')
        for adx, name in found.items():
            print ('Found:' + ':'.join(['%02X' % i for i in adx]), name)
    else:
        print ('Unknown event', e,d)

def cb (cb, event, value, userdata):
    print('charcb ', cb, userdata, ' ', end='')
    if event == b.READ:
        print('Read')
        return 'ABCDEFG'
    elif event == b.WRITE:
        print ('Write', value)
    elif event == b.NOTIFY:
        print ('Notify', value)
        period = None
        flags = value[0]
        hr = value[1]
        if flags & 0x10:
            period = (value[3] << 8) + value[2]
        print ('HR:', hr, 'Period:', period, 'ms')

def hr(bda):
    ''' Will connect to a BLE heartrate monitor, and enable HR notifications '''

    conn = b.connect(bda)
    while not conn.is_connected():
        time.sleep(.1)

    print ('Connected')

    time.sleep(2) # Wait for services

    service = ([s for s in conn.services() if s.uuid()[0:4] == b'\x00\x00\x18\x0d'] + [None])[0]
    if service:
        char = ([c for c in service.chars() if c.uuid()[0:4] == b'\x00\x00\x2a\x37'] + [None])[0]
        if char:
            descr = ([d for d in char.descrs() if d.uuid()[0:4] == b'\x00\x00\x29\x02'] + [None])[0]
            if descr:
                char.callback(cb)
                descr.write(b'\x01\x00') # Turn on notify

    return conn


def gatts():
    s1 = b.Service(0xaabb)
    s2 = b.Service(0xDEAD)

    c1 = s1.Char(0xccdd)
    c2 = s2.Char(0xccdd)

    c1.callback(cb, 'c1 data')
    c2.callback(cb, 'c2 data')

    s1.start()
    s2.start()

    b.ble_settings(adv_man_name = "mangocorp", adv_dev_name="mangoprod")
    b.ble_adv_enable(True)


b.callback(bcb)
```
