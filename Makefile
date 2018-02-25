PROGRAM=open-esp-webradio
PROGRAM_SRC_DIR=./src ./libmad
PROGRAM_INC_DIR=./include
LINKER_SCRIPTS=./ld/app.ld
EXTRA_COMPONENTS=extras/i2c extras/i2s_dma $(PROGRAM_DIR)assets
FLASH_SIZE=32
EXTRA_CFLAGS=-DINCLUDE_eTaskGetState=1

include esp-open-rtos/common.mk
