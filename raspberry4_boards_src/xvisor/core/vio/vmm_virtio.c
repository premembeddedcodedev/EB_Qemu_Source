/**
 * Copyright (c) 2013 Pranav Sawargaonkar.
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
 * @file vmm_virtio.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO Para-virtualization Framework Implementation
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_guest_aspace.h>
#include <vmm_modules.h>
#include <vio/vmm_virtio.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>

#define MODULE_DESC		"VirtIO Para-virtualization Framework"
#define MODULE_AUTHOR		"Pranav Sawargaonkar"
#define MODULE_LICENSE		"GPL"
#define MODULE_IPRIORITY	VMM_VIRTIO_IPRIORITY
#define	MODULE_INIT		vmm_virtio_core_init
#define	MODULE_EXIT		vmm_virtio_core_exit

/*
 * virtio_mutex protects entire virtio subsystem and is taken every time
 * virtio device or emulator is registered or unregistered.
 */

static DEFINE_MUTEX(virtio_mutex);

static LIST_HEAD(virtio_dev_list);

static LIST_HEAD(virtio_emu_list);

/* ========== VirtIO queue implementations ========== */

struct vmm_guest *vmm_virtio_queue_guest(struct vmm_virtio_queue *vq)
{
	return (vq) ? vq->guest : NULL;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_guest);

u32 vmm_virtio_queue_desc_count(struct vmm_virtio_queue *vq)
{
	return (vq) ? vq->desc_count : 0;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_desc_count);

u32 vmm_virtio_queue_align(struct vmm_virtio_queue *vq)
{
	return (vq) ? vq->align : 0;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_align);

physical_addr_t vmm_virtio_queue_guest_pfn(struct vmm_virtio_queue *vq)
{
	return (vq) ? vq->guest_pfn : 0;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_guest_pfn);

physical_size_t vmm_virtio_queue_guest_page_size(struct vmm_virtio_queue *vq)
{
	return (vq) ? vq->guest_page_size : 0;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_guest_page_size);

physical_addr_t vmm_virtio_queue_guest_addr(struct vmm_virtio_queue *vq)
{
	return (vq) ? vq->guest_addr : 0;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_guest_addr);

physical_addr_t vmm_virtio_queue_host_addr(struct vmm_virtio_queue *vq)
{
	return (vq) ? vq->host_addr : 0;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_host_addr);

physical_size_t vmm_virtio_queue_total_size(struct vmm_virtio_queue *vq)
{
	return (vq) ? vq->total_size : 0;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_total_size);

u32 vmm_virtio_queue_max_desc(struct vmm_virtio_queue *vq)
{
	if (!vq || !vq->guest) {
		return 0;
	}

	return vq->desc_count;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_max_desc);

int vmm_virtio_queue_get_desc(struct vmm_virtio_queue *vq, u16 indx,
			      struct vmm_vring_desc *desc)
{
	u32 ret;
	physical_addr_t desc_pa;

	if (!vq || !vq->guest || !desc) {
		return VMM_EINVALID;
	}

