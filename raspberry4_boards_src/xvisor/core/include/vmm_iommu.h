/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_iommu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief IOMMU framework header for device pass-through
 *
 * The source has been largely adapted from Linux sources:
 * include/linux/iommu.h
 *
 * Copyright (C) 2007-2008 Advanced Micro Devices, Inc.
 * Author: Joerg Roedel <joerg.roedel@amd.com>
 *
 * The original source is licensed under GPL.
 */
#ifndef _VMM_IOMMU_H__
#define _VMM_IOMMU_H__

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <libs/xref.h>

#define VMM_IOMMU_CONTROLLER_CLASS_NAME				"iommu"

struct vmm_iommu_ops;
struct vmm_iommu_group;
struct vmm_iommu_controller;
struct vmm_iommu_domain;
struct vmm_notifier_block;

/* nodeid table based IOMMU initialization callback */
typedef int (*vmm_iommu_init_t)(struct vmm_devtree_node *);

/* declare nodeid table based initialization for IOMMU */
#define VMM_IOMMU_INIT_DECLARE(name, compat, fn)	\
VMM_DEVTREE_NIDTBL_ENTRY(name, "iommu", "", "", compat, fn)

struct vmm_iommu_controller {
	/* Public members */
	char name[VMM_FIELD_NAME_SIZE];
	/* Private members */
	struct vmm_device dev;
	struct vmm_mutex groups_lock;
	struct dlist groups;
	struct vmm_mutex domains_lock;
	struct dlist domains;
};

/* iommu mapping attributes */
#define VMM_IOMMU_READ		(1 << 0)
#define VMM_IOMMU_WRITE		(1 << 1)
#define VMM_IOMMU_CACHE		(1 << 2) /* DMA cache coherency */
#define VMM_IOMMU_NOEXEC	(1 << 3)
#define VMM_IOMMU_MMIO		(1 << 4)

/* Domain feature flags */
#define __VMM_IOMMU_DOMAIN_PAGING	(1U << 0)  /* Support for iommu_map/unmap */
#define __VMM_IOMMU_DOMAIN_DMA_API	(1U << 1)  /* Domain for use in DMA-API
						      implementation              */
#define __VMM_IOMMU_DOMAIN_PT	(1U << 2)  /* Domain is identity mapped   */

/*
 * This are the possible domain-types
 *
 *	VMM_IOMMU_DOMAIN_BLOCKED	- All DMA is blocked, can be used to isolate
 *					  devices
 *	VMM_IOMMU_DOMAIN_IDENTITY	- DMA addresses are system physical addresses
 *	VMM_IOMMU_DOMAIN_UNMANAGED	- DMA mappings managed by IOMMU-API user, used
 *					  for VMs
 *	VMM_IOMMU_DOMAIN_DMA		- Internally used for DMA-API implementations.
 *					  This flag allows IOMMU drivers to implement
 *					  certain optimizations for these domains
 */
#define VMM_IOMMU_DOMAIN_BLOCKED	(0U)
#define VMM_IOMMU_DOMAIN_IDENTITY	(__VMM_IOMMU_DOMAIN_PT)
#define VMM_IOMMU_DOMAIN_UNMANAGED	(__VMM_IOMMU_DOMAIN_PAGING)
#define VMM_IOMMU_DOMAIN_DMA		(__VMM_IOMMU_DOMAIN_PAGING | \
					 __VMM_IOMMU_DOMAIN_DMA_API)
/* iommu fault flags */
#define VMM_IOMMU_FAULT_READ	0x0
#define VMM_IOMMU_FAULT_WRITE	0x1

typedef int (*vmm_iommu_fault_handler_t)(struct vmm_iommu_domain *,
			struct vmm_device *, physical_addr_t, int, void *);

struct vmm_iommu_domain_geometry {
	dma_addr_t aperture_start; /* First address that can be mapped    */
	dma_addr_t aperture_end;   /* Last address that can be mapped     */
	bool force_aperture;       /* DMA only allowed in mappable range? */
};

struct vmm_iommu_domain {
	/* Public members */
	char name[VMM_FIELD_NAME_SIZE];
	unsigned int type;
	struct vmm_bus *bus;
	struct vmm_iommu_controller *ctrl;
	/* Private members */
	struct dlist head;
	struct xref ref_count;
	struct vmm_iommu_ops *ops;
	void *priv;
	vmm_iommu_fault_handler_t handler;
	void *handler_token;
	struct vmm_iommu_domain_geometry geometry;
};

