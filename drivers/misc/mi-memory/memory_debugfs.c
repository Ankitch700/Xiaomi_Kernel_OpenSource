#include <linux/module.h>
#include <linux/kernel.h>

static int __init mi_memory_sysfs_init(void)
{
	return 0;
}

static void __exit mi_memory_sysfs_exit(void)
{

}

subsys_initcall(mi_memory_sysfs_init);
module_exit(mi_memory_sysfs_exit);

MODULE_DESCRIPTION("Interface for xiaomi memory");
MODULE_AUTHOR("Venco <duwenchao@xiaomi.com>");
MODULE_LICENSE("GPL");
