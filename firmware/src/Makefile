TARGET = dat-ting

# Hardware type - Module or Prototype
HW_TYPE = Module

USE_DAISYSP_LGPL = 1

# Sources
CPP_SOURCES = main.cpp

# Version-dependent files - require templates
VERSION_FILES = ./version.h ./VERSION.txt

# Bootloader options
# Regular bootloader with code in internal flash: 
#APP_TYPE = BOOT_NONE
# Daisy bootloader with code in SRAM:
APP_TYPE = BOOT_SRAM

# Compiler settings
GCC_PATH = "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.3 rel1\bin"
CPP_STANDARD = -std=gnu++23
OPT = -O3
C_INCLUDES = -I../inc
##C_DEFS = -DPROGNAME=$(TARGET) -DBOOT_TYPE=$(APP_TYPE)
C_DEFS = -DHW_TYPE=$(HW_TYPE) -DPROGNAME=$(TARGET) -DBOOT_TYPE=$(APP_TYPE)

# Library locations
LIBDAISY_DIR = ../../lib/libDaisy
DAISYSP_DIR = ../../lib/DaisySP
MAKEVER_DIR = ../../lib/MakeVersionInfo

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# DEBUG: Save compiler's assembler output
CFLAGS += -save-temps

# Omit debugging info
CFLAGS += -g0 -ggdb0

# Update version info
include $(MAKEVER_DIR)/Makefile

main.cpp: ./version.h
