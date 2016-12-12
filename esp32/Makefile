include ../py/mkenv.mk

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h

MICROPY_PY_USSL = 0
MICROPY_SSL_AXTLS = 0
MICROPY_FATFS = 1
MICROPY_PY_BTREE = 0

#FROZEN_DIR = scripts
#FROZEN_MPY_DIR = modules

# include py core make definitions
include ../py/py.mk

PORT ?= /dev/ttyUSB0
BAUD ?= 460800
FLASH_MODE ?= qio
FLASH_FREQ ?= 40m
FLASH_SIZE ?= 4MB
CROSS_COMPILE ?= xtensa-esp32-elf-

# paths to ESP IDF and its components
ifeq ($(ESPIDF),)
$(error Please configure the ESPIDF variable)
endif
ESPTOOL ?= $(ESPIDF)/components/esptool_py/esptool/esptool.py

INC += -I.
INC += -I..
INC += -I../stmhal
INC += -I../lib/mp-readline
INC += -I../lib/netutils
INC += -I../lib/timeutils
INC += -I$(BUILD)
INC += -I$(ESPIDF)/components/driver/include
INC += -I$(ESPIDF)/components/nghttp/port/include
INC += -I$(ESPIDF)/components/nghttp/include
INC += -I$(ESPIDF)/components/esp32/include
INC += -I$(ESPIDF)/components/ethernet/include
INC += -I$(ESPIDF)/components/expat/include/expat
INC += -I$(ESPIDF)/components/expat/port/include
INC += -I$(ESPIDF)/components/json/include
INC += -I$(ESPIDF)/components/json/port/include
INC += -I$(ESPIDF)/components/log/include
INC += -I$(ESPIDF)/components/newlib/include
INC += -I$(ESPIDF)/components/nvs_flash/include
INC += -I$(ESPIDF)/components/freertos/include
INC += -I$(ESPIDF)/components/freertos/include/freertos
INC += -I$(ESPIDF)/components/tcpip_adapter/include
INC += -I$(ESPIDF)/components/lwip/include/lwip
INC += -I$(ESPIDF)/components/lwip/include/lwip/port
INC += -I$(ESPIDF)/components/lwip/include/lwip/posix
INC += -I$(ESPIDF)/components/mbedtls/include
INC += -I$(ESPIDF)/components/mbedtls/port/include
INC += -I$(ESPIDF)/components/spi_flash/include
INC += -I$(ESPIDF)/components/vfs/include
INC += -I$(ESPIDF)/components/newlib/platform_include
INC += -I$(ESPIDF)/components/xtensa-debug-module/include
INC += -I$(ESPIDF)/components/wpa_supplicant/include
INC += -I$(ESPIDF)/components/wpa_supplicant/port/include

CFLAGS = -std=gnu99 -Os -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -mlongcalls -nostdlib -Wall -Werror -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=deprecated-declarations -DMBEDTLS_CONFIG_FILE='"mbedtls/esp_config.h"' -DHAVE_CONFIG_H -DESP_PLATFORM  $(INC)
# for lwip
CFLAGS += -Wno-address -Wno-unused-variable -Wno-unused-but-set-variable
# for wpa_supplicant
CFLAGS += -DEMBEDDED_SUPP -D__ets__ -Wno-strict-aliasing
# for expat
CFLAGS += -Wno-unused-function 

# this is what ESPIDF uses for c++ compilation
CXXFLAGS = -std=gnu++11 -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -mlongcalls -nostdlib -Wall -Werror -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=deprecated-declarations -DESP_PLATFORM $(INC)

LDFLAGS = -nostdlib -Map=$(@:.elf=.map) --cref
LDFLAGS += --gc-sections -static -EL
LDFLAGS += -u call_user_start_cpu0 -u uxTopUsedPriority
LDFLAGS += -L$(ESPIDF)/components/esp32/ld -T $(BUILD)/esp32_out.ld -T esp32.common.ld -T esp32.rom.ld -T esp32.peripherals.ld

LIBGCC_FILE_NAME = $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

