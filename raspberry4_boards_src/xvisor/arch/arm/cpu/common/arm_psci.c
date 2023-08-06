/**
 * Copyright (c) 2020 Anup Patel.
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
 * @file arm_psci.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM32, ARM32ve and ARM64 PSCI host calls
 *
 * Adapted from arch/arm/kernel/psci.c
 *
 * Copyright (C) 2012 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_main.h>
#include <vmm_devtree.h>
#include <libs/stringlib.h>

#include <arm_psci.h>

static bool psci_avail = FALSE;

/*
 * While a 64-bit OS can make calls with SMC32 calling conventions, for some
 * calls it is necessary to use SMC64 to pass or return 64-bit values.
 * For such calls PSCI_FN_NATIVE(version, name) will choose the appropriate
 * (native-width) function ID.
 */
#ifdef CONFIG_ARM64
#define PSCI_FN_NATIVE(version, name)	PSCI_##version##_FN64_##name
#else
#define PSCI_FN_NATIVE(version, name)	PSCI_##version##_FN_##name
#endif

enum psci_function {
	PSCI_FN_CPU_SUSPEND,
	PSCI_FN_CPU_ON,
	PSCI_FN_CPU_OFF,
	PSCI_FN_MIGRATE,
	PSCI_FN_MAX,
};

static u32 psci_function_id[PSCI_FN_MAX];

static int psci_to_xvisor_errno(long errno)
{
	switch (errno) {
	case PSCI_RET_SUCCESS:
		return VMM_OK;
	case PSCI_RET_NOT_SUPPORTED:
		return VMM_EOPNOTSUPP;
	case PSCI_RET_INVALID_PARAMS:
		return VMM_EINVALID;
	case PSCI_RET_DENIED:
		return VMM_EACCESS;
	default:
		return VMM_EFAIL;
	};
}

static int __noinline invoke_psci_fn_smc_raw(unsigned long func,
					     unsigned long arg0,
					     unsigned long arg1,
                                             unsigned long arg2)
{
	long ret;

#ifdef CONFIG_ARM64
        asm volatile(
                "mov    x0, %1\n\t"
                "mov    x1, %2\n\t"
                "mov    x2, %3\n\t"
                "mov    x3, %4\n\t"
                "smc    #0    \n\t"
                "mov    %0, x0\n\t"
        : "=r" (ret)
        : "r" (func), "r" (arg0), "r" (arg1), "r" (arg2)
        : "x0", "x1", "x2", "x3", "cc", "memory");
#else
        asm volatile(
                ".arch_extension sec\n\t"
                "mov    r0, %1\n\t"
                "mov    r1, %2\n\t"
                "mov    r2, %3\n\t"
                "mov    r3, %4\n\t"
                "smc    #0    \n\t"
                "mov    %0, r0\n\t"
        : "=r" (ret)
        : "r" (func), "r" (arg0), "r" (arg1), "r" (arg2)
        : "r0", "r1", "r2", "r3", "cc", "memory");
#endif

	return ret;
}

static int __noinline invoke_psci_fn_smc(unsigned long func,
					 unsigned long arg0,
					 unsigned long arg1,
					 unsigned long arg2)
{
	long ret;

	if (!psci_avail)
		return VMM_ENOSYS;

	ret = invoke_psci_fn_smc_raw(func, arg0, arg1, arg2);

	return psci_to_xvisor_errno(ret);
}

int psci_cpu_suspend(unsigned long power_state, unsigned long entry_point)
{
	return invoke_psci_fn_smc(psci_function_id[PSCI_FN_CPU_SUSPEND],
				  power_state, entry_point, 0);
}

int psci_cpu_off(unsigned long power_state)
{
	return invoke_psci_fn_smc(psci_function_id[PSCI_FN_CPU_OFF],
				  power_state, 0, 0);
}

int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	return invoke_psci_fn_smc(psci_function_id[PSCI_FN_CPU_ON],
				  cpuid, entry_point, 0);
}

int psci_migrate(unsigned long cpuid)
{
	return invoke_psci_fn_smc(psci_function_id[PSCI_FN_MIGRATE],
				  cpuid, 0, 0);
}

u32 psci_get_version(void)
{
	return invoke_psci_fn_smc_raw(PSCI_0_2_FN_PSCI_VERSION, 0, 0, 0);
}

