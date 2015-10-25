/*
 * MSM CPU Frequency Limiter Driver
 *
 * Copyright (c) 2013-2014, Dorimanx <yuri@bynet.co.il>
 * Copyright (c) 2013-2015, Pranav Vashi <neobuddy89@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/state_notifier.h>

#include <soc/qcom/limiter.h>

#define MSM_LIMITER_MAJOR		4
#define MSM_LIMITER_MINOR		0

static unsigned int debug_mask;

#define dprintk(msg...)		\
do { 				\
	if (debug_mask)		\
		pr_info(msg);	\
} while (0)

static void update_cpu_max_freq(unsigned int cpu)
{
	uint32_t max_freq;

	if (state_suspended)
		max_freq = limit.suspend_max_freq;
	else
		max_freq = limit.resume_max_freq[cpu];

	if (!max_freq)
		return;

	mutex_lock(&limit.msm_limiter_mutex[cpu]);
	dprintk("%s: Setting Max Freq for CPU%u: %u Hz\n",
			MSM_LIMITER, cpu, max_freq);
	cpufreq_set_freq(max_freq, 0, cpu);
	mutex_unlock(&limit.msm_limiter_mutex[cpu]);
}

static void update_cpu_min_freq(unsigned int cpu)
{
	uint32_t min_freq = limit.suspend_min_freq[cpu];

	if (!min_freq)
		return;

	if (state_suspended && min_freq	> limit.suspend_max_freq)
		return;

	mutex_lock(&limit.msm_limiter_mutex[cpu]);
	dprintk("%s: Setting Min Freq for CPU%u: %u Hz\n",
			MSM_LIMITER, cpu, min_freq);
	cpufreq_set_freq(0, min_freq, cpu);
	mutex_unlock(&limit.msm_limiter_mutex[cpu]);
}

static void msm_limiter_run(void)
{
	int cpu = 0;

	for_each_possible_cpu(cpu) {
		update_cpu_max_freq(cpu);

		if (state_suspended)
			update_cpu_min_freq(cpu);
	}
}

static int state_notifier_callback(struct notifier_block *this,
				unsigned long event, void *data)
{
	if (!limit.limiter_enabled)
		return NOTIFY_OK;

	switch (event) {
		case STATE_NOTIFIER_ACTIVE:
		case STATE_NOTIFIER_SUSPEND:
			msm_limiter_run();
			break;
		default:
			break;
	}

	return NOTIFY_OK;
}

static int msm_limiter_start(void)
{
	unsigned int cpu = 0;
	int ret = 0;

	limit.notif.notifier_call = state_notifier_callback;
	if (state_register_client(&limit.notif)) {
		pr_err("%s: Failed to register State notifier callback\n",
			MSM_LIMITER);
		goto err_out;
	}

	for_each_possible_cpu(cpu)
		mutex_init(&limit.msm_limiter_mutex[cpu]);

	for_each_possible_cpu(cpu) {
		update_cpu_max_freq(cpu);
		update_cpu_min_freq(cpu);
	}

	return ret;
err_out:
	limit.limiter_enabled = 0;
	return ret;
}

static void msm_limiter_stop(void)
{
	unsigned int cpu = 0;

	for_each_possible_cpu(cpu)	
		mutex_destroy(&limit.msm_limiter_mutex[cpu]);

	state_unregister_client(&limit.notif);
	limit.notif.notifier_call = NULL;
}

static ssize_t limiter_enabled_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", limit.limiter_enabled);
}

static ssize_t limiter_enabled_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1 || val < 0 || val > 1)
		return -EINVAL;

	if (val == limit.limiter_enabled)
		return count;

	limit.limiter_enabled = val;

	if (limit.limiter_enabled)
		msm_limiter_start();
	else
		msm_limiter_stop();

	return count;
}

static ssize_t debug_mask_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", debug_mask);
}

static ssize_t debug_mask_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1 || val < 0 || val > 1)
		return -EINVAL;

	if (val == debug_mask)
		return count;

	debug_mask = val;

	return count;
}


static ssize_t suspend_max_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", limit.suspend_max_freq);
}

static ssize_t suspend_max_freq_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1)
		return -EINVAL;

	if (val == 0)
		goto out;

	if (val == limit.suspend_max_freq)
		return count;

out:
	limit.suspend_max_freq = val;

	return count;
}

static ssize_t store_resume_max_freq_all(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	unsigned int val, cpu;
	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1)
		return -EINVAL;
	if (val == 0)
		goto out;

	if (val < limit.suspend_min_freq_all)
		val = limit.suspend_min_freq_all;

out:
	limit.resume_max_freq_all = val;
	for_each_possible_cpu(cpu) {
		limit.resume_max_freq[cpu] = val;
		if (limit.limiter_enabled)
			update_cpu_max_freq(cpu);
	}
	return count;
}

static ssize_t store_suspend_min_freq_all(struct kobject *kobj,
					struct kobj_attribute *attr,
 					const char *buf, size_t count)
{
	int ret;
	unsigned int val, cpu;
	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1)
		return -EINVAL;
	if (val == 0)
		goto out;
	if (val > limit.resume_max_freq_all)
		val = limit.resume_max_freq_all;

out:
	limit.suspend_min_freq_all = val;
	for_each_possible_cpu(cpu) {
		limit.suspend_min_freq[cpu] = val;
		if (limit.limiter_enabled)
			update_cpu_min_freq(cpu);
	}
	return count;
}

static ssize_t store_scaling_governor_all(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int ret, cpu;
	char val[16];
	ret = sscanf(buf, "%s\n", val);
	if (ret != 1)
		return -EINVAL;

	for_each_possible_cpu(cpu)
		ret = cpufreq_set_gov(val, cpu);
	return count;
}

#define multi_cpu(cpu)					\
static ssize_t store_resume_max_freq_##cpu		\
(struct kobject *kobj, 					\
 struct kobj_attribute *attr, 				\
 const char *buf, size_t count)				\
{							\
	int ret;					\
	unsigned int val;				\
	ret = sscanf(buf, "%u\n", &val);		\
	if (ret != 1)					\
		return -EINVAL;				\
	if (val == 0)					\
		goto out;				\
	if (val < limit.suspend_min_freq[cpu])		\
		val = limit.suspend_min_freq[cpu];	\
	if (val == limit.resume_max_freq[cpu])		\
		return count;				\
out:							\
	limit.resume_max_freq[cpu] = val;		\
	if (limit.limiter_enabled)			\
		update_cpu_max_freq(cpu);		\
	return count;					\
}							\
static ssize_t show_resume_max_freq_##cpu		\
(struct kobject *kobj,					\
 struct kobj_attribute *attr, char *buf)		\
{							\
	return sprintf(buf, "%u\n",			\
			limit.resume_max_freq[cpu]);	\
}							\
static ssize_t store_suspend_min_freq_##cpu(		\
 struct kobject *kobj,					\
 struct kobj_attribute *attr,				\
 const char *buf, size_t count)				\
{							\
	int ret;					\
	unsigned int val;				\
	ret = sscanf(buf, "%u\n", &val);		\
	if (ret != 1)					\
		return -EINVAL;				\
	if (val == 0)					\
		goto out;				\
	if (val > limit.resume_max_freq[cpu])		\
		val = limit.resume_max_freq[cpu];	\
	if (val == limit.suspend_min_freq[cpu])		\
		return count;				\
out:							\
	limit.suspend_min_freq[cpu] = val;		\
	if (limit.limiter_enabled)			\
		update_cpu_min_freq(cpu);		\
	return count;					\
}							\
static ssize_t show_suspend_min_freq_##cpu(		\
 struct kobject *kobj,					\
 struct kobj_attribute *attr, char *buf)		\
{							\
	return sprintf(buf, "%u\n",			\
		limit.suspend_min_freq[cpu]);		\
}							\
static ssize_t store_scaling_governor_##cpu(		\
 struct kobject *kobj,					\
 struct kobj_attribute *attr,				\
 const char *buf, size_t count)				\
{							\
	int ret;					\
	char val[16];					\
	ret = sscanf(buf, "%s\n", val);			\
	if (ret != 1)					\
		return -EINVAL;				\
	ret = cpufreq_set_gov(val, cpu);		\
	return count;					\
}							\
static ssize_t show_scaling_governor_##cpu(		\
 struct kobject *kobj,					\
 struct kobj_attribute *attr, char *buf)		\
{							\
	return sprintf(buf, "%s\n",			\
	cpufreq_get_gov(cpu));				\
}							\
static ssize_t show_live_max_freq_##cpu(		\
 struct kobject *kobj,					\
 struct kobj_attribute *attr, char *buf)		\
{							\
	return sprintf(buf, "%u\n",			\
	cpufreq_get_max(cpu));				\
}							\
static ssize_t show_live_min_freq_##cpu(		\
 struct kobject *kobj,					\
 struct kobj_attribute *attr, char *buf)		\
{							\
	return sprintf(buf, "%u\n",			\
	cpufreq_get_min(cpu));				\
}							\
static ssize_t show_live_cur_freq_##cpu(		\
 struct kobject *kobj,					\
 struct kobj_attribute *attr, char *buf)		\
{							\
	return sprintf(buf, "%u\n",			\
	cpufreq_quick_get(cpu));			\
}							\
static struct kobj_attribute resume_max_freq_##cpu =	\
	__ATTR(resume_max_freq_##cpu, 0666,		\
		show_resume_max_freq_##cpu,		\
		store_resume_max_freq_##cpu);		\
static struct kobj_attribute suspend_min_freq_##cpu =	\
	__ATTR(suspend_min_freq_##cpu, 0666,		\
		show_suspend_min_freq_##cpu,		\
		store_suspend_min_freq_##cpu);		\
static struct kobj_attribute scaling_governor_##cpu =	\
	__ATTR(scaling_governor_##cpu, 0666,		\
		show_scaling_governor_##cpu,		\
		store_scaling_governor_##cpu);		\
static struct kobj_attribute live_max_freq_##cpu =	\
	__ATTR(live_max_freq_##cpu, 0666,		\
		show_live_max_freq_##cpu,		\
		store_resume_max_freq_##cpu);		\
static struct kobj_attribute live_min_freq_##cpu =	\
	__ATTR(live_min_freq_##cpu, 0666,		\
		show_live_min_freq_##cpu,		\
		store_suspend_min_freq_##cpu);		\
static struct kobj_attribute live_cur_freq_##cpu =	\
	__ATTR(live_cur_freq_##cpu, 0666,		\
		show_live_cur_freq_##cpu, NULL);	\

multi_cpu(0);
multi_cpu(1);
multi_cpu(2);
multi_cpu(3);

static struct kobj_attribute resume_max_freq =
	__ATTR(resume_max_freq, 0666,
		show_resume_max_freq_0,
		store_resume_max_freq_all);

static struct kobj_attribute suspend_min_freq =
	__ATTR(suspend_min_freq, 0666,
		show_suspend_min_freq_0,
		store_suspend_min_freq_all);

static struct kobj_attribute scaling_governor =
	__ATTR(scaling_governor, 0666,
		show_scaling_governor_0,
		store_scaling_governor_all);

static ssize_t msm_limiter_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "version: %u.%u\n",
			MSM_LIMITER_MAJOR, MSM_LIMITER_MINOR);
}

static struct kobj_attribute msm_limiter_version_attribute =
	__ATTR(msm_limiter_version, 0444,
		msm_limiter_version_show,
		NULL);

static struct kobj_attribute limiter_enabled_attribute =
	__ATTR(limiter_enabled, 0666,
		limiter_enabled_show,
		limiter_enabled_store);

static struct kobj_attribute debug_mask_attribute =
	__ATTR(debug_mask, 0666,
		debug_mask_show,
		debug_mask_store);

static struct kobj_attribute suspend_max_freq_attribute =
	__ATTR(suspend_max_freq, 0666,
		suspend_max_freq_show,
		suspend_max_freq_store);

static struct attribute *msm_limiter_attrs[] =
	{
		&limiter_enabled_attribute.attr,
		&debug_mask_attribute.attr,
		&suspend_max_freq_attribute.attr,
		&resume_max_freq.attr,
		&resume_max_freq_0.attr,
		&resume_max_freq_1.attr,
		&resume_max_freq_2.attr,
		&resume_max_freq_3.attr,
		&suspend_min_freq.attr,
		&suspend_min_freq_0.attr,
		&suspend_min_freq_1.attr,
		&suspend_min_freq_2.attr,
		&suspend_min_freq_3.attr,
		&scaling_governor.attr,
		&scaling_governor_0.attr,
		&scaling_governor_1.attr,
		&scaling_governor_2.attr,
		&scaling_governor_3.attr,
		&live_max_freq_0.attr,
		&live_max_freq_1.attr,
		&live_max_freq_2.attr,
		&live_max_freq_3.attr,
		&live_min_freq_0.attr,
		&live_min_freq_1.attr,
		&live_min_freq_2.attr,
		&live_min_freq_3.attr,
		&live_cur_freq_0.attr,
		&live_cur_freq_1.attr,
		&live_cur_freq_2.attr,
		&live_cur_freq_3.attr,
		&msm_limiter_version_attribute.attr,
		NULL,
	};

static struct attribute_group msm_limiter_attr_group =
	{
		.attrs = msm_limiter_attrs,
	};

static struct kobject *msm_limiter_kobj;

static int msm_limiter_init(void)
{
	int ret;

	msm_limiter_kobj =
		kobject_create_and_add(MSM_LIMITER, kernel_kobj);
	if (!msm_limiter_kobj) {
		pr_err("%s: kobject create failed!\n",
			MSM_LIMITER);
		return -ENOMEM;
        }

	ret = sysfs_create_group(msm_limiter_kobj,
			&msm_limiter_attr_group);
        if (ret) {
		pr_err("%s: sysfs create failed!\n",
			MSM_LIMITER);
		goto err_dev;
	}

	if (limit.limiter_enabled)
		msm_limiter_start();

	return ret;
err_dev:
	if (msm_limiter_kobj != NULL)
		kobject_put(msm_limiter_kobj);
	return ret;
}

static void msm_limiter_exit(void)
{
	if (msm_limiter_kobj != NULL)
		kobject_put(msm_limiter_kobj);

	if (limit.limiter_enabled)
		msm_limiter_stop();

}

late_initcall(msm_limiter_init);
module_exit(msm_limiter_exit);

MODULE_AUTHOR("Dorimanx <yuri@bynet.co.il>");
MODULE_AUTHOR("Pranav Vashi <neobuddy89@gmail.com>");
MODULE_DESCRIPTION("MSM CPU Frequency Limiter Driver");
MODULE_LICENSE("GPL v2");