LIBS =
LIBS += $(LDFLAGS_MOD)
LIBS += --start-group
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_DRIVER_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_ESP32_O))
LIBS += $(ESPIDF)/components/esp32/libhal.a
LIBS += -L$(ESPIDF)/components/esp32/lib -lcore -lnet80211 -lphy -lrtc -lpp -lwpa -lsmartconfig -lcoexist
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_EXPAT_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_FREERTOS_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_JSON_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_LOG_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_LWIP_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_MBEDTLS_O))
LIBS += $(ESPIDF)/components/newlib/lib/libc.a
LIBS += $(ESPIDF)/components/newlib/lib/libm.a
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_NEWLIB_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_NGHTTP_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_NVSFL_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_OPENSSL_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_SPIFL_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_TCPIP_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_VFS_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_WPASUP_O))
LIBS += $(addprefix $(BUILD)/, $(ESPIDF_XTDB_O))
LIBS += $(OBJ_MP)
LIBS += -L$(dir $(LIBGCC_FILE_NAME)) -lgcc
LIBS += --end-group

# Debugging/Optimization
ifeq ($(DEBUG), 1)
CFLAGS += -g
COPT = -O0
else
#CFLAGS += -fdata-sections -ffunction-sections
COPT += -Os -DNDEBUG
#LDFLAGS += --gc-sections
endif

SRC_C = \
	main.c \
	uart.c \
	gccollect.c \
	mphalport.c \
	fatfs_port.c \
	help.c \
	modutime.c \
	moduos.c \
	machine_pin.c \
	modmachine.c \
	$(SRC_MOD)

STM_SRC_C = $(addprefix stmhal/,\
	pybstdio.c \
	input.c \
	)

EXTMOD_SRC_C = $(addprefix extmod/,\
	)

LIB_SRC_C = $(addprefix lib/,\
	libm/math.c \
	libm/fmodf.c \
	libm/roundf.c \
	libm/ef_sqrt.c \
	libm/kf_rem_pio2.c \
	libm/kf_sin.c \
	libm/kf_cos.c \
	libm/kf_tan.c \
	libm/ef_rem_pio2.c \
	libm/sf_sin.c \
	libm/sf_cos.c \
	libm/sf_tan.c \
	libm/sf_frexp.c \
	libm/sf_modf.c \
	libm/sf_ldexp.c \
	libm/asinfacosf.c \
	libm/atanf.c \
	libm/atan2f.c \
	mp-readline/readline.c \
	netutils/netutils.c \
	timeutils/timeutils.c \
	utils/pyexec.c \
	utils/pyhelp.c \
	utils/interrupt_char.c \
	)

ifeq ($(MICROPY_FATFS), 1)
LIB_SRC_C += \
	lib/fatfs/ff.c \
	lib/fatfs/option/ccsbcs.c
endif

DRIVERS_SRC_C = $(addprefix drivers/,\
	)

ESPIDF_DRIVER_O = $(addprefix $(ESPIDF)/components/driver/,\
	uart.o \
	periph_ctrl.o \
	ledc.o \
	gpio.o \
	timer.o \
	)

$(BUILD)/$(ESPIDF)/components/esp32/%.o: CFLAGS = -std=gnu99 -Os -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -mlongcalls -nostdlib -Wall -Werror -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=deprecated-declarations -DMBEDTLS_CONFIG_FILE='"mbedtls/esp_config.h"' -DHAVE_CONFIG_H -DESP_PLATFORM  $(INC)
$(BUILD)/$(ESPIDF)/components/esp32/%.o: CXXFLAGS = -std=gnu++11 -fno-exceptions -fno-rtti -Og -ggdb -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -mlongcalls -nostdlib -Wall -Werror -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=deprecated-declarations -DESP_PLATFORM $(INC)
ESPIDF_ESP32_O = $(addprefix $(ESPIDF)/components/esp32/,\
	panic.o \
	event_default_handlers.o \
	task_wdt.o \
	cpu_start.o \
	gdbstub.o \
	crosscore_int.o \
	deepsleep.o \
	heap_alloc_caps.o \
	ipc.o \
	int_wdt.o \
	event_loop.o \
	cpu_freq.o \
	brownout.o \
	hwcrypto/sha.o \
	hwcrypto/aes.o \
	abi.o \
	lib_printf.o \
	freertos_hooks.o \
	cpu_util.o \
	system_api.o \
	hw_random.o \
	)

