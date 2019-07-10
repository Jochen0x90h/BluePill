# clone libopencm3 into this directory from https://github.com/libopencm3/libopencm3
OPENCM3_DIR = libopencm3
DEVICE = STM32F103C8
OBJS += main.o

CFLAGS += -Os -ggdb3
CPPFLAGS += -MD
LDFLAGS += -static -nostartfiles
LDLIBS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group

include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

.PHONY: clean all

all: binary.elf binary.bin

flash: binary.bin
	st-flash write binary.bin 0x8000000

clean:
	$(Q)$(RM) -rf binary.* *.o

include $(OPENCM3_DIR)/mk/genlink-rules.mk
include $(OPENCM3_DIR)/mk/gcc-rules.mk
