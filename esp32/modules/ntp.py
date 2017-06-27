### Description: Update the badge's time via NTP
### License: MIT

import database
import socket

NTP_DELTA = 2208988800
NTP_HOST = "pool.ntp.org"
NTP_PORT = 123

def get_NTP_time():
	NTP_QUERY = bytearray(48)
	NTP_QUERY[0] = 0x1b
	addr = socket.getaddrinfo(NTP_HOST, NTP_PORT)[0][-1]
	s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	s.sendto(NTP_QUERY, addr)

	# Setting timeout for receiving data. Because we're using UDP,
	# there's no need for a timeout on send.
	s.settimeout(2)
	msg = None
	try:
		msg = s.recv(48)
	except OSError:
		pass
	finally:
		s.close()

	if msg is None:
		return None

	import struct
	val = struct.unpack("!I", msg[40:44])[0]
	return val - NTP_DELTA

def set_NTP_time():
	import time
	from machine import RTC
	print("Setting time from NTP")

	t = get_NTP_time()
	if t is None:
		print("Could not set time from NTP")
		return False

	tz = 0
	with database.Database() as db:
		tz = db.get("timezone", 200) # default to CEST

	tz_minutes = int(abs(tz) % 100) * (1 if tz >= 0 else -1)
	tz_hours = int(tz / 100)
	t += (tz_hours * 3600) + (tz_minutes * 60)

	tm = time.localtime(t)
	tm = tm[0:3] + (0,) + tm[3:6] + (0,)

	rtc = RTC()
	rtc.init()
	rtc.datetime(tm)

	return True