ESPIDF_EXPAT_O = $(addprefix $(ESPIDF)/components/expat/,\
	library/xmltok_ns.o \
	library/xmltok.o \
	library/xmlparse.o \
	library/xmlrole.o \
	library/xmltok_impl.o \
	port/minicheck.o \
	port/expat_element.o \
	port/chardata.o \
	)

ESPIDF_FREERTOS_O = $(addprefix $(ESPIDF)/components/freertos/,\
	ringbuf.o \
	heap_regions_debug.o \
	xtensa_intr.o \
	queue.o \
	croutine.o \
	list.o \
	xtensa_init.o \
	port.o \
	heap_regions.o \
	event_groups.o \
	tasks.o \
	timers.o \
	FreeRTOS-openocd.o \
	xtensa_overlay_os_hook.o \
	xtensa_intr_asm.o \
	xtensa_context.o \
	xtensa_vectors.o \
	portasm.o \
	)

ESPIDF_VFS_O = $(addprefix $(ESPIDF)/components/vfs/,\
	vfs_uart.o \
	vfs.o \
	)

ESPIDF_JSON_O = $(addprefix $(ESPIDF)/components/json/,\
	library/cJSON.o \
	port/cJSON_Utils.o \
	)

ESPIDF_LOG_O = $(addprefix $(ESPIDF)/components/log/,\
	log.o \
	)

ESPIDF_XTDB_O = $(addprefix $(ESPIDF)/components/xtensa-debug-module/,\
	eri.o \
	trax.o \
	)

ESPIDF_TCPIP_O = $(addprefix $(ESPIDF)/components/tcpip_adapter/,\
	tcpip_adapter_lwip.o \
	)

ESPIDF_NEWLIB_O = $(addprefix $(ESPIDF)/components/newlib/,\
	time.o \
	syscalls.o \
	syscall_table.o \
	reent_init.o \
	locks.o \
	)

ESPIDF_NGHTTP_O = $(addprefix $(ESPIDF)/components/nghttp/,\
	library/nghttp2_http.o \
	library/nghttp2_version.o \
	library/nghttp2_mem.o \
	library/nghttp2_hd_huffman.o \
	library/nghttp2_rcbuf.o \
	library/nghttp2_callbacks.o \
	library/nghttp2_session.o \
	library/nghttp2_stream.o \
	library/nghttp2_hd.o \
	library/nghttp2_priority_spec.o \
	library/nghttp2_buf.o \
	library/nghttp2_option.o \
	library/nghttp2_npn.o \
	library/nghttp2_helper.o \
	library/nghttp2_frame.o \
	library/nghttp2_outbound_item.o \
	library/nghttp2_hd_huffman_data.o \
	library/nghttp2_pq.o \
	library/nghttp2_queue.o \
	library/nghttp2_submit.o \
	library/nghttp2_map.o \
	port/http_parser.o \
	)

ESPIDF_NVSFL_O = $(addprefix $(ESPIDF)/components/nvs_flash/,\
	src/nvs_types.o \
	src/nvs_page.o \
	src/nvs_item_hash_list.o \
	src/nvs_pagemanager.o \
	src/nvs_storage.o \
	src/nvs_api.o \
	)

ESPIDF_OPENSSL_O = $(addprefix $(ESPIDF)/components/openssl/,\
	)

ESPIDF_SPIFL_O = $(addprefix $(ESPIDF)/components/spi_flash/,\
	flash_mmap.o \
	partition.o \
	spi_flash_rom_patch.o \
	cache_utils.o \
	flash_ops.o \
	)