	desc_pa = vq->vring.desc_pa + indx * sizeof(*desc);
	ret = vmm_guest_memory_read(vq->guest, desc_pa,
				    desc, sizeof(*desc), TRUE);
	if (ret != sizeof(*desc)) {
		return VMM_EIO;
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_get_desc);

u16 vmm_virtio_queue_pop(struct vmm_virtio_queue *vq)
{
	u16 val;
	u32 ret;
	physical_addr_t avail_pa;

	if (!vq || !vq->guest) {
		return 0;
	}

	ret = umod32(vq->last_avail_idx++, vq->desc_count);

	avail_pa = vq->vring.avail_pa +
		   offsetof(struct vmm_vring_avail, ring[ret]);
	ret = vmm_guest_memory_read(vq->guest, avail_pa,
				    &val, sizeof(val), TRUE);
	if (ret != sizeof(val)) {
		vmm_printf("%s: read failed at avail_pa=0x%"PRIPADDR"\n",
			   __func__, avail_pa);
		return 0;
	}

	return val;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_pop);

bool vmm_virtio_queue_available(struct vmm_virtio_queue *vq)
{
	u16 val;
	u32 ret;
	physical_addr_t avail_pa;

	if (!vq || !vq->guest) {
		return FALSE;
	}

	avail_pa = vq->vring.avail_pa +
		   offsetof(struct vmm_vring_avail, idx);
	ret = vmm_guest_memory_read(vq->guest, avail_pa,
				    &val, sizeof(val), TRUE);
	if (ret != sizeof(val)) {
		vmm_printf("%s: read failed at avail_pa=0x%"PRIPADDR"\n",
			   __func__, avail_pa);
		return FALSE;
	}

	return val != vq->last_avail_idx;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_available);

bool vmm_virtio_queue_should_signal(struct vmm_virtio_queue *vq)
{
	u32 ret;
	u16 old_idx, new_idx, event_idx;
	physical_addr_t used_pa, avail_pa;

	if (!vq || !vq->guest) {
		return FALSE;
	}

	old_idx = vq->last_used_signalled;

	used_pa = vq->vring.used_pa +
		  offsetof(struct vmm_vring_used, idx);
	ret = vmm_guest_memory_read(vq->guest, used_pa,
				    &new_idx, sizeof(new_idx), TRUE);
	if (ret != sizeof(new_idx)) {
		vmm_printf("%s: read failed at used_pa=0x%"PRIPADDR"\n",
			   __func__, used_pa);
		return FALSE;
	}

	avail_pa = vq->vring.avail_pa +
		   offsetof(struct vmm_vring_avail, ring[vq->vring.num]);
	ret = vmm_guest_memory_read(vq->guest, avail_pa,
				    &event_idx, sizeof(event_idx), TRUE);
	if (ret != sizeof(event_idx)) {
		vmm_printf("%s: read failed at avail_pa=0x%"PRIPADDR"\n",
			   __func__, avail_pa);
		return FALSE;
	}

	if (vmm_vring_need_event(event_idx, new_idx, old_idx)) {
		vq->last_used_signalled = new_idx;
		return TRUE;
	}

	return FALSE;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_should_signal);

void vmm_virtio_queue_set_avail_event(struct vmm_virtio_queue *vq)
{
	u16 val;
	u32 ret;
	physical_addr_t avail_evt_pa;

	if (!vq || !vq->guest) {
		return;
	}

	val = vq->last_avail_idx;
	avail_evt_pa = vq->vring.used_pa +
		  offsetof(struct vmm_vring_used, ring[vq->vring.num]);
	ret = vmm_guest_memory_write(vq->guest, avail_evt_pa,
				     &val, sizeof(val), TRUE);
	if (ret != sizeof(val)) {
		vmm_printf("%s: write failed at avail_evt_pa=0x%"PRIPADDR"\n",
			   __func__, avail_evt_pa);
	}

}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_set_avail_event);

void vmm_virtio_queue_set_used_elem(struct vmm_virtio_queue *vq,
				    u32 head, u32 len)
{
	u32 ret;
	u16 used_idx;
	struct vmm_vring_used_elem used_elem;
	physical_addr_t used_idx_pa, used_elem_pa;

	if (!vq || !vq->guest) {
		return;
	}

	used_idx_pa = vq->vring.used_pa +
		      offsetof(struct vmm_vring_used, idx);
	ret = vmm_guest_memory_read(vq->guest, used_idx_pa,
				    &used_idx, sizeof(used_idx), TRUE);
	if (ret != sizeof(used_idx)) {
		vmm_printf("%s: read failed at used_idx_pa=0x%"PRIPADDR"\n",
			   __func__, used_idx_pa);
	}

	used_elem.id = head;
	used_elem.len = len;
	ret = umod32(used_idx, vq->vring.num);
	used_elem_pa = vq->vring.used_pa +
		       offsetof(struct vmm_vring_used, ring[ret]);
	ret = vmm_guest_memory_write(vq->guest, used_elem_pa,
				     &used_elem, sizeof(used_elem), TRUE);
	if (ret != sizeof(used_elem)) {
		vmm_printf("%s: write failed at used_elem_pa=0x%"PRIPADDR"\n",
			   __func__, used_elem_pa);
	}

	used_idx++;
	ret = vmm_guest_memory_write(vq->guest, used_idx_pa,
				     &used_idx, sizeof(used_idx), TRUE);
	if (ret != sizeof(used_idx)) {
		vmm_printf("%s: write failed at used_idx_pa=0x%"PRIPADDR"\n",
			   __func__, used_idx_pa);
	}
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_set_used_elem);

bool vmm_virtio_queue_setup_done(struct vmm_virtio_queue *vq)
{
	return (vq) ? ((vq->guest) ? TRUE : FALSE) : FALSE;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_setup_done);

