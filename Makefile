#---------------------------------------------------------------------------------
# Basic devkitARM / libnds Makefile
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment")
endif

include $(DEVKITARM)/ds_rules

TARGET      := lowest_number
BUILD       := build
SOURCES     := source
DATA        := data
INCLUDES    := include

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
OFILES      := $(CFILES:.c=.o)
OFILES      := $(addprefix $(BUILD)/,$(OFILES))

ARCH        := -mthumb -mthumb-interwork -mcpu=arm9e -mtune=arm946e-s

CFLAGS      := -g -Wall -O2 $(ARCH)
CFLAGS      += -DARM9
CFLAGS      += -I$(DEVKITPRO)/libnds/include
CFLAGS      += -I$(DEVKITPRO)/libnds/include/calico

LDFLAGS     :=

.PHONY: all clean

all: $(TARGET).nds

$(TARGET).nds: $(TARGET).elf
	$(MAKE) -f $(DEVKITARM)/ds_rules $@

$(TARGET).elf: $(OFILES)
	$(CC) $(ARCH) $(OFILES) $(LDFLAGS) -o $@

$(BUILD)/%.o: $(SOURCES)/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).nds