ESPIDF_LWIP_O = $(addprefix $(ESPIDF)/components/lwip/,\
	api/pppapi.o \
	api/netbuf.o \
	api/api_lib.o \
	api/netifapi.o \
	api/tcpip.o \
	api/netdb.o \
	api/err.o \
	api/api_msg.o \
	api/sockets.o \
	api/lwip_debug.o \
	apps/sntp/sntp.o \
	apps/dhcpserver.o \
	core/ipv4/ip_frag.o \
	core/ipv4/dhcp.o \
	core/ipv4/ip4_addr.o \
	core/ipv4/igmp.o \
	core/ipv4/ip4.o \
	core/ipv4/autoip.o \
	core/ipv4/icmp.o \
	core/ipv6/ip6_frag.o \
	core/ipv6/dhcp6.o \
	core/ipv6/inet6.o \
	core/ipv6/ip6_addr.o \
	core/ipv6/ip6.o \
	core/ipv6/nd6.o \
	core/ipv6/mld6.o \
	core/ipv6/ethip6.o \
	core/ipv6/icmp6.o \
	core/mem.o \
	core/init.o \
	core/memp.o \
	core/sys.o \
	core/tcp_in.o \
	core/dns.o \
	core/ip.o \
	core/pbuf.o \
	core/raw.o \
	core/tcp.o \
	core/def.o \
	core/netif.o \
	core/stats.o \
	core/timers.o \
	core/inet_chksum.o \
	core/udp.o \
	core/tcp_out.o \
	netif/slipif.o \
	netif/etharp.o \
	netif/ethernet.o \
	netif/lowpan6.o \
	netif/ethernetif.o \
	port/freertos/sys_arch.o \
	port/netif/wlanif.o \
	)

ESPIDF_MBEDTLS_O = $(addprefix $(ESPIDF)/components/mbedtls/,\
	library/entropy.o \
	library/net.o \
	library/pkcs12.o \
	library/ccm.o \
	library/pk.o \
	library/sha1.o \
	library/x509_csr.o \
	library/ssl_cli.o \
	library/ecp.o \
	library/blowfish.o \
	library/x509.o \
	library/ecp_curves.o \
	library/error.o \
	library/ssl_ticket.o \
	library/entropy_poll.o \
	library/cipher.o \
	library/version_features.o \
	library/ripemd160.o \
	library/rsa.o \
	library/md.o \
	library/md_wrap.o \
	library/sha256.o \
	library/dhm.o \
	library/ssl_cache.o \
	library/pkwrite.o \
	library/base64.o \
	library/asn1parse.o \
	library/ssl_tls.o \
	library/hmac_drbg.o \
	library/pem.o \
	library/version.o \
	library/gcm.o \
	library/memory_buffer_alloc.o \
	library/md2.o \
	library/ecdsa.o \
	library/ssl_srv.o \
	library/x509_crt.o \
	library/ecdh.o \
	library/asn1write.o \
	library/md4.o \
	library/debug.o \
	library/x509_create.o \
	library/ecjpake.o \
	library/oid.o \
	library/md5.o \
	library/ssl_ciphersuites.o \
	library/sha512.o \
	library/xtea.o \
	library/aes.o \
	library/cipher_wrap.o \
	library/arc4.o \
	library/bignum.o \
	library/pkparse.o \
	library/padlock.o \
	library/threading.o \
	library/x509_crl.o \
	library/pkcs11.o \
	library/aesni.o \
	library/timing.o \
	library/certs.o \
	library/pkcs5.o \
	library/ssl_cookie.o \
	library/camellia.o \
	library/havege.o \
	library/des.o \
	library/x509write_csr.o \
	library/platform.o \
	library/ctr_drbg.o \
	library/x509write_crt.o \
	library/pk_wrap.o \
	port/net.o \
	port/esp_bignum.o \
	port/esp_hardware.o \
	)

ESPIDF_WPASUP_O = $(addprefix $(ESPIDF)/components/wpa_supplicant/,\
	src/crypto/aes-internal-enc.o \
	src/crypto/sha256-internal.o \
	src/crypto/md5-internal.o \
	src/crypto/aes-internal.o \
	src/crypto/sha1.o \
	src/crypto/aes-internal-dec.o \
	src/crypto/aes-unwrap.o \
	src/crypto/crypto_internal-rsa.o \
	src/crypto/dh_groups.o \
	src/crypto/crypto_internal.o \
	src/crypto/aes-wrap.o \
	src/crypto/sha1-internal.o \
	src/crypto/dh_group5.o \
	src/crypto/sha256.o \
	src/crypto/rc4.o \
	src/crypto/md5.o \
	src/crypto/aes-cbc.o \
	src/crypto/sha1-pbkdf2.o \
	src/crypto/bignum.o \
	src/crypto/crypto_internal-modexp.o \
	src/crypto/crypto_internal-cipher.o \
	)

