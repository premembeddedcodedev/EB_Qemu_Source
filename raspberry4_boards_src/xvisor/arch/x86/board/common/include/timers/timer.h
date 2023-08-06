/**
 * Copyright (c) 2018 Himanshu Chauhan.
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
 * @file timers.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief External Timer interface File
 */

#ifndef _TIMERS_H_
#define _TIMERS_H_

struct x86_system_timer_ops {
	int (*sys_cc_init)(void);
	int (*sys_cs_init)(void);
};

int x86_timer_init(void);
void x86_register_system_timer_ops(struct x86_system_timer_ops *ops);

#endif
