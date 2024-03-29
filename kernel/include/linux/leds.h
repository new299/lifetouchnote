/*
 * Driver model for leds and led triggers
 *
 * Copyright (C) 2005 John Lenz <lenz@cs.wisc.edu>
 * Copyright (C) 2005 Richard Purdie <rpurdie@openedhand.com>
 * Copyright (C) 2011 NEC Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LINUX_LEDS_H_INCLUDED
#define __LINUX_LEDS_H_INCLUDED

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>

struct device;
/*
 * LED Core
 */

enum led_brightness {
	LED_OFF		= 0,
	LED_HALF	= 127,
	LED_FULL	= 255,
};

//add by 
enum led_blink_mode {
	BLINK_MODE_OFF		= 0,
	BLINK_MODE_ON1_OFF1	= 1,
	BLINK_MODE_ON1_OFF2	= 2,
};
//end 

//for MTP,+++
enum led_test_mode {
	TEST_MODE_OFF	= 0,
	TEST_MODE_ON	= 1,
};
//---


struct led_classdev {
	const char		*name;
	int			 brightness;
	int			 max_brightness;
	int			 flags;
	int          blink_mode; //
	int			 testMode_OnOff; //

	/* Lower 16 bits reflect status */
#define LED_SUSPENDED		(1 << 0)
	/* Upper 16 bits reflect control information */
#define LED_CORE_SUSPENDRESUME	(1 << 16)

	/* Set LED brightness level */
	/* Must not sleep, use a workqueue if needed */
	void		(*brightness_set)(struct led_classdev *led_cdev,
					  enum led_brightness brightness);
	/* Get LED brightness level */
	enum led_brightness (*brightness_get)(struct led_classdev *led_cdev);

	/* Activate hardware accelerated blink, delays are in
	 * miliseconds and if none is provided then a sensible default
	 * should be chosen. The call can adjust the timings if it can't
	 * match the values specified exactly. */
	int		(*blink_set)(struct led_classdev *led_cdev,
				     unsigned long *delay_on,
				     unsigned long *delay_off);

    //add by 
    void		(*blink_mode_set)(struct led_classdev *led_cdev,
					  enum led_blink_mode blink);
    enum led_blink_mode (*blink_mode_get)(struct led_classdev *led_cdev);
    //end 

	//for MTP test mode,+++
	void (*test_mode_set)(struct led_classdev *led_cdev,enum led_test_mode on_off);
    enum led_test_mode (*test_mode_get)(struct led_classdev *led_cdev);
	//---

	struct device		*dev;
	struct list_head	 node;			/* LED Device list */
	const char		*default_trigger;	/* Trigger to use */

#ifdef CONFIG_LEDS_TRIGGERS
	/* Protects the trigger data below */
	struct rw_semaphore	 trigger_lock;

	struct led_trigger	*trigger;
	struct list_head	 trig_list;
	void			*trigger_data;
#endif
};

extern int led_classdev_register(struct device *parent,
				 struct led_classdev *led_cdev);
extern void led_classdev_unregister(struct led_classdev *led_cdev);
extern void led_classdev_suspend(struct led_classdev *led_cdev);
extern void led_classdev_resume(struct led_classdev *led_cdev);

/*
 * LED Triggers
 */
#ifdef CONFIG_LEDS_TRIGGERS

#define TRIG_NAME_MAX 50

struct led_trigger {
	/* Trigger Properties */
	const char	 *name;
	void		(*activate)(struct led_classdev *led_cdev);
	void		(*deactivate)(struct led_classdev *led_cdev);

	/* LEDs under control by this trigger (for simple triggers) */
	rwlock_t	  leddev_list_lock;
	struct list_head  led_cdevs;

	/* Link to next registered trigger */
	struct list_head  next_trig;
};

/* Registration functions for complex triggers */
extern int led_trigger_register(struct led_trigger *trigger);
extern void led_trigger_unregister(struct led_trigger *trigger);

/* Registration functions for simple triggers */
#define DEFINE_LED_TRIGGER(x)		static struct led_trigger *x;
#define DEFINE_LED_TRIGGER_GLOBAL(x)	struct led_trigger *x;
extern void led_trigger_register_simple(const char *name,
				struct led_trigger **trigger);
extern void led_trigger_unregister_simple(struct led_trigger *trigger);
extern void led_trigger_event(struct led_trigger *trigger,
				enum led_brightness event);

#else

/* Triggers aren't active - null macros */
#define DEFINE_LED_TRIGGER(x)
#define DEFINE_LED_TRIGGER_GLOBAL(x)
#define led_trigger_register_simple(x, y) do {} while(0)
#define led_trigger_unregister_simple(x) do {} while(0)
#define led_trigger_event(x, y) do {} while(0)

#endif

/* Trigger specific functions */
#ifdef CONFIG_LEDS_TRIGGER_IDE_DISK
extern void ledtrig_ide_activity(void);
#else
#define ledtrig_ide_activity() do {} while(0)
#endif

/*
 * Generic LED platform data for describing LED names and default triggers.
 */
struct led_info {
	const char	*name;
	const char	*default_trigger;
	int		flags;
};

struct led_platform_data {
	int		num_leds;
	struct led_info	*leds;
};

/* For the leds-gpio driver */
struct gpio_led {
	const char *name;
	const char *default_trigger;
	unsigned 	gpio;
	unsigned	active_low : 1;
	unsigned	retain_state_suspended : 1;
	unsigned	default_state : 2;
	/* default_state should be one of LEDS_GPIO_DEFSTATE_(ON|OFF|KEEP) */
};
#define LEDS_GPIO_DEFSTATE_OFF	0
#define LEDS_GPIO_DEFSTATE_ON	1
#define LEDS_GPIO_DEFSTATE_KEEP	2

struct gpio_led_platform_data {
	int 		num_leds;
	struct gpio_led *leds;
	int		(*gpio_blink_set)(unsigned gpio,
					unsigned long *delay_on,
					unsigned long *delay_off);
};


#endif		/* __LINUX_LEDS_H_INCLUDED */
