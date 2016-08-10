MCU          = attiny4313
ARCH         = AVR8
F_CPU        = 8000000
OPTIMIZATION = s
TARGET       = main
SRC          = $(TARGET).cpp
CC_FLAGS     =
LD_FLAGS     =
CPP_STANDARD = c++11
AVRDUDE_PROGRAMMER = dragon_isp
AVRDUDE_FLAGS =

# Default target
all:

# Include DMBS build script makefiles
DMBS_PATH   ?= ~/src/github.com/abcminiuser/dmbs/DMBS
include $(DMBS_PATH)/core.mk
include $(DMBS_PATH)/gcc.mk
include $(DMBS_PATH)/cppcheck.mk
include $(DMBS_PATH)/doxygen.mk
include $(DMBS_PATH)/dfu.mk
include $(DMBS_PATH)/hid.mk
include $(DMBS_PATH)/avrdude.mk
include $(DMBS_PATH)/atprogram.mk

# avrdude -p atmega328 -c dragon_isp -U lfuse:w:0x7f:m -U hfuse:w:0xd9:m -U efuse:w:0xff:m
