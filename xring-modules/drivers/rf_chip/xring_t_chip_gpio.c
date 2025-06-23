#include <linux/device.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/pm_wakeirq.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/clk.h>

#define MI_T2_GPIO_COUNT_MAX 20
static int MI_T2_GPIO_COUNT = 4; //default 4
static int FIRST_GPIO_ID;
static bool b_legacy_drv = true;
#define MI_T2_MODE_MSK (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)	/*664*/
#define MI_T2_GPIO_PROC_NAME "modem_t1_gpio_ctrl"
extern bool __clk_is_enabled(struct clk *clk);
static bool clk_reg = true;

enum {
	MI_T2_GPIO_SATELLITE_CTL = 0,
	MI_T2_GPIO_WWAN_WLAN_1,
	MI_T2_GPIO_WWAN_WLAN_2,
	MI_ANT_GPIO_QM13111,
};

static const char * const mi_t2_gpio_names_legacy[] = {
	"gpio-satellite-ctl",
	"gpio-wwan-wlan-1",
	"gpio-wwan-wlan-2",
	"gpio-qm13111",
};

static const char * const mi_t2_gpio_names[] = {
	"mi-t2-gpio-1",
	"mi-t2-gpio-2",
	"mi-t2-gpio-3",
	"mi-t2-gpio-4",
	"mi-t2-gpio-5",
	"mi-t2-gpio-6",
	"mi-t2-gpio-7",
	"mi-t2-gpio-8",
	"mi-t2-gpio-9",
	"mi-t2-gpio-10",
	"mi-t2-gpio-11",
	"mi-t2-gpio-12",
	"mi-t2-gpio-13",
	"mi-t2-gpio-14",
	"mi-t2-gpio-15",
	"mi-t2-gpio-16",
	"mi-t2-gpio-17",
	"mi-t2-gpio-18",
	"mi-t2-gpio-19",
	"mi-t2-gpio-20",
};

struct gpio_data {
	int irq;
	int gpio_num;
	int gpio_status;
	int gpio_offset_num;
};

struct mi_t2_gpio_data {
	struct device *dev;

	struct mutex lock;	/* To set/get exported values in sysfs */
	struct gpio_data *data;

	/*char dev related data */
	char *driver_name;
	dev_t mi_t2_major;
	struct class *mi_t2_class;
	struct device *chardev;
	struct cdev cdev;
	struct clk *clk;
};


#define MINOR_NUMBER_COUNT 1

#define MI_T2_IOCTL_MAGIC 'y'
#define MI_T2_IOCTL_GPIO_SATELLITE_CTL_SET		_IOWR(MI_T2_IOCTL_MAGIC, 0, int)
#define MI_T2_IOCTL_GPIO_WWAN_WLAN_1_SET		_IOWR(MI_T2_IOCTL_MAGIC, 1, int)
#define MI_T2_IOCTL_GPIO_WWAN_WLAN_2_SET		_IOWR(MI_T2_IOCTL_MAGIC, 2, int)

#define MI_T2_IOCTL_GPIO_SATELLITE_CTL_GET		_IOWR(MI_T2_IOCTL_MAGIC, 3, int)
#define MI_T2_IOCTL_GPIO_WWAN_WLAN_1_GET		_IOWR(MI_T2_IOCTL_MAGIC, 4, int)
#define MI_T2_IOCTL_GPIO_WWAN_WLAN_2_GET		_IOWR(MI_T2_IOCTL_MAGIC, 5, int)

#define MI_T2_IOCTL_GPIO_QM13111_SET		    _IOWR(MI_T2_IOCTL_MAGIC, 6, int)
#define MI_T2_IOCTL_GPIO_QM13111_GET		    _IOWR(MI_T2_IOCTL_MAGIC, 7, int)


