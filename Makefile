PROGRAM=open-esp-webradio
PROGRAM_SRC_DIR=./src
PROGRAM_INC_DIR=./include
EXTRA_COMPONENTS = extras/i2c extras/i2s_dma
FLASH_SIZE = 32

include esp-open-rtos/common.mk

