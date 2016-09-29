/*
 * MSM CPU Frequency Limiter Driver
 *
 * Copyright (c) 2013-2014, Dorimanx <yuri@bynet.co.il>
 * Copyright (c) 2013-2016, Pranav Vashi <neobuddy89@gmail.com>
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

/* Try not to change below values. */
#define MSM_LIMITER			"msm_limiter"
#define MSM_LIMITER_MAJOR		5
#define MSM_LIMITER_MINOR		3

/* Recommended to set below values from userspace. */
#define FREQ_CONTROL			0
#define DEBUG_MODE			0
#define MPD_ENABLED			0

/*
 * Define SOC freq limits below.
 * NOTE: We do not set min freq on resume because it will
 * conflict CPU boost driver on resume. Changing resume_max_freq
 * will reflect new max freq. Changing suspend_min_freq will reflect
 * new min freq. All frequency changes do require freq_control enabled.
 * Changing scaling_governor will reflect new governor.
 * Passing single value to above parameters will apply that value to 
 * all the CPUs present. Otherwise, you can pass value in token:value
 * pair to apply value individually.
 */

#if defined(CONFIG_ARCH_MSM8916)
#define DEFAULT_SUSP_MAX_FREQUENCY	998400
#elif defined(CONFIG_ARCH_APQ8084)
#define DEFAULT_SUSP_MAX_FREQUENCY	1728000
#endif
#if defined(CONFIG_ARCH_MSM8916)
#define DEFAULT_RESUME_MAX_FREQUENCY	1209600
#elif defined(CONFIG_ARCH_APQ8084)
#define DEFAULT_RESUME_MAX_FREQUENCY	2649600
#endif
#if defined(CONFIG_ARCH_MSM8916)
#define DEFAULT_MIN_FREQUENCY		200000
#elif defined(CONFIG_ARCH_APQ8084)
#define DEFAULT_MIN_FREQUENCY		300000
#endif

static struct notifier_block notif;
static unsigned int freq_control = FREQ_CONTROL;
static unsigned int debug_mask = DEBUG_MODE;
unsigned int mpd_enabled = MPD_ENABLED;

struct cpu_limit {
	unsigned int suspend_max_freq;
	unsigned int resume_max_freq;
	unsigned int suspend_min_freq;
	struct mutex msm_limiter_mutex;
};

static DEFINE_PER_CPU(struct cpu_limit, limit);

#define dprintk(msg...)		\
do { 				\
	if (debug_mask)		\
		pr_info(msg);	\
} while (0)

static void update_cpu_max_freq(unsigned int cpu)
{
	uint32_t max_freq;

	if (state_suspended)
		max_freq = per_cpu(limit, cpu).suspend_max_freq;
	else
		max_freq = per_cpu(limit, cpu).resume_max_freq;

	if (!max_freq)
		return;

	mutex_lock(&per_cpu(limit, cpu).msm_limiter_mutex);
	dprintk("%s: Setting Max Freq for CPU%u: %u Hz\n",
			MSM_LIMITER, cpu, max_freq);
	cpufreq_set_freq(max_freq, 0, cpu);
	mutex_unlock(&per_cpu(limit, cpu).msm_limiter_mutex);
}

static void update_cpu_min_freq(unsigned int cpu)
{
	uint32_t min_freq = per_cpu(limit, cpu).suspend_min_freq;

	if (!min_freq)
		return;

	if (state_suspended && min_freq	> per_cpu(limit, cpu).suspend_max_freq)
		return;

	mutex_lock(&per_cpu(limit, cpu).msm_limiter_mutex);
	dprintk("%s: Setting Min Freq for CPU%u: %u Hz\n",
			MSM_LIMITER, cpu, min_freq);
	cpufreq_set_freq(0, min_freq, cpu);
	mutex_unlock(&per_cpu(limit, cpu).msm_limiter_mutex);
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
	if (!freq_control)
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

	notif.notifier_call = state_notifier_callback;
	if (state_register_client(&notif)) {
		pr_err("%s: Failed to register State notifier callback\n",
			MSM_LIMITER);
		goto err_out;
	}

	for_each_possible_cpu(cpu)
		mutex_init(&per_cpu(limit, cpu).msm_limiter_mutex);

	for_each_possible_cpu(cpu) {
		update_cpu_max_freq(cpu);
		update_cpu_min_freq(cpu);
	}

	return ret;
err_out:
	freq_control = 0;
	return ret;
}

static void msm_limiter_stop(void)
{
	unsigned int cpu = 0;

	for_each_possible_cpu(cpu)	
		mutex_destroy(&per_cpu(limit, cpu).msm_limiter_mutex);

	state_unregister_client(&notif);
	notif.notifier_call = NULL;
}

static ssize_t freq_control_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", freq_control);
}

