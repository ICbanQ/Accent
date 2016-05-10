#include <linux/init.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <asm/io.h>
#include <mach/platform.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/tty.h>

MODULE_LICENSE("GPL");


static const int motor_12_gpio = 17;
static const int motor_34_gpio = 19;
static const int motor_led1_gpio = 7;
static const int motor_led2_gpio = 8;


static void execute_string(char *str)
{
	int len;
	int i;
	char sz[256];

	char *argv[3];


	static char *envp[] = {
		"HOME=/",
		"TERM=linux",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };


		//{ "/usr/bin/cretures_motordriver_exec", str, NULL }; 			// path, arg,,,, termination

	argv[0] = "/usr/bin/motordriver_exec";
	argv[1] = str;
	argv[2] = NULL;

	call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);

}

/**********************/
/*Driver Attributes*/
static ssize_t servo_test_in(struct device* dev, struct device_attribute* attr, char* buf)
{
	execute_string("st#");
	return sprintf(buf, "ACK\n");
}
static DEVICE_ATTR(servo_test, S_IRUGO, servo_test_in, NULL);

static ssize_t servo_enable_in(struct device* dev, struct device_attribute* attr, char* buf)
{
	execute_string("se#");
	return sprintf(buf, "ACK\n");
}
static DEVICE_ATTR(servo_enable, S_IRUGO, servo_enable_in, NULL);

static ssize_t servo_disable_in(struct device* dev, struct device_attribute* attr, char* buf)
{
	execute_string("sd#");
	return sprintf(buf, "ACK\n");
}
static DEVICE_ATTR(servo_disable, S_IRUGO, servo_disable_in, NULL);

int input_count = 0;
char input_buf[256] = { '\0', };
char cmd_buf[256] = { '\0', };

static ssize_t show_servo_param(struct device* dev, struct device_attribute* attr, char* buf)
{
	return sprintf(buf, "%s\n", cmd_buf);

}
static ssize_t store_servo_all_velocity(struct device* dev, struct device_attribute* attr, char* buf, size_t count)
{
	input_count = count;

	memcpy(input_buf, buf, count - 1);
	input_buf[count - 1] = '\0';

	sprintf(cmd_buf, "sav %s#", input_buf);
	execute_string(cmd_buf);

	return count;
}
static DEVICE_ATTR(servo_all_velocity, S_IWUSR | S_IRUGO, show_servo_param, store_servo_all_velocity);



static ssize_t store_servo_all_position(struct device* dev, struct device_attribute* attr, char* buf, size_t count)
{
	input_count = count;

	memcpy(input_buf, buf, count - 1);
	input_buf[count - 1] = '\0';

	sprintf(cmd_buf, "sa %s#", input_buf);
	execute_string(cmd_buf);

	return count;
}
static DEVICE_ATTR(servo_all_position, S_IWUSR | S_IRUGO, show_servo_param, store_servo_all_position);


static ssize_t store_servo_n(struct device* dev, struct device_attribute* attr, char* buf, size_t count)
{
	input_count = count;

	memcpy(input_buf, buf, count - 1);
	input_buf[count - 1] = '\0';

	sprintf(cmd_buf, "s%s#", input_buf);
	execute_string(cmd_buf);

	return count;
}
static DEVICE_ATTR(servo_n, S_IWUSR | S_IRUGO, show_servo_param, store_servo_n);

static ssize_t store_servo_init_pos(struct device* dev, struct device_attribute* attr, char* buf, size_t count)
{
	input_count = count;

	memcpy(input_buf, buf, count - 1);
	input_buf[count - 1] = '\0';

	sprintf(cmd_buf, "sia %s#", input_buf);
	execute_string(cmd_buf);

	return count;
}
static DEVICE_ATTR(servo_init_pos, S_IWUSR | S_IRUGO, show_servo_param, store_servo_init_pos);


static ssize_t show_motor_12_on(struct device* dev, struct device_attribute* attr, char* buf)
{
	gpio_set_value(motor_12_gpio, 1);
	gpio_set_value(motor_led1_gpio, 1);

	return sprintf(buf, "ACK\n");
}
static DEVICE_ATTR(motor_12_on, S_IRUGO, show_motor_12_on, NULL);

static ssize_t show_motor_12_off(struct device* dev, struct device_attribute* attr, char* buf)
{
	gpio_set_value(motor_12_gpio, 0);
	gpio_set_value(motor_led1_gpio, 0);

	return sprintf(buf, "ACK\n");
}
static DEVICE_ATTR(motor_12_off, S_IRUGO, show_motor_12_off, NULL);


