obj-m += gt9916.o
gt9916-y := \
				goodix_brl_i2c.o \
				goodix_brl_spi.o \
				goodix_ts_core.o \
				goodix_brl_hw.o \
				goodix_cfg_bin.o \
				goodix_ts_utils.o \
				goodix_brl_fwupdate.o \
				goodix_ts_gesture.o \
				goodix_ts_inspect.o \
				goodix_ts_tools.o
# N6 code for HQ-336561 by huangshiquan at 2023/11/01 start
ifeq ($(KERNEL_FACTORY_BUILD),1)
KBUILD_CFLAGS += -DKERNEL_FACTORY_BUILD=1
endif
# N6 code for HQ-336561 by huangshiquan at 2023/11/01 end