static ssize_t freq_control_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1 || val < 0 || val > 1)
		return -EINVAL;

	if (val == freq_control)
		return count;

	freq_control = val;

	if (freq_control)
		msm_limiter_start();
	else
		msm_limiter_stop();

	return count;
}

static ssize_t mpd_enabled_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", mpd_enabled);
}

static ssize_t mpd_enabled_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u\n", &val);
	if (ret != 1 || val < 0 || val > 1)
		return -EINVAL;

	if (val == mpd_enabled)
		return count;

	mpd_enabled = val;

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

static ssize_t set_resume_max_freq(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int i, ntokens = 0;
	unsigned int val, cpu;
	const char *cp = buf;

	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	/* single number: apply to all CPUs */
	if (!ntokens) {
		if (sscanf(buf, "%u\n", &val) != 1)
			return -EINVAL;
		for_each_possible_cpu(i) {
			per_cpu(limit, i).resume_max_freq =
				max(val, per_cpu(limit, i).suspend_min_freq);
			if (freq_control)
				update_cpu_max_freq(i);
		}

		return count;
	}

	/* CPU:value pair */
	if (!(ntokens % 2))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < ntokens; i += 2) {
		if (sscanf(cp, "%u:%u", &cpu, &val) != 2)
			return -EINVAL;
		if (cpu > num_possible_cpus())
			return -EINVAL;

		per_cpu(limit, cpu).resume_max_freq =
			max(val, per_cpu(limit, cpu).suspend_min_freq);

		if (freq_control)
			update_cpu_max_freq(cpu);

		cp = strchr(cp, ' ');
		cp++;
	}

	return count;
}

static ssize_t get_resume_max_freq(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cnt = 0, cpu;

	for_each_possible_cpu(cpu)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, per_cpu(limit, cpu).resume_max_freq);

	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static struct kobj_attribute resume_max_freq =
	__ATTR(resume_max_freq, 0644,
		get_resume_max_freq,
		set_resume_max_freq);

static ssize_t set_suspend_max_freq(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int i, ntokens = 0;
	unsigned int val, cpu;
	const char *cp = buf;

	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	/* single number: apply to all CPUs */
	if (!ntokens) {
		if (sscanf(buf, "%u\n", &val) != 1)
			return -EINVAL;
		for_each_possible_cpu(i)
			per_cpu(limit, i).suspend_max_freq =
				max(val, per_cpu(limit, i).suspend_min_freq);

		return count;
	}

	/* CPU:value pair */
	if (!(ntokens % 2))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < ntokens; i += 2) {
		if (sscanf(cp, "%u:%u", &cpu, &val) != 2)
			return -EINVAL;
		if (cpu > num_possible_cpus())
			return -EINVAL;

		per_cpu(limit, cpu).suspend_max_freq =
			max(val, per_cpu(limit, cpu).suspend_min_freq);

		cp = strchr(cp, ' ');
		cp++;
	}

	return count;
}

static ssize_t get_suspend_max_freq(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cnt = 0, cpu;

	for_each_possible_cpu(cpu)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, per_cpu(limit, cpu).suspend_max_freq);

	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static struct kobj_attribute suspend_max_freq =
	__ATTR(suspend_max_freq, 0644,
		get_suspend_max_freq,
		set_suspend_max_freq);

static ssize_t set_suspend_min_freq(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int i, ntokens = 0;
	unsigned int val, cpu;
	const char *cp = buf;

	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	/* single number: apply to all CPUs */
	if (!ntokens) {
		if (sscanf(buf, "%u\n", &val) != 1)
			return -EINVAL;
		for_each_possible_cpu(i) {
			per_cpu(limit, i).suspend_min_freq =
				min(val, per_cpu(limit, i).resume_max_freq);
			if (freq_control)
				update_cpu_min_freq(i);
		}

		return count;
	}

	/* CPU:value pair */
	if (!(ntokens % 2))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < ntokens; i += 2) {
		if (sscanf(cp, "%u:%u", &cpu, &val) != 2)
			return -EINVAL;
		if (cpu > num_possible_cpus())
			return -EINVAL;

		per_cpu(limit, cpu).suspend_min_freq =
			min(val, per_cpu(limit, cpu).resume_max_freq);

		if (freq_control)
			update_cpu_min_freq(cpu);

		cp = strchr(cp, ' ');
		cp++;
	}

	return count;
}

static ssize_t get_suspend_min_freq(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cnt = 0, cpu;

	for_each_possible_cpu(cpu)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, per_cpu(limit, cpu).suspend_min_freq);

	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static struct kobj_attribute suspend_min_freq =
	__ATTR(suspend_min_freq, 0644,
		get_suspend_min_freq,
		set_suspend_min_freq);

