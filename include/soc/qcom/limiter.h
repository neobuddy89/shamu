#define MPD_ENABLED			0

static unsigned int mpd_enabled = MPD_ENABLED;

int cpufreq_set_gov(char *target_gov, unsigned int cpu);
char *cpufreq_get_gov(unsigned int cpu);
int cpufreq_set_freq(unsigned int max_freq, unsigned int min_freq,
			unsigned int cpu);
int cpufreq_get_max(unsigned int cpu);
int cpufreq_get_min(unsigned int cpu);
