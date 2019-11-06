// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 Angelo Compagnucci <angelo.compagnucci@gmail.com>
 *
 * servo-pwm.c - driver for controlling servo motors via pwm.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/slab.h>

#define DEFAULT_PERIOD		2000000
#define DEFAULT_DUTY_0		50000
#define DEFAULT_DUTY_180	250000
#define DEFAULT_ANGLE		0

struct servo_pwm_data {
	u32 duty_0;
	u32 duty_180;
	u32 period;
	u32 angle;
	struct mutex lock;
	struct pwm_device *pwm;
};

static int servo_pwm_set(struct servo_pwm_data *servo_data)
{
	u32 new_duty = (servo_data->duty_180 - servo_data->duty_0) /
			180 * servo_data->angle + servo_data->duty_0;
	int ret;

	ret = pwm_config(servo_data->pwm, new_duty, servo_data->period);

	return ret;
}

static ssize_t angle_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct servo_pwm_data *servo_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", servo_data->angle);
}

static ssize_t angle_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct servo_pwm_data *servo_data = dev_get_drvdata(dev);
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret < 0)
		return -EINVAL;

	if (val > 180)
		return -EINVAL;

	mutex_lock(&servo_data->lock);

	servo_data->angle = val;

	ret = servo_pwm_set(servo_data);
	if (ret) {
		mutex_unlock(&servo_data->lock);
		return ret;
	}

	mutex_unlock(&servo_data->lock);

	return count;
}

static DEVICE_ATTR_RW(angle);

static struct attribute *servo_pwm_attrs[] = {
	&dev_attr_angle.attr,
	NULL,
};

ATTRIBUTE_GROUPS(servo_pwm);

static int servo_pwm_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct servo_pwm_data *servo_data;
	struct pwm_args pargs;
	int ret = 0;

	servo_data = devm_kzalloc(&pdev->dev, sizeof(*servo_data), GFP_KERNEL);
	if (!servo_data)
		return -ENOMEM;

	if (!of_property_read_u32(node, "duty-0", &servo_data->duty_0) == 0)
		servo_data->duty_0 = DEFAULT_DUTY_0;

	if (!of_property_read_u32(node, "duty-180", &servo_data->duty_180) == 0)
		servo_data->duty_180 = DEFAULT_DUTY_180;

	if (!of_property_read_u32(node, "angle", &servo_data->angle) == 0)
		servo_data->angle = DEFAULT_ANGLE;

	servo_data->pwm = devm_of_pwm_get(&pdev->dev, node, NULL);
	if (IS_ERR(servo_data->pwm)) {
		ret = PTR_ERR(servo_data->pwm);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "unable to request pwm\n");
		return ret;
	}

	pwm_apply_args(servo_data->pwm);

	pwm_get_args(servo_data->pwm, &pargs);

	servo_data->period = pargs.period;

	if (!servo_data->period)
		servo_data->period = DEFAULT_PERIOD;

	ret = servo_pwm_set(servo_data);
	if (ret) {
		dev_err(&pdev->dev, "cannot configure servo: %d\n", ret);
		return ret;
	}

	ret = pwm_enable(servo_data->pwm);
	if (ret) {
		dev_err(&pdev->dev, "cannot enable servo: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, servo_data);

	ret = devm_device_add_groups(&pdev->dev, servo_pwm_groups);
	if (ret) {
		dev_err(&pdev->dev, "error creating sysfs groups: %d\n", ret);
		return ret;
	}

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
		.name	= "servo-pwm",
		.of_match_table = of_servo_pwm_match,
	},
};

module_platform_driver(servo_pwm_driver);

MODULE_AUTHOR("Angelo Compagnucci <angelo.compagnucci@gmail.com>");
MODULE_DESCRIPTION("generic PWM servo motor driver");
MODULE_LICENSE("GPL");