static void psci_0_2_set_functions(void)
{
	psci_function_id[PSCI_FN_CPU_SUSPEND] =
				PSCI_FN_NATIVE(0_2, CPU_SUSPEND);
	psci_function_id[PSCI_FN_CPU_ON] = PSCI_FN_NATIVE(0_2, CPU_ON);
	psci_function_id[PSCI_FN_CPU_OFF] = PSCI_0_2_FN_CPU_OFF;
	psci_function_id[PSCI_FN_MIGRATE] = PSCI_FN_NATIVE(0_2, MIGRATE);
}

static int psci_0_2_sys_reset(void)
{
	return invoke_psci_fn_smc(PSCI_0_2_FN_SYSTEM_RESET, 0, 0, 0);
}

static int psci_0_2_sys_poweroff(void)
{
	return invoke_psci_fn_smc(PSCI_0_2_FN_SYSTEM_OFF, 0, 0, 0);
}

static int psci_0_2_init(struct vmm_devtree_node *psci)
{
	u32 ver = psci_get_version();

	vmm_init_printf("PSCI v%d.%d detected in firmware.\n",
			PSCI_VERSION_MAJOR(ver),
			PSCI_VERSION_MINOR(ver));

	if (PSCI_VERSION_MAJOR(ver) == 0 && PSCI_VERSION_MINOR(ver) < 2) {
		vmm_lerror("psci", "Conflicting PSCI version detected.\n");
		return VMM_EINVALID;
	}

	psci_0_2_set_functions();

	vmm_register_system_reset(psci_0_2_sys_reset);
	vmm_register_system_shutdown(psci_0_2_sys_poweroff);

	return VMM_OK;
}

static int psci_0_1_init(struct vmm_devtree_node *psci)
{
	int rc;

	vmm_init_printf("PSCI v0.1 detected in firmware.\n");

	/* Retrieve the "cpu_on" attribute */
	rc = vmm_devtree_read_u32(psci, "cpu_on",
			&psci_function_id[PSCI_FN_CPU_ON]);
	if (rc) {
		vmm_printf("%s: Can't find 'cpu_on' attribute\n",
			   __func__);
		return VMM_ENOSYS;
	}

	/* Retrieve the "cpu_suspend" attribute */
	rc = vmm_devtree_read_u32(psci, "cpu_suspend",
			&psci_function_id[PSCI_FN_CPU_SUSPEND]);
	if (rc) {
		vmm_printf("%s: Can't find 'cpu_suspend' attribute\n",
			   __func__);
		return VMM_ENOSYS;
	}

	/* Retrieve the "cpu_off" attribute */
	rc = vmm_devtree_read_u32(psci, "cpu_off",
			&psci_function_id[PSCI_FN_CPU_OFF]);
	if (rc) {
		vmm_printf("%s: Can't find 'cpu_off' attribute\n",
			   __func__);
		return VMM_ENOSYS;
	}

	/* Retrieve the "migrate" attribute */
	rc = vmm_devtree_read_u32(psci, "migrate",
			&psci_function_id[PSCI_FN_MIGRATE]);
	if (rc) {
		vmm_printf("%s: Can't find 'migrate' attribute\n",
			   __func__);
		return VMM_ENOSYS;
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid psci_matches[] = {
	{ .compatible = "arm,psci-0.2", .data = psci_0_2_init },
	{ .compatible = "arm,psci", .data = psci_0_1_init },
	{ /* end of list */ },
};

bool psci_available(void)
{
	return psci_avail;
}

void __init psci_init(void)
{
	int rc;
	const char *method;
	struct vmm_devtree_node *psci;
	const struct vmm_devtree_nodeid *id;
	int (*psci_init)(struct vmm_devtree_node *);

	/* Look for node with PSCI compatible string */
	psci = vmm_devtree_find_matching(NULL, psci_matches);
	if (!psci) {
		/* Skip if no PSCI node available */
		return;
	}
	id = vmm_devtree_match_node(psci_matches, psci);
	psci_init = id->data;

	/* It should have a "method" attibute equal to "smc" */
	rc = vmm_devtree_read_string(psci, "method", &method);
	if (rc) {
		vmm_lerror("psci", "Can't find 'method' attribute\n");
		goto done;
	}

	/* Check method attribute */
	if (method && strcmp(method, "smc")) {
		vmm_printf("%s: 'method' is not 'smc'\n",
			   __func__);
		goto done;
	}

	/* Do PSCI version specific init */
	rc = psci_init(psci);
	if (rc) {
		vmm_lerror("psci", "init failed error %d\n", rc);
		goto done;
	}

	/* Mark PSCI available */
	psci_avail = TRUE;

done:
	vmm_devtree_dref_node(psci);
}
