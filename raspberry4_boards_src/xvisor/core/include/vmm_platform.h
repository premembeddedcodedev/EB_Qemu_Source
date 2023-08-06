/**
 * Copyright (c) 2016 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file vmm_platform.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Platform bus interface header
 */

#ifndef __VMM_PLATFORM_H_
#define __VMM_PLATFORM_H_

#include <vmm_types.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>

/** Forward declaration of platform bus */
extern struct vmm_bus platform_bus;

/** Bind device pins
 *  Note: The device driver framework only provide dummy weak
 *  implementation of this function which does nothing.
 *  Note: The pinctrl framework will provide complete implementation
 *  of this function. If pinctrl framework is not available then
 *  this function will do nothing.
 */
int vmm_platform_pinctrl_bind(struct vmm_device *dev);

/** Init device pins
 *  Note: The device driver framework only provide dummy weak
 *  implementation of this function which does nothing.
 *  Note: The pinctrl framework will provide complete implementation
 *  of this function. If pinctrl framework is not available then
 *  this function will do nothing.
 */
int vmm_platform_pinctrl_init(struct vmm_device *dev);

/** Find matching nodeid for given platform device */
const struct vmm_devtree_nodeid *vmm_platform_match_nodeid(
						struct vmm_device *dev);

/** Find platform device by node */
struct vmm_device *vmm_platform_find_device_by_node(
					struct vmm_devtree_node *np);


/** Probe device instances under a given device tree node */
int vmm_platform_probe(struct vmm_devtree_node *node);

#endif /* __VMM_PLATFORM_H_ */
