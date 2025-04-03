ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/misc/mediatek/sspm/v2 \
	-I$(srctree)/drivers/misc/mediatek/mcupm/include \
	-I$(srctree)/drivers/gpu/mediatek/gpufreq

obj-$(CONFIG_MTK_SOC_THERMAL)	+= soc_temp.o
obj-$(CONFIG_MTK_SOC_THERMAL_LVTS)	+= soc_temp_lvts.o
obj-$(CONFIG_MTK_PMIC_THERMAL) += pmic_temp.o
obj-$(CONFIG_MTK_MD_THERMAL)	+= md_cooling_all.o
md_cooling_all-y 		+= md_cooling.o
md_cooling_all-$(CONFIG_DEBUG_FS) += md_cooling_debugfs.o

obj-$(CONFIG_MTK_CHARGER_THERMAL) += charger_cooling.o
obj-$(CONFIG_MTK_BACKLIGHT_THERMAL) += backlight_cooling.o

obj-$(CONFIG_MTK_THERMAL_TRACE) += thermal_trace.o

obj-$(CONFIG_MTK_THERMAL_IPI)	+= thermal_ipi.o

obj-$(CONFIG_MTK_THERMAL_INTERFACE)	+= thermal_interface.o

obj-$(CONFIG_MTK_THERMAL_JATM)	+= thermal_jatm.o

obj-$(CONFIG_MTK_BOARD_THERMAL)	+= board_temp.o

# N6 code for HQ-308650 by yingxuezhen at 2023/07/24 start
obj-m += mi_thermal_message.o
# N6 code for HQ-308650 by yingxuezhen at 2022/07/24 end

# N6 code for HQ-309221 by yingxuezhen at 2023/07/26 start
obj-m += hq_CPU_temp.o
# N6 code for HQ-309221 by yingxuezhen at 2022/07/26 end

# N6 code for HQ-309377 by hankang at 2023/07/26 start
obj-m += hq_charge_temp.o
# N6 code for HQ-309377 by hankang at 2022/07/26 end
# N6 code for HQ-304647 by xiexinli at 2023/07/28 start
obj-m += hq_fl_temp.o
# N6 code for HQ-304647 by xiexinli at 2023/07/28 start