static ssize_t gpio_mi_t2_set_general(struct device *device, int gpio_type, const char *buf, size_t count)
{
	int rc = -1;
	int gpio = 0;
	struct mi_t2_gpio_data *mi_t2_data;

	mi_t2_data = dev_get_drvdata(device);
	gpio = mi_t2_data->data[gpio_type].gpio_num;

	mutex_lock(&mi_t2_data->lock);
	if (!strncmp(buf, "1", strlen("1"))) {
		if (gpio_is_valid(gpio)) {
			rc = gpio_direction_output(gpio, 1);
			if (rc) {
				dev_err(device, "mi_t2 %s: fail to set gpio %d !\n", __func__, gpio);
				goto unlock_ret;
			}
		} else {
			dev_err(device, "mi_t2 %s: unable to get gpio %d!\n", __func__, gpio);
		}
	} else if (!strncmp(buf, "0", strlen("0"))) {
		if (gpio_is_valid(gpio)) {
			rc = gpio_direction_output(gpio, 0);
			if (rc) {
				dev_err(device, "mi_t2 %s: fail to set gpio %d !\n", __func__, gpio);
				goto unlock_ret;
			}
		} else {
			dev_err(device, "mi_t2 %s: unable to get gpio %d!\n", __func__, gpio);
		}
	} else {
		rc = -EINVAL;
		dev_err(device, "mi_t2 %s: invalid input %s!only 0 or 1 is valid.\n", __func__, buf);
	}

unlock_ret:
	mutex_unlock(&mi_t2_data->lock);
	dev_info(device, "%s, gpio_type [%d] set, rc [%d], buf = %s\n", __func__, gpio_type, rc, buf);
	return count;
}



static ssize_t gpio_mi_t2_get_general(struct device *device, int gpio_type, char *buf)
{
	int value = -1;
	int gpio = 0;
	struct mi_t2_gpio_data *mi_t2_data;
	mi_t2_data = dev_get_drvdata(device);

	mutex_lock(&mi_t2_data->lock);
	gpio = mi_t2_data->data[gpio_type].gpio_num;

	if (gpio_is_valid(gpio)) {
		value = gpio_get_value(gpio);
	} else {
		dev_err(device, "mi_t2 %s: unable to get gpio %d!\n", __func__, gpio);
	}
	mutex_unlock(&mi_t2_data->lock);

	dev_info(device, "%s, gpio_type [%d] get, value [%d]\n", __func__, gpio_type, value);
	return sysfs_emit(buf, "%d\n", value);
}


static int gpio_mi_t2_request_gpio(struct platform_device *pdev, struct mi_t2_gpio_data *mi_t2_data, const char *gpio_name, int idx)
{
	int ret = 0;
	int gpio_num = -1;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	static int gpio_offset;
	if (idx == -1) {
		dev_err(dev, "Invalid gpio config name: %s", gpio_name);
		return -1;
	}

	gpio_num = of_get_named_gpio(np, gpio_name, 0);
	if (gpio_num < 0) {
		dev_err(dev, "Failed to get gpio %s, error: %d.\n", gpio_name, gpio_num);
	}

	ret = gpio_direction_output(gpio_num, 0);  //probe set all gpio to 0

    if (idx == 0) {
		gpio_offset = gpio_num - FIRST_GPIO_ID;
    }
    mi_t2_data->data[idx].gpio_offset_num = gpio_num-gpio_offset;

	ret = devm_gpio_request(dev, gpio_num, gpio_name);
	if (ret) {
		dev_err(dev, "Request gpio failed %s, error: %d.\n",  gpio_name, gpio_num);
		return ret;
	}
	dev_info(dev, "mi_t2_gpio %s, #%u. offset=%u\n", gpio_name, gpio_num, mi_t2_data->data[idx].gpio_offset_num);
	mi_t2_data->data[idx].gpio_num = gpio_num;

	return ret;
}

static int mi_t2_gpio_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct mi_t2_gpio_data *mi_t2_data = container_of(inode->i_cdev, struct mi_t2_gpio_data, cdev);
	struct device *dev = mi_t2_data->chardev;

	pr_info("Inside %s\n", __func__);
	get_device(dev);
	return ret;
}