int vmm_virtio_queue_cleanup(struct vmm_virtio_queue *vq)
{
	if (!vq || !vq->guest) {
		goto done;
	}

	vq->last_avail_idx = 0;
	vq->last_used_signalled = 0;

	vq->guest = NULL;

	vq->desc_count = 0;
	vq->align = 0;
	vq->guest_pfn = 0;
	vq->guest_page_size = 0;

	vq->guest_addr = 0;
	vq->host_addr = 0;
	vq->total_size = 0;

done:
	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_cleanup);

int vmm_virtio_queue_setup(struct vmm_virtio_queue *vq,
			   struct vmm_guest *guest,
			   physical_addr_t guest_pfn,
			   physical_size_t guest_page_size,
			   u32 desc_count, u32 align)
{
	int rc = VMM_OK;
	u32 reg_flags;
	physical_addr_t gphys_addr, hphys_addr;
	physical_size_t gphys_size, avail_size;

	if (!vq || !guest) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_virtio_queue_cleanup(vq))) {
		vmm_printf("%s: cleanup failed\n", __func__);
		return rc;
	}

	gphys_addr = guest_pfn * guest_page_size;
	gphys_size = vmm_vring_size(desc_count, align);

	if ((rc = vmm_guest_physical_map(guest, gphys_addr, gphys_size,
					 &hphys_addr, &avail_size,
					 &reg_flags))) {
		vmm_printf("%s: vmm_guest_physical_map() failed\n", __func__);
		return VMM_EFAIL;
	}

	if (!(reg_flags & VMM_REGION_ISRAM)) {
		vmm_printf("%s: region is not backed by RAM\n", __func__);
		return VMM_EINVALID;
	}

	if (avail_size < gphys_size) {
		vmm_printf("%s: available size less than required size\n",
			   __func__);
		return VMM_EINVALID;
	}

	vmm_vring_init(&vq->vring, desc_count, NULL, gphys_addr, align);

	vq->guest = guest;
	vq->desc_count = desc_count;
	vq->align = align;
	vq->guest_pfn = guest_pfn;
	vq->guest_page_size = guest_page_size;

	vq->guest_addr = gphys_addr;
	vq->host_addr = hphys_addr;
	vq->total_size = gphys_size;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_setup);

/*
 * Each buffer in the virtqueues is actually a chain of descriptors.  This
 * function returns the next descriptor in the chain, max descriptor count
 * if we're at the end.
 */
static unsigned next_desc(struct vmm_virtio_queue *vq,
			  struct vmm_vring_desc *desc,
			  u32 i, u32 max)
{
	int rc;
	u32 next;

	if (!(desc->flags & VMM_VRING_DESC_F_NEXT)) {
		return max;
	}

	next = desc->next;

	rc = vmm_virtio_queue_get_desc(vq, next, desc);
	if (rc) {
		vmm_printf("%s: failed to get descriptor next=%d error=%d\n",
			   __func__, next, rc);
		return max;
	}

	return next;
}

int vmm_virtio_queue_get_head_iovec(struct vmm_virtio_queue *vq,
				    u16 head, struct vmm_virtio_iovec *iov,
				    u32 *ret_iov_cnt, u32 *ret_total_len,
				    u16 *ret_head)
{
	int i, rc = VMM_OK;
	u16 idx, max;
	struct vmm_vring_desc desc;

	if (!vq || !vq->guest || !iov) {
		goto fail;
	}

	idx = head;

	if (ret_iov_cnt) {
		*ret_iov_cnt = 0;
	}
	if (ret_total_len) {
		*ret_total_len = 0;
	}
	if (ret_head) {
		*ret_head = 0;
	}

	max = vmm_virtio_queue_max_desc(vq);

	rc = vmm_virtio_queue_get_desc(vq, idx, &desc);
	if (rc) {
		vmm_printf("%s: failed to get descriptor idx=%d error=%d\n",
			   __func__, idx, rc);
		goto fail;
	}

	if (desc.flags & VMM_VRING_DESC_F_INDIRECT) {
#if 0
		max = desc[idx].len / sizeof(struct vring_desc);
		desc = guest_flat_to_host(kvm, desc[idx].addr);
		idx = 0;
#endif
		vmm_printf("%s: indirect descriptor not supported idx=%d\n",
			   __func__, idx);
		rc = VMM_ENOTSUPP;
		goto fail;
	}

	i = 0;
	do {
		iov[i].addr = desc.addr;
		iov[i].len = desc.len;

		if (ret_total_len) {
			*ret_total_len += desc.len;
		}

		if (desc.flags & VMM_VRING_DESC_F_WRITE) {
			iov[i].flags = 1;  /* Write */
		} else {
			iov[i].flags = 0; /* Read */
		}

		i++;
	} while ((idx = next_desc(vq, &desc, idx, max)) != max);

	if (ret_iov_cnt) {
		*ret_iov_cnt = i;
	}

	vmm_virtio_queue_set_avail_event(vq);

	if (ret_head) {
		*ret_head = head;
	}

	return VMM_OK;

fail:
	if (ret_iov_cnt) {
		*ret_iov_cnt = 0;
	}
	if (ret_total_len) {
		*ret_total_len = 0;
	}
	return rc;
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_get_head_iovec);

