# The name of your project (used to name the compiled .hex file)
TARGET = $(notdir $(CURDIR))

# The teensy version to use, 30, 31, or LC
TEENSY = 31

# Set to 24000000, 48000000, or 96000000 to set CPU core speed
TEENSY_CORE_SPEED = 48000000

# Some libraries will require this to be defined
# If you define this, you will break the default main.cpp
#ARDUINO = 10600

# configurable options
OPTIONS = -DUSB_SERIAL -DLAYOUT_US_ENGLISH

# directory to build in
BUILDDIR = $(abspath $(CURDIR)/build)

#************************************************************************
# Location of Teensyduino utilities, Toolchain, and Arduino Libraries.
# To use this makefile without Arduino, copy the resources from these
# locations and edit the pathnames.  The rest of Arduino is not needed.
#************************************************************************

# path location for Teensy Loader, teensy_post_compile and teensy_reboot
ARDUINOPATH = $(CURDIR)

ifeq ($(OS),Windows_NT)
    $(error What is Win Dose?)
endif

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    ARDUINOPATH := /usr/share/arduino
else ifeq ($(UNAME_S),Darwin)
    ARDUINOPATH := /Applications/Arduino.app/Contents/Java
endif
TOOLSPATH := $(ARDUINOPATH)/hardware/tools

# path location for Teensy 3 core
COREPATH = $(CURDIR)/src/fs-0-core/core

# path location for Arduino libraries
LIBRARYPATH = $(CURDIR)/src/fs-0-core/libs -I$(CURDIR)/src/libs

# path location for the arm-none-eabi compiler
COMPILERPATH = $(TOOLSPATH)/arm/bin

#************************************************************************
# Settings below this point usually do not need to be edited
#************************************************************************

# CPPFLAGS = compiler options for C and C++
CPPFLAGS = -Wall -Os -s -mthumb -ffunction-sections -fdata-sections -nostdlib -MMD $(OPTIONS) -DTEENSYDUINO=128 -DF_CPU=$(TEENSY_CORE_SPEED) -I$(COREPATH) -I$(LIBRARYPATH)

# compiler options for C++ only
CXXFLAGS = -std=c++11 -felide-constructors -fno-exceptions -fno-rtti -flto

# compiler options for C only
CFLAGS =

# linker options
LDFLAGS := -Os -Wl,--gc-sections,--defsym=__rtc_localtime=0 -mthumb -flto

# additional libraries to link
LIBS := -lm

# compiler options specific to teensy version
ifeq ($(TEENSY), 30)
    CPPFLAGS += -D__MK20DX128__ -mcpu=cortex-m4
    LDSCRIPT := $(COREPATH)/mk20dx128.ld
    LDFLAGS += -mcpu=cortex-m4 -T$(LDSCRIPT)
else
    ifeq ($(TEENSY), 31)
        CPPFLAGS += -D__MK20DX256__ -mcpu=cortex-m4
        LDSCRIPT = $(COREPATH)/mk20dx256.ld
        LDFLAGS += -mcpu=cortex-m4 -T$(LDSCRIPT)
    else
        ifeq ($(TEENSY), LC)
            CPPFLAGS += -D__MKL26Z64__ -mcpu=cortex-m0plus
            LDSCRIPT = $(COREPATH)/mkl26z64.ld
            LDFLAGS += -mcpu=cortex-m0plus -T$(LDSCRIPT)
            LIBS += -larm_cortexM0l_math
        else
            $(error Invalid setting for TEENSY)
        endif
    endif
endif

CPPFLAGS += -DUSING_MAKEFILE

# names for the compiler programs
CC := $(abspath $(COMPILERPATH))/arm-none-eabi-gcc
CXX := $(abspath $(COMPILERPATH))/arm-none-eabi-g++
OBJCOPY := $(abspath $(COMPILERPATH))/arm-none-eabi-objcopy
SIZE := $(abspath $(COMPILERPATH))/arm-none-eabi-size

# Make does not offer a recursive wildcard function, so here's one:
rwildcard=$(wildcard $1$2) $(foreach dir,$(wildcard $1*),$(call rwildcard,$(dir)/,$2))

# automatically create lists of the sources and objects
C_FILES := $(call rwildcard,src/,*.c)
CPP_FILES := $(call rwildcard,src/,*.cpp)
INO_FILES := $(call rwildcard,src/,*.ino)

SOURCES := $(C_FILES:.c=.o) $(CPP_FILES:.cpp=.o) $(INO_FILES:.ino=.o) $(TC_FILES:.c=.o) $(TCPP_FILES:.cpp=.o) $(LC_FILES:.c=.o) $(LCPP_FILES:.cpp=.o)
OBJS := $(foreach src,$(SOURCES), $(BUILDDIR)/$(src))

.PHONY: all
all: hex

.PHONY: build
build: $(TARGET).elf

hex: $(TARGET).hex

.PHONY: post_compile
post_compile: $(TARGET).hex
	@$(abspath $(TOOLSPATH))/teensy_post_compile -file="$(basename $<)" -path=$(CURDIR) -tools="$(abspath $(TOOLSPATH))"

.PHONY: reboot
reboot:
	@-$(abspath $(TOOLSPATH))/teensy_reboot

.PHONY: upload
upload: post_compile reboot

$(BUILDDIR)/%.o: %.c
	@echo "[CC] $<"
	@mkdir -p "$(dir $@)"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(L_INC) -o "$@" -c "$<"

$(BUILDDIR)/%.o: %.cpp
	@echo "[CXX] $<"
	@mkdir -p "$(dir $@)"
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(L_INC) -o "$@" -c "$<"

$(BUILDDIR)/%.o: %.ino
	@echo "[CXX] $<"
	@mkdir -p "$(dir $@)"
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(L_INC) -o "$@" -x c++ -include Arduino.h -c "$<"

$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	@echo "[LD] $@"
	@$(CC) $(LDFLAGS) -o "$@" $(OBJS) $(LIBS)

%.hex: %.elf
	@echo "[HEX] $@"
	@$(SIZE) "$<"
	@$(OBJCOPY) -O ihex -R .eeprom "$<" "$@"

# compiler generated dependency info
-include $(OBJS:.o=.d)

.PHONY: clean
clean:
	rm -rf "$(BUILDDIR)"
	rm -f "$(TARGET).elf" "$(TARGET).hex"
