#include <linux/module.h>

int thermal_message_init(void)
{
	return 0;
}

void thermal_message_exit(void)
{
}

module_init(thermal_message_init);
module_exit(thermal_message_exit);

MODULE_LICENSE("GPL v2");