int vmm_virtio_queue_get_iovec(struct vmm_virtio_queue *vq,
			       struct vmm_virtio_iovec *iov,
			       u32 *ret_iov_cnt, u32 *ret_total_len,
			       u16 *ret_head)
{
	u16 head = vmm_virtio_queue_pop(vq);

	return vmm_virtio_queue_get_head_iovec(vq, head, iov,
					       ret_iov_cnt, ret_total_len,
					       ret_head);
}
VMM_EXPORT_SYMBOL(vmm_virtio_queue_get_iovec);

u32 vmm_virtio_iovec_to_buf_read(struct vmm_virtio_device *dev,
				 struct vmm_virtio_iovec *iov,
				 u32 iov_cnt, void *buf,
				 u32 buf_len)
{
	u32 i = 0, pos = 0, len = 0;

	for (i = 0; i < iov_cnt && pos < buf_len; i++) {
		len = ((buf_len - pos) < iov[i].len) ?
				(buf_len - pos) : iov[i].len;

		len = vmm_guest_memory_read(dev->guest, iov[i].addr,
					    buf + pos, len, TRUE);
		if (!len) {
			break;
		}

		pos += len;
	}

	return pos;
}
VMM_EXPORT_SYMBOL(vmm_virtio_iovec_to_buf_read);

u32 vmm_virtio_buf_to_iovec_write(struct vmm_virtio_device *dev,
				  struct vmm_virtio_iovec *iov,
				  u32 iov_cnt, void *buf,
				  u32 buf_len)
{
	u32 i = 0, pos = 0, len = 0;

	for (i = 0; i < iov_cnt && pos < buf_len; i++) {
		len = ((buf_len - pos) < iov[i].len) ?
					(buf_len - pos) : iov[i].len;

		len = vmm_guest_memory_write(dev->guest, iov[i].addr,
					     buf + pos, len, TRUE);
		if (!len) {
			break;
		}

		pos += len;
	}

	return pos;
}
VMM_EXPORT_SYMBOL(vmm_virtio_buf_to_iovec_write);

void vmm_virtio_iovec_fill_zeros(struct vmm_virtio_device *dev,
				 struct vmm_virtio_iovec *iov,
				 u32 iov_cnt)
{
	u32 i = 0, pos = 0, len = 0;
	u8 zeros[16];

	memset(zeros, 0, sizeof(zeros));

	while (i < iov_cnt) {
		len = (iov[i].len < 16) ? iov[i].len : 16;
		len = vmm_guest_memory_write(dev->guest, iov[i].addr + pos,
					     zeros, len, TRUE);
		if (!len) {
			break;
		}

		pos += len;
		if (pos == iov[i].len) {
			pos = 0;
			i++;
		}
	}
}
VMM_EXPORT_SYMBOL(vmm_virtio_iovec_fill_zeros);

/* ========== VirtIO device and emulator implementations ========== */

static int __virtio_reset_emulator(struct vmm_virtio_device *dev)
{
	if (dev && dev->emu && dev->emu->reset) {
		return dev->emu->reset(dev);
	}

	return VMM_OK;
}

static int __virtio_connect_emulator(struct vmm_virtio_device *dev,
				     struct vmm_virtio_emulator *emu)
{
	if (dev && emu && emu->connect) {
		return emu->connect(dev, emu);
	}

	return VMM_OK;
}

static void __virtio_disconnect_emulator(struct vmm_virtio_device *dev)
{
	if (dev && dev->emu && dev->emu->disconnect) {
		dev->emu->disconnect(dev);
	}
}

static int __virtio_config_read_emulator(struct vmm_virtio_device *dev,
					 u32 offset, void *dst, u32 dst_len)
{
	if (dev && dev->emu && dev->emu->read_config) {
		return dev->emu->read_config(dev, offset, dst, dst_len);
	}

	return VMM_OK;
}

static int __virtio_config_write_emulator(struct vmm_virtio_device *dev,
					  u32 offset, void *src, u32 src_len)
{
	if (dev && dev->emu && dev->emu->write_config) {
		return dev->emu->write_config(dev, offset, src, src_len);
	}

	return VMM_OK;
}

