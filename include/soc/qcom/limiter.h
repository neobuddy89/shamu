#define MSM_LIMITER			"msm_limiter"
#define LIMITER_ENABLED			0
#define MPD_ENABLED			0
#if defined(CONFIG_ARCH_MSM8916)
#define DEFAULT_SUSPEND_FREQUENCY	998400
#else
#define DEFAULT_SUSPEND_FREQUENCY	1728000
#endif
#if defined(CONFIG_ARCH_APQ8084)
#define DEFAULT_RESUME_FREQUENCY	2649600
#elif defined(CONFIG_ARCH_MSM8916)
#define DEFAULT_RESUME_FREQUENCY	1209600
#else
#define DEFAULT_RESUME_FREQUENCY	2265600
#endif
#if defined(CONFIG_ARCH_MSM8916)
#define DEFAULT_MIN_FREQUENCY		200000
#else
#define DEFAULT_MIN_FREQUENCY		300000
#endif

static struct cpu_limit {
	unsigned int limiter_enabled;
	unsigned int mpd_enabled;
	uint32_t suspend_max_freq;
	uint32_t resume_max_freq_all;
	uint32_t resume_max_freq[4];
	uint32_t suspend_min_freq_all;
	uint32_t suspend_min_freq[4];
	struct mutex msm_limiter_mutex[4];
	struct notifier_block notif;
} limit = {
	.limiter_enabled = LIMITER_ENABLED,
	.mpd_enabled = MPD_ENABLED,
	.suspend_max_freq = DEFAULT_SUSPEND_FREQUENCY,
	.resume_max_freq_all = DEFAULT_RESUME_FREQUENCY,
	.resume_max_freq[0] = DEFAULT_RESUME_FREQUENCY,
	.resume_max_freq[1] = DEFAULT_RESUME_FREQUENCY,
	.resume_max_freq[2] = DEFAULT_RESUME_FREQUENCY,
	.resume_max_freq[3] = DEFAULT_RESUME_FREQUENCY,
	.suspend_min_freq_all = DEFAULT_MIN_FREQUENCY,
	.suspend_min_freq[0] = DEFAULT_MIN_FREQUENCY,
	.suspend_min_freq[1] = DEFAULT_MIN_FREQUENCY,
	.suspend_min_freq[2] = DEFAULT_MIN_FREQUENCY,
	.suspend_min_freq[3] = DEFAULT_MIN_FREQUENCY,
};

int cpufreq_set_gov(char *target_gov, unsigned int cpu);
char *cpufreq_get_gov(unsigned int cpu);
int cpufreq_set_freq(unsigned int max_freq, unsigned int min_freq,
			unsigned int cpu);
int cpufreq_get_max(unsigned int cpu);
int cpufreq_get_min(unsigned int cpu);
