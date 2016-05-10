#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <mach/platform.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");

static const int XInputGpioPin = 18;
static const int YInputGpioPin = 27;
static const int ZInputGpioPin = 22;

static int GetADCValue(int GPIO)
{
	int counter = 0;
	volatile int milisec = 100;

	int i;


	gpio_direction_output(GPIO, 0);
	gpio_set_value(GPIO, 0);

	for(i = 0; i < 30000000; i++)
	{
		milisec--;
	}


	gpio_direction_input(GPIO);

	for(i = 0; i < 9999999; i++)
	{
		if(gpio_get_value(GPIO) == 0)
			counter++;
		else 
			break;
	}

	return counter;

}



int testPin = 18;
static ssize_t get_value(struct device *dev, struct device_attribute* attr, char *buf)
{
	gpio_direction_input(testPin);

	return sprintf(buf, "%d\n", gpio_get_value(testPin));
}
static DEVICE_ATTR(get_value, S_IRUGO, get_value, NULL);

static ssize_t show_x_input(struct device* dev, struct device_attribute* attr, char* buf)
{
	return sprintf(buf, "%d\n", GetADCValue(XInputGpioPin));
}
static DEVICE_ATTR(get_x_input, S_IRUGO, show_x_input, NULL);


static ssize_t show_y_input(struct device* dev, struct device_attribute* attr, char* buf)
{
	return sprintf(buf, "%d\n", GetADCValue(YInputGpioPin));
}
static DEVICE_ATTR(get_y_input, S_IRUGO, show_y_input, NULL);

static ssize_t show_z_input(struct device* dev, struct device_attribute* attr, char* buf)
{
	return sprintf(buf, "%d\n", GetADCValue(ZInputGpioPin));
}
static DEVICE_ATTR(get_z_input, S_IRUGO, show_z_input, NULL);




static struct class *s_pDeviceClass;
static struct device *s_pDeviceObject;

static int __init AccelModule_init(void)
{
	int result;


	result = gpio_request(XInputGpioPin, "");
	printk("gpio_request accel_x: %d\n", result);

	result = gpio_request(YInputGpioPin, "");
	printk("gpio_request accel_y: %d\n", result);

	result = gpio_request(ZInputGpioPin, "");
	printk("gpio_request accel_z: %d\n", result);

	

	s_pDeviceClass = class_create(THIS_MODULE, "accel");

	s_pDeviceObject = device_create(s_pDeviceClass, NULL, 0, NULL, "accel");


	result = device_create_file(s_pDeviceObject, &dev_attr_get_x_input);
	result = device_create_file(s_pDeviceObject, &dev_attr_get_y_input);
	result = device_create_file(s_pDeviceObject, &dev_attr_get_z_input);
	result = device_create_file(s_pDeviceObject, &dev_attr_get_value);

	printk("Init Accel Driver\n");


	return 0;
}

static void __exit AccelModule_exit(void)
{
	gpio_free(XInputGpioPin);
	gpio_free(YInputGpioPin);
	gpio_free(ZInputGpioPin);


	device_remove_file(s_pDeviceObject, &dev_attr_get_x_input);
	device_remove_file(s_pDeviceObject, &dev_attr_get_y_input);
	device_remove_file(s_pDeviceObject, &dev_attr_get_z_input);
	device_remove_file(s_pDeviceObject, &dev_attr_get_value);

	device_destroy(s_pDeviceClass, 0);
	class_destroy(s_pDeviceClass);
}

module_init(AccelModule_init);
module_exit(AccelModule_exit);
