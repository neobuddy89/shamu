#ifndef __LINUX_STATE_NOTIFIER_H
#define __LINUX_STATE_NOTIFIER_H

#include <linux/notifier.h>

#define STATE_NOTIFIER_UNKNOWN		0x01
#define STATE_NOTIFIER_ACTIVE		0x02
#define STATE_NOTIFIER_SUSPEND		0x03
#define STATE_NOTIFIER_STANDBY		0x04
#define STATE_NOTIFIER_BL		0x05
#define STATE_NOTIFIER_INIT		0x06
#define STATE_NOTIFIER_FLASH		0x07
#define STATE_NOTIFIER_QUERY		0x08
#ifdef CONFIG_WAKE_GESTURES
#define STATE_NOTIFIER_WG		0x09
#endif
#define STATE_NOTIFIER_INVALID		0x10

struct state_event {
	void *data;
};

int state_register_client(struct notifier_block *nb);
int state_unregister_client(struct notifier_block *nb);
int state_notifier_call_chain(unsigned long val, void *v);

#endif /* _LINUX_STATE_NOTIFIER_H */
