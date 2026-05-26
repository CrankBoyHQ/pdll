HEAP_SIZE      = 8388208
STACK_SIZE     = 61800

PRODUCT = TestHost.pdx

SDK = ${PLAYDATE_SDK_PATH}
ifeq ($(SDK),)
	SDK = $(shell egrep '^\s*SDKRoot' ~/.Playdate/config | head -n 1 | cut -c9-)
endif

ifeq ($(SDK),)
$(error SDK path not found; set ENV value PLAYDATE_SDK_PATH)
endif

SRC = main.c ../../uzlib/tinflate.c ../../uzlib/tinfzlib.c \
      ../../uzlib/adler32.c ../../uzlib/crc32.c

UINCDIR = ../..

UDEFS =
UADEFS =
ULIBDIR =
ULIBS =

include $(SDK)/C_API/buildsupport/common.mk