static int mi_t2_gpio_release(struct inode *inode, struct file *file)
{
	struct mi_t2_gpio_data *mi_t2_data = container_of(inode->i_cdev, struct mi_t2_gpio_data, cdev);
	struct device *dev = mi_t2_data->chardev;

	pr_info("Inside %s\n", __func__);
	put_device(dev);
	return 0;
}


static ssize_t gpio_mi_t2_get_satellite_ctl(struct device *device, struct device_attribute *attr, char *buf)
{
	return gpio_mi_t2_get_general(device, MI_T2_GPIO_SATELLITE_CTL, buf);
}

static ssize_t gpio_mi_t2_set_satellite_ctl(struct device *device, struct device_attribute *attr, const char *buf, size_t count)
{
	return gpio_mi_t2_set_general(device, MI_T2_GPIO_SATELLITE_CTL, buf, count);
}

static DEVICE_ATTR(satellite_ctl, MI_T2_MODE_MSK, gpio_mi_t2_get_satellite_ctl, gpio_mi_t2_set_satellite_ctl);


static ssize_t gpio_mi_t2_get_wwan_wlan_1(struct device *device, struct device_attribute *attr, char *buf)
{
	return gpio_mi_t2_get_general(device, MI_T2_GPIO_WWAN_WLAN_1, buf);
}

static ssize_t gpio_mi_t2_set_wwan_wlan_1(struct device *device, struct device_attribute *attr, const char *buf, size_t count)
{
	return gpio_mi_t2_set_general(device, MI_T2_GPIO_WWAN_WLAN_1, buf, count);
}

static DEVICE_ATTR(wwan_wlan_1, MI_T2_MODE_MSK, gpio_mi_t2_get_wwan_wlan_1, gpio_mi_t2_set_wwan_wlan_1);


static ssize_t gpio_mi_t2_get_wwan_wlan_2(struct device *device, struct device_attribute *attr, char *buf)
{
	return gpio_mi_t2_get_general(device, MI_T2_GPIO_WWAN_WLAN_2, buf);
}

static ssize_t gpio_mi_t2_set_wwan_wlan_2(struct device *device, struct device_attribute *attr, const char *buf, size_t count)
{
	return gpio_mi_t2_set_general(device, MI_T2_GPIO_WWAN_WLAN_2, buf, count);
}

static DEVICE_ATTR(wwan_wlan_2, MI_T2_MODE_MSK, gpio_mi_t2_get_wwan_wlan_2, gpio_mi_t2_set_wwan_wlan_2);

static ssize_t gpio_mi_t2_get_qm13111(struct device *device, struct device_attribute *attr, char *buf)
{
	return gpio_mi_t2_get_general(device, MI_ANT_GPIO_QM13111, buf);
}

static ssize_t gpio_mi_t2_set_qm13111(struct device *device, struct device_attribute *attr, const char *buf, size_t count)
{
	return gpio_mi_t2_set_general(device, MI_ANT_GPIO_QM13111, buf, count);
}

static DEVICE_ATTR(gpio174, MI_T2_MODE_MSK, gpio_mi_t2_get_qm13111, gpio_mi_t2_set_qm13111);


static struct attribute *attributes[] = {
	&dev_attr_satellite_ctl.attr,
	&dev_attr_wwan_wlan_1.attr,
	&dev_attr_wwan_wlan_2.attr,
	&dev_attr_gpio174.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};



