import esp

esp.rtcmem_write(0,1)
esp.rtcmem_write(1,~1)
esp.start_sleeping(1)