OBJ_MP =
OBJ_MP += $(PY_O)
OBJ_MP += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ_MP += $(addprefix $(BUILD)/, $(STM_SRC_C:.c=.o))
OBJ_MP += $(addprefix $(BUILD)/, $(EXTMOD_SRC_C:.c=.o))
OBJ_MP += $(addprefix $(BUILD)/, $(LIB_SRC_C:.c=.o))
OBJ_MP += $(addprefix $(BUILD)/, $(DRIVERS_SRC_C:.c=.o))

OBJ_ESPIDF =
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_NEWLIB_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_DRIVER_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_ESP32_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_EXPAT_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_FREERTOS_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_VFS_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_JSON_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_LOG_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_LWIP_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_MBEDTLS_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_XTDB_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_TCPIP_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_NGHTTP_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_NVSFL_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_OPENSSL_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_SPIFL_O))
OBJ_ESPIDF += $(addprefix $(BUILD)/, $(ESPIDF_WPASUP_O))

OBJ = $(OBJ_MP) $(OBJ_ESPIDF)

# List of sources for qstr extraction
SRC_QSTR += $(SRC_C) $(STM_SRC_C) $(EXTMOD_SRC_C) $(DRIVERS_SRC_C)
# Append any auto-generated sources that are needed by sources listed in SRC_QSTR
SRC_QSTR_AUTO_DEPS +=

all: $(BUILD)/firmware.bin

.PHONY: idf-version deploy

idf-version:
	$(ECHO) "IDF version: c8685c200276abda4418f1367743f650d2aeacb2"

deploy: $(BUILD)/firmware.bin
	$(ECHO) "Writing $< to the board"
	$(Q)$(ESPTOOL) --chip esp32 --port $(PORT) --baud $(BAUD) write_flash -z --flash_mode $(FLASH_MODE) --flash_freq $(FLASH_FREQ) --flash_size $(FLASH_SIZE) 0x1000 $(ESPIDF)/examples/01_hello_world/build/bootloader/bootloader.bin 0x8000 $(ESPIDF)/examples/01_hello_world/build/partitions_singleapp.bin 0x10000 $<

$(BUILD)/esp32_out.ld: sdkconfig.h
	$(Q)$(CC) -I. -C -P -x c -E $(ESPIDF)/components/esp32/ld/esp32.ld -o $@

$(BUILD)/firmware.bin: $(BUILD)/firmware.elf
	$(ECHO) "Create $@"
	$(Q)$(ESPTOOL) --chip esp32 elf2image --flash_mode $(FLASH_MODE) --flash_freq $(FLASH_FREQ) --flash_size $(FLASH_SIZE) $<

$(BUILD)/firmware.elf: $(OBJ) $(BUILD)/esp32_out.ld
	$(ECHO) "LINK $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $(LIBS)
	$(Q)$(SIZE) $@

define compile_cxx
$(ECHO) "CXX $<"
$(Q)$(CXX) $(CXXFLAGS) -c -MD -o $@ $<
@# The following fixes the dependency file.
@# See http://make.paulandlesley.org/autodep.html for details.
@# Regex adjusted from the above to play better with Windows paths, etc.
@$(CP) $(@:.o=.d) $(@:.o=.P); \
  $(SED) -e 's/#.*//' -e 's/^.*:  *//' -e 's/ *\\$$//' \
      -e '/^$$/ d' -e 's/$$/ :/' < $(@:.o=.d) >> $(@:.o=.P); \
  $(RM) -f $(@:.o=.d)
endef

vpath %.cpp . $(TOP)
$(BUILD)/%.o: %.cpp
	$(call compile_cxx)

include ../py/mkrules.mk