static long mi_t2_gpio_ioctl(struct file *file, unsigned int ioctl_num,
				unsigned long __user ioctl_param)
{
	int ret = 0;

	void __user *argp = (void __user *)ioctl_param;
	int __user *p = argp;
	int gpio_num;
	int gpio_value = 0;

	struct mi_t2_gpio_data *mi_t2_data = container_of(file->f_inode->i_cdev, struct mi_t2_gpio_data, cdev);

	pr_info("%s ioctl num %u\n", __func__, ioctl_num);
	switch (ioctl_num) {
	case MI_T2_IOCTL_GPIO_SATELLITE_CTL_SET:
		pr_info("mi_t2 MI_T2_IOCTL_GPIO_SATELLITE_CTL_SET !\n");
		get_user(gpio_value, p);
		if (!(mi_t2_data->data))
			break;
		gpio_num = mi_t2_data->data[MI_T2_GPIO_SATELLITE_CTL].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			ret = gpio_direction_output(gpio_num, gpio_value);
			if (ret) {
				pr_info("mi_t2 %s: fail to set gpio %d !\n", __func__, gpio_num);
			}
		} else {
			pr_info("mi_t2 %s: unable to get gpio %d!\n", __func__, gpio_num);
		}
	break;
	case MI_T2_IOCTL_GPIO_WWAN_WLAN_1_SET:
		pr_info("mi_t2 MI_T2_IOCTL_GPIO_WWAN_WLAN_1_SET !\n");
		get_user(gpio_value, p);
		if (!(mi_t2_data->data))
		break;
		gpio_num = mi_t2_data->data[MI_T2_GPIO_WWAN_WLAN_1].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			ret = gpio_direction_output(gpio_num, gpio_value);
			if (ret) {
				pr_info("mi_t2 %s: fail to set gpio %d !\n", __func__, gpio_num);
			}
		} else {
			pr_info("mi_t2 %s: unable to get gpio %d!\n", __func__, gpio_num);
		}
	break;
	case MI_T2_IOCTL_GPIO_WWAN_WLAN_2_SET:
		pr_info("mi_t2 MI_T2_IOCTL_GPIO_WWAN_WLAN_2_SET !\n");
		get_user(gpio_value, p);
		if (!(mi_t2_data->data))
		break;
		gpio_num = mi_t2_data->data[MI_T2_GPIO_WWAN_WLAN_2].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			ret = gpio_direction_output(gpio_num, gpio_value);
			if (ret) {
				pr_info("mi_t2 %s: fail to set gpio %d !\n", __func__, gpio_num);
			}
		} else {
			pr_info("mi_t2 %s: unable to get gpio %d!\n", __func__, gpio_num);
		}
	break;
	case MI_T2_IOCTL_GPIO_SATELLITE_CTL_GET:
		pr_info("mi_t2 MI_T2_IOCTL_GPIO_SATELLITE_CTL_GET !\n");
		if (!(mi_t2_data->data))
		break;
		gpio_num = mi_t2_data->data[MI_T2_GPIO_SATELLITE_CTL].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			gpio_value = gpio_get_value(gpio_num);
			put_user(gpio_value, p);
		} else {
			pr_info("mi_t2 %s: unable to read gpio %d!\n", __func__, gpio_num);
		}
	break;
	case MI_T2_IOCTL_GPIO_WWAN_WLAN_1_GET:
		pr_info("mi_t2 MI_T2_IOCTL_GPIO_WWAN_WLAN_1_GET !\n");
		if (!(mi_t2_data->data))
		break;
		gpio_num = mi_t2_data->data[MI_T2_GPIO_WWAN_WLAN_1].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			gpio_value = gpio_get_value(gpio_num);
			put_user(gpio_value, p);
		} else {
			pr_info("mi_t2 %s: unable to read gpio %d!\n", __func__, gpio_num);
		}
	break;
	case MI_T2_IOCTL_GPIO_WWAN_WLAN_2_GET:
		pr_info("mi_t2 MI_T2_IOCTL_GPIO_WWAN_WLAN_2_GET !\n");
		if (!(mi_t2_data->data))
		break;
		gpio_num = mi_t2_data->data[MI_T2_GPIO_WWAN_WLAN_2].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			gpio_value = gpio_get_value(gpio_num);
			put_user(gpio_value, p);
		} else {
			pr_info("mi_t2 %s: unable to read gpio %d!\n", __func__, gpio_num);
		}
	break;
	case MI_T2_IOCTL_GPIO_QM13111_SET:
		pr_info("mi_t2 MI_T2_IOCTL_GPIO_QM13111_SET !\n");
		get_user(gpio_value, p);
		if (!(mi_t2_data->data))
		break;
		gpio_num = mi_t2_data->data[MI_ANT_GPIO_QM13111].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			ret = gpio_direction_output(gpio_num, gpio_value);
			if (ret) {
				pr_info("mi_t2 %s: fail to set gpio %d !\n", __func__, gpio_num);
			}
		} else {
			pr_info("mi_t2 %s: unable to get gpio %d!\n", __func__, gpio_num);
		}
	break;
	case MI_T2_IOCTL_GPIO_QM13111_GET:
		pr_info("mi_t2 MI_T2_IOCTL_GPIO_QM13111_GET !\n");
		if (!(mi_t2_data->data))
		break;
		gpio_num = mi_t2_data->data[MI_ANT_GPIO_QM13111].gpio_num;
		if (gpio_is_valid(gpio_num)) {
			gpio_value = gpio_get_value(gpio_num);
			put_user(gpio_value, p);
		} else {
			pr_info("mi_t2 %s: unable to read gpio %d!\n", __func__, gpio_num);
		}
	break;
	default:
		pr_err("%s Entered default. Invalid ioctl num %u", __func__, ioctl_num);
		ret = -EINVAL;
	break;
	}
	return ret;
}



