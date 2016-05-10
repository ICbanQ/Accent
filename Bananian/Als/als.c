#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>

struct als_data {
	atomic_t enable; /* attribute value */
	atomic_t delay; /* attribute value */
	atomic_t position; /* attribute value */
	atomic_t threshold; /* attribute value */
	//struct acceleration last; /* last measured data */
	struct mutex enable_mutex;
	struct mutex data_mutex;
	struct i2c_client *client;
	struct input_dev *input;
	struct delayed_work work;
};

static struct als_data *p_ald;
static struct i2c_client *g_client;
unsigned char gread_val[3];

int p_als_write(u8 reg, u8 data) {
        u8 buf[2];
        struct i2c_msg msg = {g_client->addr, 0, 2, buf};

        buf[0] = reg;
        buf[1] = data;

        return i2c_transfer(g_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}

int p_als_read(u8 reg) {
        u8 buf[1];
        struct i2c_msg msg = {g_client->addr, 0, 1, buf};
        int ret;

        buf[0] = reg;

        ret = i2c_transfer(g_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
        if (ret == -EIO) {
                printk("als i2c transfer error\n");
                return -EIO;
        }

	msg.flags = I2C_M_RD;

        ret = i2c_transfer(g_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO) {
		printk("als i2c transfer error\n");
        return -EIO;
	}

	//printk("[%s] isl29023 i2c reg ==> %x \n",__func__, buf[0]);
	return buf[0];
}


#define SAMPLECOUNT 10
#define NEW_SAMPLECOUNT	3

static int sampleData[SAMPLECOUNT];
static int sampleCount = 0;

static int summedValue = 0;
static int averageValue = 0;

static int dummyTemperature = 0;
static int dummyHumidity = 0;

static void p_als_work_func(struct work_struct *work) {
    	int value = 0;
	int i;
	gread_val[0] = p_als_read(0x02);
	gread_val[1] = p_als_read(0x03);
	value = gread_val[1];
	value <<= 8;
	value |= gread_val[0];

	sampleData[sampleCount] = value;
	sampleCount++;

	summedValue += value;

	//printk("sample value %d\n", value);

	if(sampleCount >= SAMPLECOUNT){

		averageValue = summedValue / SAMPLECOUNT;

		//printk("average sample value %d\n", averageValue);
		//dummy temperature, humidity
		dummyTemperature = averageValue % 10 + 10;
		dummyHumidity = (averageValue / 2) % 10 +70;

		for(i = 0; i < NEW_SAMPLECOUNT; i++){
			summedValue -= sampleData[i];
		}

		//Moving sample data
		for(i = NEW_SAMPLECOUNT; i < SAMPLECOUNT; i++){
			sampleData[i - NEW_SAMPLECOUNT] = sampleData[i];
		}

		sampleCount -= NEW_SAMPLECOUNT;
	}

	schedule_delayed_work(&p_ald->work, msecs_to_jiffies(500));

}


/****************************************/
//Attributes
//Light value

static ssize_t show_light(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", averageValue);
}
static DEVICE_ATTR(light_input, S_IRUGO, show_light, NULL);

//Dummy temperature
static ssize_t show_temperature(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", dummyTemperature);
}
//static DEVICE_ATTR(temperature_input, S_IRUGO, show_temperature, NULL);

//Dummy humidity
static ssize_t show_humidity(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", dummyHumidity);
}

//static DEVICE_ATTR(humidity_input, S_IRUGO, show_humidity, NULL);


static struct attribute *p_als_attributes[] = {
	&dev_attr_light_input.attr,
//	&dev_attr_temperature_input.attr,
//	&dev_attr_humidity_input.attr,
	NULL
};

static const struct attribute_group p_als_attr_group = {
	.attrs = p_als_attributes,
};

static int p_als_regist_init(void)
{
	u8 ret;
	ret = p_als_write(0x00,0x00);
	ret = p_als_write(0x00,0xA0);
	if(ret < 0)
	{
		printk("[als_regist_init] failed write of 0x00, ret = %d\n", ret);
	}

	ret = p_als_write(0x01,0x00);

	if(ret < 0)
	{
		printk("[als_regist_init] failed write of 0x01, ret = %d\n", ret);
	}

	mdelay(500);
	pr_alert("[als_regist_init] successfully ...\n");
	return 0;
}
static int p_als_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	int err;
	g_client = client;
	pr_alert("als probe addr %x\n", g_client->addr);
	p_als_regist_init();

	p_ald = kzalloc(sizeof(struct als_data), GFP_KERNEL);
	INIT_DELAYED_WORK(&p_ald->work, p_als_work_func);
	//err = p_als_input_init(isl29023);
	//if (err < 0)
	//	goto error_1;
	//err = misc_register(&p_als_misc);
	//if (err < 0)
	//	goto error_1;

	err = sysfs_create_group(&client->dev.kobj, &p_als_attr_group);

	if (err)
		goto error_1;
	printk(KERN_INFO "als driver successfully probed %x\n", g_client->addr);

	schedule_delayed_work(&p_ald->work, msecs_to_jiffies(500));
	return 0;
error_1:
	kfree(p_ald);
   	printk(KERN_INFO "als driver successfully probed fail!!!!!!!!!!\n");

	return err;
}



static int p_als_remove(struct i2c_client *client)
{
	cancel_delayed_work(&p_ald->work);

	sysfs_remove_group(&client->dev.kobj, &p_als_attr_group);
	kfree(p_ald);
	return 0;
}

static struct i2c_device_id p_als_id[] = {
	{ "als", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, p_als_id);

#ifdef CONFIG_PM
static int p_als_runtime_suspend(struct device *dev) {
	p_als_write(0x00,0x00);
	cancel_delayed_work(&p_ald->work);
	printk("als_runtime_suspend: SUSPENDED !\n");
	return 0;
}

static int p_als_runtime_resume(struct device *dev) {
	p_als_write(0x00,0xA0);
	schedule_delayed_work(&p_ald->work, msecs_to_jiffies(1000));
	return 0;
}

static const struct dev_pm_ops p_als_pm_ops = {
	.runtime_suspend = p_als_runtime_suspend,
	.runtime_resume = p_als_runtime_resume,
};

#define P_ALS_PM_OPS (&p_als_pm_ops)
#else	/* CONFIG_PM */
#define P_ALS_PM_OPS NULL
#endif	/* CONFIG_PM */

static struct i2c_driver p_als_driver = {
	.driver = {
		.name = "als",
		.pm = P_ALS_PM_OPS,
	},
	.probe = p_als_probe,
	.remove = p_als_remove,
	.id_table = p_als_id,
}
;
module_i2c_driver(p_als_driver);
MODULE_LICENSE("GPL v2");