enum vmm_iommu_cap {
	VMM_IOMMU_CAP_CACHE_COHERENCY,	/* IOMMU can enforce cache coherent DMA
					   transactions */
	VMM_IOMMU_CAP_INTR_REMAP,	/* IOMMU supports interrupt isolation */
	VMM_IOMMU_CAP_NOEXEC,		/* IOMMU_NOEXEC flag */
};

/*
 * Following constraints are specifc to FSL_PAMUV1:
 *  -aperture must be power of 2, and naturally aligned
 *  -number of windows must be power of 2, and address space size
 *   of each window is determined by aperture size / # of windows
 *  -the actual size of the mapped region of a window must be power
 *   of 2 starting with 4KB and physical address must be naturally
 *   aligned.
 * DOMAIN_ATTR_FSL_PAMUV1 corresponds to the above mentioned contraints.
 * The caller can invoke iommu_domain_get_attr to check if the underlying
 * iommu implementation supports these constraints.
 */
enum vmm_iommu_attr {
	VMM_DOMAIN_ATTR_GEOMETRY,
	VMM_DOMAIN_ATTR_PAGING,
	VMM_DOMAIN_ATTR_WINDOWS,
	VMM_DOMAIN_ATTR_FSL_PAMU_STASH,
	VMM_DOMAIN_ATTR_FSL_PAMU_ENABLE,
	VMM_DOMAIN_ATTR_FSL_PAMUV1,
	VMM_DOMAIN_ATTR_MAX,
};

/**
 * IOMMU ops and capabilities
 * @capable: check capability
 * @domain_alloc: allocate iommu domain
 * @domain_free: free iommu domain
 * @attach_dev: attach device to an iommu domain
 * @detach_dev: detach device from an iommu domain
 * @map: map a physically contiguous memory region to an iommu domain
 * @unmap: unmap a physically contiguous memory region from an iommu domain
 * @iova_to_phys: translate iova to physical address
 * @add_device: add device to iommu grouping
 * @remove_device: remove device from iommu grouping
 * @domain_get_attr: Query domain attributes
 * @domain_set_attr: Change domain attributes
 * @domain_window_enable: Configure and enable a particular window for a domain
 * @domain_window_disable: Disable a particular window for a domain
 * @domain_set_windows: Set the number of windows for a domain
 * @domain_get_windows: Return the number of windows for a domain
 * @of_xlate: add OF master IDs to iommu grouping
 * @pgsize_bitmap: bitmap of all possible supported page sizes
 */
struct vmm_iommu_ops {
	bool (*capable)(enum vmm_iommu_cap);

	struct vmm_iommu_domain *(*domain_alloc)(unsigned int type,
					struct vmm_iommu_controller *ctrl);
	void (*domain_free)(struct vmm_iommu_domain *domain);
	int (*attach_dev)(struct vmm_iommu_domain *domain,
			  struct vmm_device *dev);
	void (*detach_dev)(struct vmm_iommu_domain *domain,
			   struct vmm_device *dev);
	int (*map)(struct vmm_iommu_domain *domain, physical_addr_t iova,
		   physical_addr_t paddr, size_t size, int prot);
	size_t (*unmap)(struct vmm_iommu_domain *domain,
			physical_addr_t iova, size_t size);
	physical_addr_t (*iova_to_phys)(struct vmm_iommu_domain *domain,
					physical_addr_t iova);
	int (*add_device)(struct vmm_device *dev);
	void (*remove_device)(struct vmm_device *dev);

	int (*domain_get_attr)(struct vmm_iommu_domain *domain,
			       enum vmm_iommu_attr attr, void *data);
	int (*domain_set_attr)(struct vmm_iommu_domain *domain,
			       enum vmm_iommu_attr attr, void *data);

	/* Window handling functions */
	int (*domain_window_enable)(struct vmm_iommu_domain *domain, u32 wnd_nr,
				    physical_addr_t paddr, u64 size, int prot);
	void (*domain_window_disable)(struct vmm_iommu_domain *domain, u32 wnd_nr);
	/* Set the numer of window per domain */
	int (*domain_set_windows)(struct vmm_iommu_domain *domain, u32 w_count);
	/* Get the numer of window per domain */
	u32 (*domain_get_windows)(struct vmm_iommu_domain *domain);