static  struct file_operations mi_t2_gpio_fops = {
	.owner = THIS_MODULE,
	.open = mi_t2_gpio_open,
	.release = mi_t2_gpio_release,
};

static int set_clk(struct device *dev, int clk_value) {
	int ret = 0;
    int clk_state = clk_value;
    struct mi_t2_gpio_data *mi_t2_data;
	pr_info("%s  enter\n", __func__);

    if (!dev) {
		pr_err("%s: Invalid parameter\n", __func__);
		return -EINVAL;
    }
    mi_t2_data = dev_get_drvdata(dev);
    if (!mi_t2_data) {
		pr_err("%s: Failed to get device data\n", __func__);
		return -ENODEV;
    }
    if (clk_state && !__clk_is_enabled(mi_t2_data->clk)) {
		ret = clk_prepare_enable(mi_t2_data->clk);
		if (ret) {
			pr_err("%s: Failed to enable clk: %d\n", __func__, ret);
			return ret;
		}
		pr_info("%s: Successfully enabled clk\n", __func__);
    } else if (!clk_state && __clk_is_enabled(mi_t2_data->clk)) {
		clk_disable_unprepare(mi_t2_data->clk);
		pr_info("%s: Successfully disabled clk\n", __func__);
    } else {
		pr_info("%s: Clk is already in the undesired state\n", __func__);
    }
    return ret;
}

ssize_t t2_gpio_write(struct file *file, const char __user *buffer, size_t count, loff_t *ops)
{
    char config[256];
	char clk_config[256] = {0};
	int i = 0;
	struct mi_t2_gpio_data *mi_t2_data = NULL;
	mi_t2_data = (struct mi_t2_gpio_data *) file_inode(file)->i_private;
	if (mi_t2_data == NULL)
	{
		pr_err("%s  failed.  data = null", __func__);
		return 0;
	}

    if (count <= 0) {
		pr_err("%s  failed. support count: %d : write count:%zu", __func__, MI_T2_GPIO_COUNT, count);
		return -1;
    }

	if (clk_reg) {
		if (count > sizeof(clk_config)) {
			return -EINVAL;
		}
		if (copy_from_user(clk_config, buffer, count)) {
			return -EFAULT;
		}
		pr_info("%s  clk_config= %s\n", __func__, clk_config);
		if (strncmp(clk_config, "ON", 2) == 0) {
			pr_info("%s  enter clk enable\n", __func__);
			set_clk(mi_t2_data->dev, 1);
			return count;
		} else if (strncmp(clk_config, "OFF", 3) == 0) {
			pr_info("%s enter clk disable\n", __func__);
			set_clk(mi_t2_data->dev, 0);
			return count;
		} else {
			printk(KERN_INFO "Invalid input clk set\n");
		}
	}
    memset(config, 0xff, MI_T2_GPIO_COUNT);
    config[MI_T2_GPIO_COUNT] = 0;
    pr_info("%s  count = %zu\n", __func__, count);
    if (copy_from_user((void *)config, (const void __user *)buffer, count > MI_T2_GPIO_COUNT?MI_T2_GPIO_COUNT:count)) {
		return -EFAULT;
	}
	pr_info("%s  config[0]= 0x%x\n", __func__, config[0]);
    for (i = 0; i < MI_T2_GPIO_COUNT; i++)
	{
		if (config[i] != 0xff)
		{
			gpio_mi_t2_set_general(mi_t2_data->dev, i, config+i, 1);
		}
	}
    return count;
}

