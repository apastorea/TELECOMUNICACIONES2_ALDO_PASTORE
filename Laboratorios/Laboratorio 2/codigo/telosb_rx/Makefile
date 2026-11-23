APPLICATION = telosb_rx
BOARD ?= telosb
RIOTBASE ?= $(CURDIR)/../..

INCLUDES += -I$(APPDIR)

USEMODULE += cc2420
USEMODULE += test_utils_netdev_ieee802154_minimal

CFLAGS += -DEVENT_THREAD_STACKSIZE_DEFAULT=1024

NODE_ID ?= 2
CFLAGS += -DNODE_ID=$(NODE_ID)

include $(RIOTBASE)/Makefile.include
