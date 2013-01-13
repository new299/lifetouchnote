/* include/linux/timed_output.h
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2011 NEC Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#ifndef _LINUX_TIMED_OUTPUT_H
#define _LINUX_TIMED_OUTPUT_H

//add by 
#include <../../../../arch/arm/mach-tegra/include/nvodm_vibrate.h>
//end 

struct timed_output_dev {
	const char	*name;

	/* enable the output and set the timer */
	void	(*enable)(struct timed_output_dev *sdev, int timeout);

	/* returns the current number of milliseconds remaining on the timer */
	int		(*get_time)(struct timed_output_dev *sdev);

	//add by 
	void    (*findex)(struct timed_output_dev *sdev, int temp);

	/* returns the current index of PWM value */
	int		(*get_index)(struct timed_output_dev *sdev);
	//end 

	/* private data */
	struct device	*dev;
	int		index;
	int		state;
	//add by 
	int		vibrateTime;
	int		currentIndex;
	//end 
};

extern int timed_output_dev_register(struct timed_output_dev *dev);
extern void timed_output_dev_unregister(struct timed_output_dev *dev);

#endif
