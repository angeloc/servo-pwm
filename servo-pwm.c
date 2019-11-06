// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023 Angelo Compagnucci <angelo@amarulasolutions.com>
 * servo-pwm.c - driver for controlling servo motors via pwm.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pwm.h>

#define DEFAULT_DUTY_MIN	500000
#define DEFAULT_DUTY_MAX	2500000
#define DEFAULT_DEGREES		175
#define DEFAULT_ANGLE		0

struct servo_pwm_data {
	u32 duty_min;
	u32 duty_max;
	u32 degrees;
	u32 angle;

	struct mutex lock;
	struct pwm_device *pwm;
	struct pwm_state pwmstate;
};

static int servo_pwm_set(struct servo_pwm_data *data, int val)
{
	u64 new_duty = (((data->duty_max - data->duty_min) /
			data->degrees) * val) + data->duty_min;
	int ret;

	mutex_lock(&data->lock);

	data->pwmstate.duty_cycle = new_duty;
	data->pwmstate.enabled = 1;
	ret = pwm_apply_state(data->pwm, &data->pwmstate);

	if (!ret)
		data->angle = val;

	mutex_unlock(&data->lock);

	return ret;
}

static ssize_t angle_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct servo_pwm_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->angle);
}

static ssize_t angle_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct servo_pwm_data *data = dev_get_drvdata(dev);
	int ret, val = 0;

	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return -EINVAL;

	if (val < 0 || val > data->degrees)
		return -EINVAL;

	ret = servo_pwm_set(data, val);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR_RW(angle);

static ssize_t degrees_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct servo_pwm_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->degrees);
}

static DEVICE_ATTR_RO(degrees);

static struct attribute *servo_pwm_attrs[] = {
	&dev_attr_angle.attr,
	&dev_attr_degrees.attr,
	NULL,
};

ATTRIBUTE_GROUPS(servo_pwm);

static int servo_pwm_probe(struct platform_device *pdev)
{
	struct fwnode_handle *fwnode = dev_fwnode(&pdev->dev);
	struct servo_pwm_data *data;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_init(&data->lock);

	if (!fwnode_property_read_u32(fwnode, "duty-min", &data->duty_min) == 0)
		data->duty_min = DEFAULT_DUTY_MIN;

	if (!fwnode_property_read_u32(fwnode, "duty-max", &data->duty_max) == 0)
		data->duty_max = DEFAULT_DUTY_MAX;

	if (!fwnode_property_read_u32(fwnode, "degrees", &data->degrees) == 0)
		data->degrees = DEFAULT_DEGREES;

	data->pwm = devm_fwnode_pwm_get(&pdev->dev, fwnode, NULL);
	if (IS_ERR(data->pwm)) {
		return dev_err_probe(&pdev->dev, PTR_ERR(data->pwm),
				     "unable to request PWM\n");
	}

	pwm_init_state(data->pwm, &data->pwmstate);

	platform_set_drvdata(pdev, data);

	return 0;
}

static const struct of_device_id of_servo_pwm_match[] = {
	{ .compatible = "servo-pwm", },
	{},
};
MODULE_DEVICE_TABLE(of, of_servo_pwm_match);

static struct platform_driver servo_pwm_driver = {
	.probe		= servo_pwm_probe,
	.driver		= {
		.name			= "servo-pwm",
		.of_match_table 	= of_servo_pwm_match,
		.dev_groups 		= servo_pwm_groups,
	},
};

module_platform_driver(servo_pwm_driver);

MODULE_AUTHOR("Angelo Compagnucci <angelo@amarulasolutions.com>");
MODULE_DESCRIPTION("generic PWM servo motor driver");
MODULE_LICENSE("GPL");
