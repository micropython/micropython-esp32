import esp, deepsleep

esp.rtcmem_write(0,1)
esp.rtcmem_write(1,~1)
deepsleep.reboot()