static int gpio_t2_get_state(struct device *device, int gpio_type, char *buf)
{
    int value = -1;
    int gpio = 0;
    struct mi_t2_gpio_data *mi_t2_data;
    mi_t2_data = dev_get_drvdata(device);
    mutex_lock(&mi_t2_data->lock);
    gpio = mi_t2_data->data[gpio_type].gpio_num;
    if (gpio_is_valid(gpio)) {
		value = gpio_get_value(gpio);
    } else {
		dev_err(device, "mi_t2 %s: unable to get gpio %d!\n", __func__, gpio);
    }
    mutex_unlock(&mi_t2_data->lock);
    dev_info(device, "%s, gpio_type [%d] get, value [%d]\n", __func__, gpio_type, value);
    *buf = (char)value;
    return mi_t2_data->data[gpio_type].gpio_offset_num;
}


static int t2_gpio_show(struct seq_file *m, void *v)
{
    int i = 0, gpio = 0;
    char config[4];
    struct mi_t2_gpio_data *mi_t2_data = (struct mi_t2_gpio_data *) m->private;;
    seq_printf(m, "GPIO count=%d : \n", MI_T2_GPIO_COUNT);
#if 1
    for (i = 0; i < MI_T2_GPIO_COUNT; i++)
    {
		gpio = gpio_t2_get_state(mi_t2_data->dev, i, config);
		seq_printf(m, "GPIO-%d state=%d \n", gpio, config[0]);
    }
#endif
    return 0;
}
static int t2_gpio_open(struct inode *inode, struct file *file)
{
    return single_open(file, t2_gpio_show, file_inode(file)->i_private);
}

static struct proc_ops   t2_gpio_fops = {
    .proc_open	= t2_gpio_open,
    .proc_read	= seq_read,
    .proc_lseek	= seq_lseek,
    .proc_release	= single_release,
    .proc_write	= t2_gpio_write,
};


static int gpio_mi_t2_reg_chrdev(struct mi_t2_gpio_data *mi_t2_data)
{
	int ret = 0;

	mi_t2_data->driver_name = "mi_t2_chip";
	ret = alloc_chrdev_region(&mi_t2_data->mi_t2_major, 0, MINOR_NUMBER_COUNT, mi_t2_data->driver_name);
	if (ret < 0) {
		pr_err("%s alloc_chr_dev_region failed ret : %d\n", __func__, ret);
		return ret;
	}
	pr_info("%s major number %d", __func__, MAJOR(mi_t2_data->mi_t2_major));

	mi_t2_data->mi_t2_class = class_create(mi_t2_data->driver_name);
	if (IS_ERR(mi_t2_data->mi_t2_class)) {
		ret = PTR_ERR(mi_t2_data->mi_t2_class);
		pr_err("%s class create failed. ret : %d", __func__, ret);
		goto err_class;
	}

	mi_t2_data->chardev = device_create(mi_t2_data->mi_t2_class, NULL,
				mi_t2_data->mi_t2_major, NULL,
				mi_t2_data->driver_name);
	if (IS_ERR(mi_t2_data->chardev)) {
		ret = PTR_ERR(mi_t2_data->chardev);
		pr_err("%s device create failed ret : %d\n", __func__, ret);
		goto err_device;
	}

	cdev_init(&mi_t2_data->cdev, &mi_t2_gpio_fops);
	ret = cdev_add(&mi_t2_data->cdev, mi_t2_data->mi_t2_major, 1);
	if (ret) {
		pr_err("%s cdev add failed, ret : %d\n", __func__, ret);
		goto err_cdev;
	}
	return ret;

err_cdev:
	device_destroy(mi_t2_data->mi_t2_class, mi_t2_data->mi_t2_major);
err_device:
	class_destroy(mi_t2_data->mi_t2_class);
err_class:
	unregister_chrdev_region(0, MINOR_NUMBER_COUNT);
	return ret;
}




