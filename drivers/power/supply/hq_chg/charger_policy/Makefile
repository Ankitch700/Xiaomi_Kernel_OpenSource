obj-$(CONFIG_HUAQIN_CHARGER_MANAGER)        += main_chg.o
main_chg-objs:= hq_charger_manager.o hq_jeita.o hq_charge_sysfs.o
ifeq ($(CONFIG_XIAOMI_SMART_CHG),m)
main_chg-objs += xm_smart_chg.o
endif
obj-$(CONFIG_HUAQIN_CHARGER_MANAGER)        += ../common/hq_voter.o
obj-$(CONFIG_HUAQIN_CP_POLICY)              += hq_cp_policy.o
obj-$(CONFIG_HUAQIN_CP_POLICY)              += hq_pd_adapter.o
obj-$(CONFIG_PD_BATTERY_SECRET)             += xm_pd_adapter.o
obj-$(CONFIG_HUAQIN_QC_POLICY)              += hq_qc_policy.o
