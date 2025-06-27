load(":target_variants.bzl", "la_variants")
load(":msm_kernel_la.bzl", "define_msm_la")
load(":image_opts.bzl", "boot_image_opts")
load(":xiaomi_sm8750_common.bzl", "xiaomi_common_in_tree_modules")
load(":sun.bzl",
        "target_arch",
        "target_arch_in_tree_modules",
        "target_arch_consolidate_in_tree_modules",
        "consolidate_board_kernel_cmdline_extras",
        "consolidate_board_bootconfig_extras",
        "consolidate_kernel_vendor_cmdline_extras",
        "perf_board_kernel_cmdline_extras",
        "perf_board_bootconfig_extras",
        "perf_kernel_vendor_cmdline_extras",
)

target_name = "miro"

def define_miro():
    for variant in la_variants:
        _target_in_tree_modules = target_arch_in_tree_modules + xiaomi_common_in_tree_modules + [
            # keep sorted
            "drivers/gpio/gpio-mi-t1.ko",
            "drivers/power/supply/mca/mca_hardware_ic/wireless_ic/sc96281/sc96281_comp.ko",
            "drivers/media/rc/ir-spi.ko",
            "drivers/power/xm_power/xm_power.ko",
            "drivers/mihw/game/migt.ko",
            "drivers/mihw/millet/millet_binder.ko",
            "drivers/mihw/millet/millet_core.ko",
            "drivers/mihw/millet/millet_hs.ko",
            "drivers/mihw/millet/millet_pkg.ko",
            "drivers/mihw/millet/millet_sig.ko",
            "drivers/mihw/millet/millet_oem_cgroup.ko",
            "drivers/mihw/millet/binder_gki.ko",
            "drivers/mihw/powersave/powersave.ko",
            "drivers/mihw/unionpower/unionpower.ko",
            "drivers/leds/leds-aw21024.ko",
	    "drivers/staging/mi_mem_engine/mi_mem_engine.ko",
            "drivers/block/zram/zram.ko",
            "mm/zsmalloc.ko",
            "drivers/mtd/mtd_blkdevs.ko",
            "drivers/mtd/parsers/ofpart.ko",
            "drivers/mtd/mtdoops.ko",
            "drivers/mtd/devices/block2mtd.ko",
            "drivers/mtd/chips/chipreg.ko",
            "drivers/mtd/mtdblock.ko",
            "drivers/mtd/mtd.ko",
            ]

        _target_consolidate_in_tree_modules = _target_in_tree_modules + \
                target_arch_consolidate_in_tree_modules + [
            # keep sorted
            ]

        if variant == "consolidate":
            mod_list = _target_consolidate_in_tree_modules
            board_kernel_cmdline_extras = consolidate_board_kernel_cmdline_extras
            board_bootconfig_extras = consolidate_board_bootconfig_extras
            kernel_vendor_cmdline_extras = consolidate_kernel_vendor_cmdline_extras
        else:
            mod_list = _target_in_tree_modules
            board_kernel_cmdline_extras = perf_board_kernel_cmdline_extras
            board_bootconfig_extras = perf_board_bootconfig_extras
            kernel_vendor_cmdline_extras = perf_kernel_vendor_cmdline_extras

        define_msm_la(
            msm_target = target_name,
            msm_arch = target_arch,
            variant = variant,
            in_tree_module_list = mod_list,
            boot_image_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a9c000",
                kernel_vendor_cmdline_extras = kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            ),
        )

