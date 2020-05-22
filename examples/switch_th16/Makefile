PROGRAM = sth

EXTRA_COMPONENTS = \
  extras/onewire \
	extras/ds18b20 \
	extras/http-parser \
	extras/dhcpserver \
	$(abspath ../../components/esp8266-open-rtos/wifi_config) \
	$(abspath ../../components/esp8266-open-rtos/cJSON) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit)

FLASH_SIZE ?= 8
FLASH_MODE ?= dout
FLASH_SPEED ?= 40
HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0x7A000
HOMEKIT_MAX_CLIENTS = 16
HOMEKIT_SMALL = 0
HOMEKIT_OVERCLOCK = 1
HOMEKIT_OVERCLOCK_PAIR_SETUP = 1
HOMEKIT_OVERCLOCK_PAIR_VERIFY = 0

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