static ssize_t set_scaling_governor(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int i, ntokens = 0;
	unsigned int cpu;
	char val[16];
	const char *cp = buf;

	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	/* single value: apply to all CPUs */
	if (!ntokens) {
		if (sscanf(buf, "%s\n", val) != 1)
			return -EINVAL;
		for_each_possible_cpu(i)
			cpufreq_set_gov(val, i);

		return count;
	}

	/* CPU:value pair */
	if (!(ntokens % 2))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < ntokens; i += 2) {
		if (sscanf(cp, "%u:%s", &cpu, val) != 2)
			return -EINVAL;
		if (cpu > num_possible_cpus())
			return -EINVAL;

		cpufreq_set_gov(val, cpu);

		cp = strchr(cp, ' ');
		cp++;
	}

	return count;
}

static ssize_t get_scaling_governor(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cnt = 0, cpu;

	for_each_possible_cpu(cpu)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%s ", cpu, cpufreq_get_gov(cpu));

	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static struct kobj_attribute scaling_governor =
	__ATTR(scaling_governor, 0644,
		get_scaling_governor,
		set_scaling_governor);

static ssize_t get_live_max_freq(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cnt = 0, cpu;

	for_each_possible_cpu(cpu)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, cpufreq_get_max(cpu));

	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static struct kobj_attribute live_max_freq =
	__ATTR(live_max_freq, 0444,
		get_live_max_freq, NULL);

static ssize_t get_live_min_freq(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cnt = 0, cpu;

	for_each_possible_cpu(cpu)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, cpufreq_get_min(cpu));

	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static struct kobj_attribute live_min_freq =
	__ATTR(live_min_freq, 0444,
		get_live_min_freq, NULL);

static ssize_t get_live_cur_freq(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cnt = 0, cpu;

	for_each_possible_cpu(cpu)
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, cpufreq_quick_get(cpu));

	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static struct kobj_attribute live_cur_freq =
	__ATTR(live_cur_freq, 0444,
		get_live_cur_freq, NULL);

static ssize_t msm_limiter_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "version: %u.%u\n",
			MSM_LIMITER_MAJOR, MSM_LIMITER_MINOR);
}

static struct kobj_attribute msm_limiter_version =
	__ATTR(msm_limiter_version, 0444,
		msm_limiter_version_show,
		NULL);

static struct kobj_attribute freq_control_attr =
	__ATTR(freq_control, 0644,
		freq_control_show,
		freq_control_store);

static struct kobj_attribute mpd_enabled_attr =
	__ATTR(mpd_enabled, 0644,
		mpd_enabled_show,
		mpd_enabled_store);

static struct kobj_attribute debug_mask_attr =
	__ATTR(debug_mask, 0644,
		debug_mask_show,
		debug_mask_store);

static struct attribute *msm_limiter_attrs[] =
	{
		&freq_control_attr.attr,
		&mpd_enabled_attr.attr,
		&debug_mask_attr.attr,
		&suspend_max_freq.attr,
		&resume_max_freq.attr,
		&suspend_min_freq.attr,
		&scaling_governor.attr,
		&live_max_freq.attr,
		&live_min_freq.attr,
		&live_cur_freq.attr,
		&msm_limiter_version.attr,
		NULL,
	};

static struct attribute_group msm_limiter_attr_group =
	{
		.attrs = msm_limiter_attrs,
	};

static struct kobject *msm_limiter_kobj;

static int msm_limiter_init(void)
{
	int ret, cpu;

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

	/* One-time init of required values. */
	for_each_possible_cpu(cpu) {		
#if defined(CONFIG_ARCH_MSM8916) || defined(CONFIG_ARCH_APQ8084)
		per_cpu(limit, cpu).suspend_max_freq = DEFAULT_SUSP_MAX_FREQUENCY;
		per_cpu(limit, cpu).resume_max_freq = DEFAULT_RESUME_MAX_FREQUENCY;
		per_cpu(limit, cpu).suspend_min_freq = DEFAULT_MIN_FREQUENCY;
#else
		per_cpu(limit, cpu).suspend_max_freq =  cpuinfo_get_max(cpu);
		per_cpu(limit, cpu).resume_max_freq =  cpuinfo_get_max(cpu);
		per_cpu(limit, cpu).suspend_min_freq =  cpuinfo_get_min(cpu);
#endif
	}

	if (freq_control)
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

	if (freq_control)
		msm_limiter_stop();

}

late_initcall(msm_limiter_init);
module_exit(msm_limiter_exit);

MODULE_AUTHOR("Dorimanx <yuri@bynet.co.il>");
MODULE_AUTHOR("Pranav Vashi <neobuddy89@gmail.com>");
MODULE_DESCRIPTION("MSM CPU Frequency Limiter Driver");
MODULE_LICENSE("GPL v2");