static bool __virtio_match_device(const struct vmm_virtio_device_id *ids,
				  struct vmm_virtio_device *dev)
{
	while (ids->type) {
		if (ids->type == dev->id.type)
			return TRUE;
		ids++;
	}
	return FALSE;
}

static int __virtio_bind_emulator(struct vmm_virtio_device *dev,
				  struct vmm_virtio_emulator *emu)
{
	int rc = VMM_EINVALID;
	if (__virtio_match_device(emu->id_table, dev)) {
		dev->emu = emu;
		if ((rc = __virtio_connect_emulator(dev, emu))) {
			dev->emu = NULL;
		}
	}
	return rc;
}

static int __virtio_find_emulator(struct vmm_virtio_device *dev)
{
	struct vmm_virtio_emulator *emu;

	if (!dev || dev->emu) {
		return VMM_EINVALID;
	}

	list_for_each_entry(emu, &virtio_emu_list, node) {
		if (!__virtio_bind_emulator(dev, emu)) {
			return VMM_OK;
		}
	}

	return VMM_EFAIL;
}

static void __virtio_attach_emulator(struct vmm_virtio_emulator *emu)
{
	struct vmm_virtio_device *dev;

	if (!emu) {
		return;
	}

	list_for_each_entry(dev, &virtio_dev_list, node) {
		if (!dev->emu) {
			__virtio_bind_emulator(dev, emu);
		}
	}
}

int vmm_virtio_config_read(struct vmm_virtio_device *dev,
			   u32 offset, void *dst, u32 dst_len)
{
	return __virtio_config_read_emulator(dev, offset, dst, dst_len);
}
VMM_EXPORT_SYMBOL(vmm_virtio_config_read);

int vmm_virtio_config_write(struct vmm_virtio_device *dev,
			    u32 offset, void *src, u32 src_len)
{
	return __virtio_config_write_emulator(dev, offset, src, src_len);
}
VMM_EXPORT_SYMBOL(vmm_virtio_config_write);

int vmm_virtio_reset(struct vmm_virtio_device *dev)
{
	return __virtio_reset_emulator(dev);
}
VMM_EXPORT_SYMBOL(vmm_virtio_reset);

int vmm_virtio_register_device(struct vmm_virtio_device *dev)
{
	int rc = VMM_OK;

	if (!dev || !dev->tra) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&dev->node);
	dev->emu = NULL;
	dev->emu_data = NULL;

	vmm_mutex_lock(&virtio_mutex);

	list_add_tail(&dev->node, &virtio_dev_list);
	rc = __virtio_find_emulator(dev);

	vmm_mutex_unlock(&virtio_mutex);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_virtio_register_device);

void vmm_virtio_unregister_device(struct vmm_virtio_device *dev)
{
	if (!dev) {
		return;
	}

	vmm_mutex_lock(&virtio_mutex);

	__virtio_disconnect_emulator(dev);
	list_del(&dev->node);

	vmm_mutex_unlock(&virtio_mutex);
}
VMM_EXPORT_SYMBOL(virtio_unregister_device);

int vmm_virtio_register_emulator(struct vmm_virtio_emulator *emu)
{
	bool found;
	struct vmm_virtio_emulator *vemu;

	if (!emu) {
		return VMM_EFAIL;
	}

	vemu = NULL;
	found = FALSE;

	vmm_mutex_lock(&virtio_mutex);

	list_for_each_entry(vemu, &virtio_emu_list, node) {
		if (strcmp(vemu->name, emu->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&virtio_mutex);
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&emu->node);
	list_add_tail(&emu->node, &virtio_emu_list);

	__virtio_attach_emulator(emu);

	vmm_mutex_unlock(&virtio_mutex);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_virtio_register_emulator);

void vmm_virtio_unregister_emulator(struct vmm_virtio_emulator *emu)
{
	struct vmm_virtio_device *dev;

	vmm_mutex_lock(&virtio_mutex);

	list_del(&emu->node);

	list_for_each_entry(dev, &virtio_dev_list, node) {
		if (dev->emu == emu) {
			__virtio_disconnect_emulator(dev);
			__virtio_find_emulator(dev);
		}
	}

	vmm_mutex_unlock(&virtio_mutex);
}
VMM_EXPORT_SYMBOL(vmm_virtio_unregister_emulator);

static int __init vmm_virtio_core_init(void)
{
	/* Nothing to be done */
	return VMM_OK;
}

static void __exit vmm_virtio_core_exit(void)
{
	/* Nothing to be done */
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
