import uos, machine, ubinascii, ujson, m5
import network, _thread, upip, time, gc
import umqtt
from micropython import const

BUTTON_A_PIN = const(39)
BUTTON_B_PIN = const(38)
BUTTON_C_PIN = const(37)
SPEAKER_PIN = const(25)

chipid = machine.unique_id()
# chipid_str = ubinascii.hexlify(chipid)
chipid_str = b'30aea449be0c'
mqttclient = umqtt.MQTTClient("umqtt_client", "mqtt.m5stack.com")

mqtt_topic_out = b'/M5Cloud/'+chipid_str+'/out'
mqtt_topic_in  = b'/M5Cloud/'+chipid_str+'/in'
mqtt_topic_repl_out = b'/M5Cloud/'+chipid_str+'/repl/out'
mqtt_topic_repl_in  = b'/M5Cloud/'+chipid_str+'/repl/in'
print('mqtt_topic_out:'+str(mqtt_topic_out))
print('mqtt_topic_in:'+str(mqtt_topic_in))
print('mqtt_topic_repl_out:'+str(mqtt_topic_repl_out))
print('mqtt_topic_repl_in:'+str(mqtt_topic_repl_in))


def connect_wifi():
  print("connect_wifi..\r\n")
  sta_if = network.WLAN(network.STA_IF); sta_if.active(True);
  # sta_if.scan()                             # Scan for available access points
  sta_if.connect("MasterHax_2.4G", "wittyercheese551") # Connect to an AP
  while (sta_if.isconnected() != True):                      # Check for successful connection
    time.sleep_ms(100)
  gc.collect()


# Received messages from subscriptions will be delivered to this callback
def mqtt_sub_cb(topic, msg):
  # print((topic, msg))
  if topic == mqtt_topic_in :
      jsondata = ujson.loads(msg)
      cmd = jsondata.get('cmd')
      if cmd == 'CMD_LISTDIR':
        try:
          path = jsondata.get('path')
          liststr = uos.listdir(path)
        except OSError: 
          resp_buf = {'status':405, 'data':'', 'msg':path}
          mqttclient.publish(mqtt_topic_out, ujson.dumps(resp_buf))
        else:
          return_data = {'type':'REP_LISTDIR', 'path': path, 'data': liststr}
          resp_buf = {'status':200, 'data':return_data, 'msg':''}
          mqttclient.publish(mqtt_topic_out, ujson.dumps(resp_buf))

      elif cmd == 'CMD_READ_FILE':
        try:
          path = jsondata.get('path')
          f = open(path, 'rb')
        except OSError: 
          resp_buf = {'status':404, 'data':'', 'msg':path}
          mqttclient.publish(mqtt_topic_out, ujson.dumps(resp_buf))
        else:
          return_data = {'type':'REP_READ_FILE', 'path': path, 'data': f.read()}
          resp_buf = {'status':200, 'data':return_data, 'msg':''}
          mqttclient.publish(mqtt_topic_out, ujson.dumps(resp_buf))

      elif cmd == 'CMD_WRITE_FILE':
        path = jsondata.get('path')
        # recvfile = jsondata.get('data')
        f = open(path, 'wb')
        f.write(jsondata.get('data'))
        f.close()
        return_data = {'type':'REP_WRITE_FILE', 'path': path, 'data':''}
        resp_buf = {'status':200, 'data':return_data, 'msg':''}
        mqttclient.publish(mqtt_topic_out, ujson.dumps(resp_buf))
  
  elif topic == mqtt_topic_repl_in :
    m5.termin(msg)


# def web_terminal_hanlde():

# Test reception e.g. with:
def mqtt_handle():
  print("connect_mqtt..\r\n")
  # mqttclient = MQTTClient("umqtt_client", server)
  mqttclient.set_callback(mqtt_sub_cb)
  mqttclient.connect()
  mqttclient.subscribe(mqtt_topic_in)
  mqttclient.subscribe(mqtt_topic_repl_in)
  # c.publish(b"foo_topic", str(uos.listdir()))
  # f=open('main.py', 'rb');
  # c.publish(mqtt_out, f.read())

  while True:
    if True:
      # Non-blocking wait for message
      mqttclient.check_msg();
      # c.wait_msg()
      rambuff = m5.termout_getch()
      if rambuff[0] != 0:
        # s = rambuff.decode('utf-8')
        mqttclient.publish(mqtt_topic_repl_out, rambuff)
      time.sleep_ms(50)
  mqttclient.disconnect()

def M5Cloud_handle(params, params2):
  connect_wifi()
  mqtt_handle()

def main():
  _thread.start_new_thread( M5Cloud_handle, ("M5Cloud_handle", 1))

# if __name__ == "__main__":
#   main()

main()
