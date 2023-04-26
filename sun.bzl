load(":target_variants.bzl", "la_variants")
load(":msm_kernel_la.bzl", "define_msm_la")
load(":image_opts.bzl", "boot_image_opts")

target_name = "sun"

def define_sun():
    _sun_in_tree_modules = [
        # keep sorted
        "drivers/power/reset/qcom-reboot-reason.ko",
    ]

    _sun_consolidate_in_tree_modules = _sun_in_tree_modules + [
        # keep sorted
        "drivers/misc/lkdtm/lkdtm.ko",
        "kernel/locking/locktorture.ko",
        "kernel/rcu/rcutorture.ko",
        "kernel/torture.ko",
        "lib/atomic64_test.ko",
        "lib/test_user_copy.ko",
    ]

    for variant in la_variants:
        if variant == "consolidate":
            mod_list = _sun_consolidate_in_tree_modules
        else:
            mod_list = _sun_in_tree_modules

        define_msm_la(
            msm_target = target_name,
            variant = variant,
            in_tree_module_list = mod_list,
            boot_image_opts = boot_image_opts(
                earlycon_addr = "hvc0",
                kernel_vendor_cmdline_extras = [
                    # do not sort
                    "console=hvc0",
                    "nowatchdog",	# disable wdog for now
                    "hvc_dcc.enable=1",
                    "bootconfig",
                    "printk.devkmsg=on",
                    "loglevel=8",
                    "nokaslr",
                    "androidboot.first_stage_console=1",
                ],
            ),
        )