	int (*of_xlate)(struct vmm_device *dev,
			struct vmm_devtree_phandle_args *args);

	unsigned long pgsize_bitmap;
};

#define VMM_IOMMU_GROUP_NOTIFY_ADD_DEVICE	1 /* Device added */
#define VMM_IOMMU_GROUP_NOTIFY_DEL_DEVICE	2 /* Pre Device removed */
#define VMM_IOMMU_GROUP_NOTIFY_BIND_DRIVER	3 /* Pre Driver bind */
#define VMM_IOMMU_GROUP_NOTIFY_BOUND_DRIVER	4 /* Post Driver bind */
#define VMM_IOMMU_GROUP_NOTIFY_UNBIND_DRIVER	5 /* Pre Driver unbind */
#define VMM_IOMMU_GROUP_NOTIFY_UNBOUND_DRIVER	6 /* Post Driver unbind */

/* =============== IOMMU Controller APIs =============== */

/** Register IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_controller_register(struct vmm_iommu_controller *ctrl);

/** Unregister IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_controller_unregister(struct vmm_iommu_controller *ctrl);

/** Find an IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
struct vmm_iommu_controller *vmm_iommu_controller_find(const char *name);

/** Iterate over each IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_controller_iterate(struct vmm_iommu_controller *start,
		void *data, int (*fn)(struct vmm_iommu_controller *, void *));

/** Count number of IOMMU controllers
 *  Note: This function must be called in Orphan (or Thread) context
 */
u32 vmm_iommu_controller_count(void);

/** Iterate over each IOMMU group of given IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_controller_for_each_group(struct vmm_iommu_controller *ctrl,
		void *data, int (*fn)(struct vmm_iommu_group *, void *));

/** Count number of IOMMU groups in given IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
u32 vmm_iommu_controller_group_count(struct vmm_iommu_controller *ctrl);

/** Iterate over each IOMMU domain of given IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_controller_for_each_domain(struct vmm_iommu_controller *ctrl,
		void *data, int (*fn)(struct vmm_iommu_domain *, void *));

/** Count number of IOMMU domains in given IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
u32 vmm_iommu_controller_domain_count(struct vmm_iommu_controller *ctrl);

/* =============== IOMMU Group APIs =============== */

/** Alloc new IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
struct vmm_iommu_group *vmm_iommu_group_alloc(const char *name,
					struct vmm_iommu_controller *ctrl);

/** Get IOMMU group of given device */
struct vmm_iommu_group *vmm_iommu_group_get(struct vmm_device *dev);

/** Put IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
void vmm_iommu_group_free(struct vmm_iommu_group *group);
#define vmm_iommu_group_put(group)	vmm_iommu_group_free(group)

/** Get IOMMU group instance by ID */
struct vmm_iommu_group *vmm_iommu_group_get_by_id(int id);

/** Get private data for given IOMMU group */
void *vmm_iommu_group_get_iommudata(struct vmm_iommu_group *group);

/** Set private data for given IOMMU group */
void vmm_iommu_group_set_iommudata(struct vmm_iommu_group *group,
				   void *iommu_data,
				   void (*release)(void *iommu_data));

/** Add device to IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_add_device(struct vmm_iommu_group *group,
			       struct vmm_device *dev);

/** Remove device from IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
void vmm_iommu_group_remove_device(struct vmm_device *dev);

/** Iterate over each device of given IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_for_each_dev(struct vmm_iommu_group *group, void *data,
				 int (*fn)(struct vmm_device *, void *));

/** Register notifier client for IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_register_notifier(struct vmm_iommu_group *group,
				      struct vmm_notifier_block *nb);

/** Unregister notifier client for IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_unregister_notifier(struct vmm_iommu_group *group,
					struct vmm_notifier_block *nb);

/** Get name for given IOMMU group */
const char *vmm_iommu_group_name(struct vmm_iommu_group *group);

/** Get IOMMU controller for given IOMMU group */
struct vmm_iommu_controller *vmm_iommu_group_controller(
					struct vmm_iommu_group *group);

/** Attach IOMMU domain to given IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_attach_domain(struct vmm_iommu_group *group,
				  struct vmm_iommu_domain *domain);

/** Detach IOMMU domain from given IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_detach_domain(struct vmm_iommu_group *group);

/** Get IOMMU domain of given IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
struct vmm_iommu_domain *vmm_iommu_group_get_domain(
					struct vmm_iommu_group *group);

/* =============== IOMMU Domain APIs =============== */