static int gpio_mi_t2_probe(struct platform_device *pdev)
{
	int i;
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct mi_t2_gpio_data *mi_t2_data;
    u32 config_gpio_count = 0;
	u32 config_gpio_first = 0;
	const char *clk_names = NULL;
	clk_reg = false;
	pr_info("%s\n", __func__);

	mi_t2_data = devm_kzalloc(dev, sizeof(struct mi_t2_gpio_data), GFP_KERNEL);
	if (!mi_t2_data) {
		dev_err(dev, "Memor allocation error!\n");
		return -ENOMEM;
	}
	ret = of_property_read_u32(dev->of_node, "gpio-count", &config_gpio_count);
    if (ret == 0) {
		if (MI_T2_GPIO_COUNT_MAX <= config_gpio_count) {
			dev_err(dev, "Too many gpio controlled,  please enlarge MI_T2_GPIO_COUNT_MAX if it is right !\n");
			return -ENOMEM;
		}
		MI_T2_GPIO_COUNT = config_gpio_count;
		b_legacy_drv  = false;
		pr_info("t2_gpio_count = %d.\n", config_gpio_count);
    } else {
		dev_err(dev, " Get t2_gpio_count failed %d use default value 4 !\n", ret);
		return -ENOMEM;
    }

    ret = of_property_read_u32(dev->of_node, "first-num", &config_gpio_first);
	if (ret == 0) {
		pr_info("first gpio num = %d.\n", config_gpio_first);
		FIRST_GPIO_ID = config_gpio_first;
	} else {
		dev_err(dev, " Get first gpio num failed %d use default value 0 !\n", ret);
		FIRST_GPIO_ID = 0;
		return -ENOMEM;
	}

	mi_t2_data->data = devm_kcalloc(dev, MI_T2_GPIO_COUNT, sizeof(struct gpio_data), GFP_KERNEL);
	if (!mi_t2_data->data) {
		dev_err(dev, "Memor allocation error!.\n");
		return -ENOMEM;
	}

	ret = of_property_read_string(dev->of_node, "clock-names", &clk_names);
	if (ret == 0) {
		pr_info("clock-names = %s.\n", clk_names);
		mi_t2_data->clk = devm_clk_get(dev, clk_names);
		if (!IS_ERR(mi_t2_data->clk)) {
			clk_reg = true;
			pr_info("clk_get success\n");
			if (__clk_is_enabled(mi_t2_data->clk)) {
				clk_disable_unprepare(mi_t2_data->clk);
				pr_info("clk default is set disable \n");
			}
			} else {
				pr_err("clk_get failed\n");
			}
		} else {
			dev_err(dev, " This project does not support clocks %d use default value !\n", ret);
		}
	mutex_init(&mi_t2_data->lock);
	mi_t2_data->dev = dev;
	platform_set_drvdata(pdev, mi_t2_data);

	if (!mi_t2_data) {
		dev_err(dev, "Memor allocation error!\n");
		return -ENOMEM;
	} else {
		if (b_legacy_drv) {
			for (i = 0; i < MI_T2_GPIO_COUNT; i++) {
				gpio_mi_t2_request_gpio(pdev, mi_t2_data,  mi_t2_gpio_names_legacy[i], i);
			}
		} else {
			for (i = 0; i < MI_T2_GPIO_COUNT; i++) {
				gpio_mi_t2_request_gpio(pdev, mi_t2_data,  mi_t2_gpio_names[i], i);
			}
		}
	}

    if (b_legacy_drv) {
	    ret = sysfs_create_group(&dev->kobj, &attribute_group);
	    if (ret < 0) {
		    dev_err(dev, "Failed to create sysfs node.\n");
		    return -EINVAL;
	    }
		mi_t2_gpio_fops.unlocked_ioctl = mi_t2_gpio_ioctl;
    }

	device_init_wakeup(dev, true);
	ret = gpio_mi_t2_reg_chrdev(mi_t2_data);
	if (ret) {
		pr_err("%s register char dev failed, rc : %d", __func__, ret);
		return ret;
	}
	proc_create_data(MI_T2_GPIO_PROC_NAME, 0777, NULL, &t2_gpio_fops, (void *) mi_t2_data);
	return ret;
}

