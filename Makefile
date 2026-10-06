TARGET      := lowest_number
BUILD       := build
SOURCES     := source
DATA        :=
INCLUDES    :=

ARCH        := -mthumb -mthumb-interwork -mcpu=arm9e -mtune=arm946e-s

CFLAGS      := -g -Wall -O2 -ffunction-sections -fdata-sections $(ARCH)
CFLAGS      += $(INCLUDE) -DARM9

CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS     := -g $(ARCH)

LDFLAGS     := -specs=ds_arm9.specs -g $(ARCH)
LDFLAGS     += -Wl,-Map,$(notdir $*.map)

LIBS        := -lnds9

LIBDIRS     := $(LIBNDS) $(PORTLIBS)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)

export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
OFILES := $(CFILES:.c=.o)

export INCLUDE := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all clean

all:
	$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean...
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).nds

else

DEPENDS := $(OFILES:.o=.d)

.PHONY: all

all: $(OUTPUT).nds

$(OUTPUT).nds: $(OUTPUT).elf
	@$(NDSTOOL) -c $@ -9 $<

$(OUTPUT).elf: $(OFILES)
	$(CC) $(LDFLAGS) $(LIBPATHS) $(OFILES) $(LIBS) -o $@

%.o: %.c
	@echo $(notdir $<)
	$(CC) $(CFLAGS) -MMD -MP -MF $*.d -c $< -o $@

-include $(DEPENDS)

endif

include $(DEVKITARM)/ds_rules