/** Alloc new IOMMU domain for given bus type and IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
struct vmm_iommu_domain *vmm_iommu_domain_alloc(const char *name,
					struct vmm_bus *bus,
					struct vmm_iommu_controller *ctrl,
					unsigned int type);

/**  Increase reference count of a domain */
void vmm_iommu_domain_ref(struct vmm_iommu_domain *domain);

/** Free existing IOMMU domain
 *  Note: This function must be called in Orphan (or Thread) context
 */
void vmm_iommu_domain_free(struct vmm_iommu_domain *domain);
#define vmm_iommu_domain_dref(domain)	vmm_iommu_domain_free(domain)

/** Set fault handler for given IOMMU domain */
void vmm_iommu_set_fault_handler(struct vmm_iommu_domain *domain,
				 vmm_iommu_fault_handler_t handler,
				 void *token);

/**
 * Report about an IOMMU fault to the IOMMU framework
 * @domain: the iommu domain where the fault has happened
 * @dev: the device where the fault has happened
 * @iova: the faulting address
 * @flags: mmu fault flags (e.g. VMM_IOMMU_FAULT_READ/VMM_IOMMU_FAULT_WRITE/...)
 *
 * This function should be called by the low-level IOMMU implementations
 * whenever IOMMU faults happen, to allow high-level users, that are
 * interested in such events, to know about them.
 *
 * This event may be useful for several possible use cases:
 * - mere logging of the event
 * - dynamic TLB/PTE loading
 * - if restarting of the faulting device is required
 *
 * Returns 0 on success and an appropriate error code otherwise (if dynamic
 * PTE/TLB loading will one day be supported, implementations will be able
 * to tell whether it succeeded or not according to this return value).
 *
 * Specifically, VMM_ENOSYS is returned if a fault handler isn't installed
 * (though fault handlers can also return VMM_ENOSYS, in case they want to
 * elicit the default behavior of the IOMMU drivers).
 */
static inline int vmm_report_iommu_fault(struct vmm_iommu_domain *domain,
		struct vmm_device *dev, physical_addr_t iova, int flags)
{
	int ret = VMM_ENOSYS;

	/*
	 * if upper layers showed interest and installed a fault handler,
	 * invoke it.
	 */
	if (domain->handler)
		ret = domain->handler(domain, dev, iova, flags,
						domain->handler_token);

	return ret;
}

/** Get IO virtual addres mapping for given IOMMU domain */
physical_addr_t vmm_iommu_iova_to_phys(struct vmm_iommu_domain *domain,
				       physical_addr_t iova);

/** Map IO virtual address to Physical address for given IOMMU domain */
int vmm_iommu_map(struct vmm_iommu_domain *domain, physical_addr_t iova,
		  physical_addr_t paddr, size_t size, int prot);

/** Unmap IO virtual address for given IOMMU domain */
size_t vmm_iommu_unmap(struct vmm_iommu_domain *domain,
			physical_addr_t iova, size_t size);

/** Enable physical address window for IOMMU domain */
int vmm_iommu_domain_window_enable(struct vmm_iommu_domain *domain,
				   u32 wnd_nr, physical_addr_t offset,
				   u64 size, int prot);

/** Disable physical address window for IOMMU domain */
void vmm_iommu_domain_window_disable(struct vmm_iommu_domain *domain,
				     u32 wnd_nr);

/** Get attributes of IOMMU domain */
int vmm_iommu_domain_get_attr(struct vmm_iommu_domain *domain,
			      enum vmm_iommu_attr, void *data);

/** Set attributes for IOMMU domain */
int vmm_iommu_domain_set_attr(struct vmm_iommu_domain *domain,
			      enum vmm_iommu_attr, void *data);

/* =============== IOMMU Misc APIs =============== */

/** Set IOMMU operations for given bus type */
int vmm_bus_set_iommu(struct vmm_bus *bus, struct vmm_iommu_ops *ops);

/** Check whethere IOMMU operations are available for given bus type */
bool vmm_iommu_present(struct vmm_bus *bus);

/**  Capability check on IOMMU for given bus type */
bool vmm_iommu_capable(struct vmm_bus *bus, enum vmm_iommu_cap cap);

/** Initialize IOMMU framework */
int __init vmm_iommu_init(void);

#endif