static int gpio_mi_t2_remove(struct platform_device *pdev)
{
	struct mi_t2_gpio_data *mi_t2_data = platform_get_drvdata(pdev);
    remove_proc_entry(MI_T2_GPIO_PROC_NAME, NULL);
	sysfs_remove_group(&pdev->dev.kobj, &attribute_group);
	dev_pm_clear_wake_irq(mi_t2_data->dev);

	mutex_destroy(&mi_t2_data->lock);

	return 0;
}


#if defined(CONFIG_OF)
static const struct of_device_id gpio_mi_t2_of_match[] = {
	{ .compatible = "mdm,mi_t2_chip_gpio", },
	{},
};
MODULE_DEVICE_TABLE(of, gpio_mi_t2_of_match);
#endif


#ifdef CONFIG_PM
static int gpio_mi_t2_suspend(struct device *dev)
{
	struct mi_t2_gpio_data *mi_t2_data;

	mi_t2_data = dev_get_drvdata(dev);
	dev_info(dev, "mi_t2_suspend.\n");

	return 0;
}

static int gpio_mi_t2_resume(struct device *dev)
{
	struct mi_t2_gpio_data *mi_t2_data;

	mi_t2_data = dev_get_drvdata(dev);
	dev_info(dev, "mi_t2_resume.\n");

	return 0;
}

static const struct dev_pm_ops gpio_mi_t2_pm_ops = {
	.suspend = gpio_mi_t2_suspend,
	.resume = gpio_mi_t2_resume,
	.freeze = gpio_mi_t2_suspend,
	.restore = gpio_mi_t2_resume,
};
#endif

static struct platform_driver gpio_mi_t2_driver = {
	.driver = {
		.name = "gpio-mi-t2",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gpio_mi_t2_of_match),
#ifdef CONFIG_PM
		.pm = &gpio_mi_t2_pm_ops,
#endif
	},
	.probe = gpio_mi_t2_probe,
	.remove = gpio_mi_t2_remove,
};

static int __init gpio_mi_t2_driver_init(void)
{
	int err;

	pr_info("%s.\n", __func__);
	err = platform_driver_register(&gpio_mi_t2_driver);
	if (err) {
	    pr_err("%s error: %d\n", __func__, err);
	}

	return err;
}

static void __exit gpio_mi_t2_driver_exit(void)
{
	platform_driver_unregister(&gpio_mi_t2_driver);
}

module_init(gpio_mi_t2_driver_init);
module_exit(gpio_mi_t2_driver_exit);

//module_platform_driver(gpio_mi_t2_driver);
MODULE_AUTHOR("zhanghanghang@xiaomi.com");
MODULE_DESCRIPTION("Driver for mi_t2 GPIO control");
MODULE_LICENSE("GPL v2");