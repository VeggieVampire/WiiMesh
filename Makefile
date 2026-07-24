# WiiMesh - Meshtastic client for Nintendo Wii

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=/opt/devkitpro/devkitPPC")
endif

include $(DEVKITPPC)/wii_rules
export LD := $(CXX)

TARGET      := boot
BUILD       := build
SOURCES     := src src/transport src/meshtastic src/ui src/storage
INCLUDES    := include
DATA        :=
LIBS        := -lwiiuse -lbte -lfat -logc
LIBDIRS     :=
HOST_CXX    ?= g++

CXXFLAGS    := -g -O2 -Wall -Wextra -ffunction-sections -fdata-sections \
               -DWIIMESH_WII=1 $(MACHDEP)
CFLAGS      := $(CXXFLAGS)
LDFLAGS     := -g $(MACHDEP) -Wl,--gc-sections

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                 $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES    := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(LIBOGC_INC) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(LIBOGC_LIB)
export CPPFLAGS := $(INCLUDE)

.PHONY: all clean run host-test

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).dol

host-test:
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	$(HOST_CXX) -std=c++17 -DWIIMESH_HOST_TEST=1 -Iinclude \
		src/meshtastic/ProtoReader.cpp src/meshtastic/MeshtasticProtocol.cpp \
		src/transport/MockTransport.cpp tests/protocol_mock_test.cpp \
		-o $(BUILD)/protocol_mock_test
	$(BUILD)/protocol_mock_test

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