static ssize_t show_motor_34_on(struct device* dev, struct device_attribute* attr, char* buf)
{
	gpio_set_value(motor_34_gpio, 1);
	gpio_set_value(motor_led2_gpio, 1);

	return sprintf(buf, "ACK\n");
}
static DEVICE_ATTR(motor_34_on, S_IRUGO, show_motor_34_on, NULL);

static ssize_t show_motor_34_off(struct device* dev, struct device_attribute* attr, char* buf)
{
	gpio_set_value(motor_34_gpio, 0);
	gpio_set_value(motor_led2_gpio, 0);

	return sprintf(buf, "ACK\n");
}
static DEVICE_ATTR(motor_34_off, S_IRUGO, show_motor_34_off, NULL);




static struct class *s_pDeviceClass;
static struct device *s_pDeviceObject;

static int __init MotorDriverModule_init(void)
{
	int result;


	result = gpio_request(motor_12_gpio, "");
	printk("motor 12 %d\n", result);
	result = gpio_request(motor_34_gpio, "");
	printk("motor 34 %d\n", result);
	result = gpio_request(motor_led1_gpio, "");
	printk("led 1 %d\n", result);
	result = gpio_request(motor_led2_gpio, "");
	printk("led 2 %d\n", result);

	gpio_direction_output(motor_12_gpio, 0);
	gpio_direction_output(motor_34_gpio, 0);
	gpio_direction_output(motor_led1_gpio, 0);
	gpio_direction_output(motor_led2_gpio, 0);

	gpio_set_value(motor_12_gpio, 0);
	gpio_set_value(motor_34_gpio, 0);
	gpio_set_value(motor_led1_gpio, 0);
	gpio_set_value(motor_led2_gpio, 0);


	s_pDeviceClass = class_create(THIS_MODULE, "cretures_motordriver");
	//BUG_ON(IS_ERR(s_pDeviceClass));

	s_pDeviceObject = device_create(s_pDeviceClass, NULL, 0, NULL, "cretures_motordriver");
	//BUG_ON(IS_ERR(s_pDeviceObject));

	result = device_create_file(s_pDeviceObject, &dev_attr_servo_test);
	result = device_create_file(s_pDeviceObject, &dev_attr_servo_enable);
	result = device_create_file(s_pDeviceObject, &dev_attr_servo_disable);
	result = device_create_file(s_pDeviceObject, &dev_attr_servo_all_velocity);
	result = device_create_file(s_pDeviceObject, &dev_attr_servo_all_position);
	result = device_create_file(s_pDeviceObject, &dev_attr_servo_n);
	result = device_create_file(s_pDeviceObject, &dev_attr_servo_init_pos);
	result = device_create_file(s_pDeviceObject, &dev_attr_motor_12_on);
	result = device_create_file(s_pDeviceObject, &dev_attr_motor_12_off);
	result = device_create_file(s_pDeviceObject, &dev_attr_motor_34_on);
	result = device_create_file(s_pDeviceObject, &dev_attr_motor_34_off);
	//result = device_create_file(s_pDeviceObject, &dev_attr_get_value);
	//BUG_ON(result < 0);

	printk("Init Cretures Motor Driver\n");


	//ttyAMA0 -> 9600
	execute_string("init");


	return 0;
}

static void __exit MotorDriverModule_exit(void)
{
	gpio_free(motor_12_gpio);
	gpio_free(motor_34_gpio);
	gpio_free(motor_led1_gpio);
	gpio_free(motor_led2_gpio);


	device_remove_file(s_pDeviceObject, &dev_attr_servo_test);
	device_remove_file(s_pDeviceObject, &dev_attr_servo_enable);
	device_remove_file(s_pDeviceObject, &dev_attr_servo_disable);
	device_remove_file(s_pDeviceObject, &dev_attr_servo_all_velocity);
	device_remove_file(s_pDeviceObject, &dev_attr_servo_all_position);
	device_remove_file(s_pDeviceObject, &dev_attr_servo_n);
	device_remove_file(s_pDeviceObject, &dev_attr_servo_init_pos);
	device_remove_file(s_pDeviceObject, &dev_attr_motor_12_on);
	device_remove_file(s_pDeviceObject, &dev_attr_motor_12_off);
	device_remove_file(s_pDeviceObject, &dev_attr_motor_34_on);
	device_remove_file(s_pDeviceObject, &dev_attr_motor_34_off);


	device_destroy(s_pDeviceClass, 0);
	class_destroy(s_pDeviceClass);

}

module_init(MotorDriverModule_init);
module_exit(MotorDriverModule_exit);
