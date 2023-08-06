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
 * @file virtio_net.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO based Network Device Emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vio/vmm_virtio.h>
#include <vio/vmm_virtio_net.h>

#include <net/vmm_protocol.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_net.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>
#include <net/vmm_mbuf.h>

#define MODULE_DESC			"VirtIO Net Emulator"
#define MODULE_AUTHOR			"Pranav Sawargaonkar"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VIRTIO_IPRIORITY + 1)
#define MODULE_INIT			virtio_net_init
#define MODULE_EXIT			virtio_net_exit

#define VIRTIO_NET_QUEUE_SIZE		256

#define VIRTIO_NET_UNK_QUEUE		0
#define VIRTIO_NET_RX_QUEUE		1
#define VIRTIO_NET_TX_QUEUE		2
#define VIRTIO_NET_CTRL_QUEUE		3

#define VIRTIO_NET_MTU			1514

#define VIRTIO_NET_TX_LAZY_BUDGET	(VIRTIO_NET_QUEUE_SIZE / 4)

struct virtio_net_queue {
	int num;
	int valid;
	int type;
	struct vmm_netport_lazy lazy;
	struct vmm_virtio_queue vq;
	struct vmm_virtio_iovec iov[VIRTIO_NET_QUEUE_SIZE];
	struct virtio_net_dev *ndev;
};

struct virtio_net_dev {
	struct vmm_virtio_device *vdev;

	struct virtio_net_queue *vqs;
	u32 cq;		/* Configuration queue number */
	u32 max_queues;
	u32 can_receive;
	struct vmm_virtio_net_config config;
	u64 features;

	int mode;
	struct vmm_netport *port;
	char name[VMM_VIRTIO_DEVICE_MAX_NAME_LEN];
};

static u64 virtio_net_get_host_features(struct vmm_virtio_device *dev)
{
	return 1UL << VMM_VIRTIO_NET_F_MAC
#if 0
		| 1UL << VMM_VIRTIO_NET_F_CSUM
		| 1UL << VMM_VIRTIO_NET_F_HOST_UFO
		| 1UL << VMM_VIRTIO_NET_F_HOST_TSO4
		| 1UL << VMM_VIRTIO_NET_F_HOST_TSO6
		| 1UL << VMM_VIRTIO_NET_F_GUEST_UFO
		| 1UL << VMM_VIRTIO_NET_F_GUEST_TSO4
		| 1UL << VMM_VIRTIO_NET_F_GUEST_TSO6
#endif
		| 1UL << VMM_VIRTIO_RING_F_EVENT_IDX
#if 0
		| 1UL << VMM_VIRTIO_RING_F_INDIRECT_DESC
#endif
		| 1UL << VMM_VIRTIO_NET_F_MQ
		| 1UL << VMM_VIRTIO_NET_F_CTRL_VQ
		;
}

static void virtio_net_set_guest_features(struct vmm_virtio_device *dev,
					  u32 select, u32 features)
{
	struct virtio_net_dev *ndev = dev->emu_data;

	if (1 < select)
		return;

	ndev->features &= ~((u64)UINT_MAX << (select * 32));
	ndev->features |= ((u64)features << (select * 32));
}

static int virtio_net_init_vq(struct vmm_virtio_device *dev,
			      u32 vq, u32 page_size, u32 align, u32 pfn)
{
	int rc;
	struct virtio_net_dev *ndev = dev->emu_data;

	rc = vmm_virtio_queue_setup(&ndev->vqs[vq].vq, dev->guest,
				pfn, page_size, VIRTIO_NET_QUEUE_SIZE, align);
	if (rc == VMM_OK) {
		ndev->vqs[vq].valid = 1;
	}

	return rc;
}

static int virtio_net_get_pfn_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc;
	struct virtio_net_dev *ndev = dev->emu_data;

	rc = vmm_virtio_queue_guest_pfn(&ndev->vqs[vq].vq);
	if (rc == VMM_OK) {
		ndev->vqs[vq].num = vq;
		ndev->vqs[vq].valid = 1;
	}

	return rc;
}

static int virtio_net_get_size_vq(struct vmm_virtio_device *dev, u32 vq)
{
	return VIRTIO_NET_QUEUE_SIZE;
}

static int virtio_net_set_size_vq(struct vmm_virtio_device *dev,
				  u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}

static void virtio_net_tx_poke(struct virtio_net_dev *ndev, u32 vq);

static void virtio_net_tx_lazy(struct vmm_netport *port, void *arg, int budget)
{
	int rc;
	u16 head = 0;
	u32 iov_cnt = 0, pkt_len = 0, total_len = 0;
	struct virtio_net_queue *q = arg;
	struct virtio_net_dev *ndev = q->ndev;
	struct vmm_virtio_queue *vq = &q->vq;
	struct vmm_virtio_device *dev = ndev->vdev;
	struct vmm_virtio_iovec *iov = q->iov;
	struct vmm_mbuf *mb;

	while ((budget > 0) && vmm_virtio_queue_available(vq)) {
		rc = vmm_virtio_queue_get_iovec(vq, iov,
						&iov_cnt, &total_len, &head);
		if (rc) {
			vmm_printf("%s: failed to get iovec (error %d)\n",
				   __func__, rc);
			continue;
		}

		/* iov[0] is offload info */
		pkt_len = total_len - iov[0].len;

		if (pkt_len <= VIRTIO_NET_MTU) {
			MGETHDR(mb, 0, 0);
			MEXTMALLOC(mb, pkt_len, 0);
			vmm_virtio_iovec_to_buf_read(dev,
						 &iov[1], iov_cnt - 1,
						 M_BUFADDR(mb), pkt_len);
			mb->m_len = mb->m_pktlen = pkt_len;
			vmm_port2switch_xfer_mbuf(ndev->port, mb);
		}

		vmm_virtio_queue_set_used_elem(vq, head, total_len);

		budget--;
	}

	if (vmm_virtio_queue_should_signal(vq)) {
		dev->tra->notify(dev, q->num);
	}

	virtio_net_tx_poke(ndev, q->num);
}

static void virtio_net_tx_poke(struct virtio_net_dev *ndev, u32 vq)
{
	struct virtio_net_queue *q = &ndev->vqs[vq];

	if (vmm_virtio_queue_available(&q->vq)) {
		vmm_port2switch_xfer_lazy(&q->lazy);
	}
}

static void virtio_net_handle_ctrl(struct virtio_net_dev *ndev, u32 qnum)
{
	int rc;
	u16 head = 0;
	u32 iov_cnt = 0, total_len = 0;
	struct virtio_net_queue *q = &ndev->vqs[qnum];
	struct vmm_virtio_queue *vq = &q->vq;
	struct vmm_virtio_iovec *iov = q->iov;
	struct vmm_virtio_device *dev = ndev->vdev;
	struct vmm_virtio_net_ctrl_hdr ctrl_hdr;
	struct vmm_virtio_net_ctrl_mq ctrl_mq;
	vmm_virtio_net_ctrl_ack_t status;

	while (vmm_virtio_queue_available(vq)) {
		rc = vmm_virtio_queue_get_iovec(vq, iov,
						  &iov_cnt, &total_len, &head);
		if (rc) {
			vmm_printf("%s: failed to get iovec (error %d)\n",
				   __func__, rc);
			continue;
		}

		status = VMM_VIRTIO_NET_ERR;

		if ((iov_cnt < 2) ||
		    (iov[0].len < sizeof(ctrl_hdr)) ||
		    (iov[iov_cnt - 1].len < sizeof(status))) {
			vmm_printf("%s: invalid ctrl IOV\n", __func__);
			goto skip;
		}

		vmm_virtio_iovec_to_buf_read(dev, &iov[0], 1,
					     &ctrl_hdr, sizeof(ctrl_hdr));

		switch (ctrl_hdr.class) {
		case VMM_VIRTIO_NET_CTRL_MQ:
			if ((iov_cnt < 3) ||
			    (iov[1].len < sizeof(ctrl_mq))) {
				vmm_printf("%s: invalid ctrl mq IOV\n",
					   __func__);
				goto skip;
			}
			vmm_virtio_iovec_to_buf_read(dev, &iov[1], 1,
						     &ctrl_mq, sizeof(ctrl_mq));

			if (ctrl_mq.virtqueue_pairs < ndev->max_queues) {
				status = VMM_VIRTIO_NET_OK;
			}
			break;
		default:
			vmm_printf("%s: IOV Class %d is not handled\n",
				   __func__, ctrl_hdr.class);
			break;
		};

skip:
		vmm_virtio_buf_to_iovec_write(dev, &iov[iov_cnt - 1], 1,
					      &status, 1);

		vmm_virtio_queue_set_used_elem(vq, head, total_len);
	}

	if (vmm_virtio_queue_should_signal(vq)) {
		dev->tra->notify(dev, q->num);
	}
}

static int virtio_net_notify_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc = VMM_OK;
	struct virtio_net_dev *ndev = dev->emu_data;

	switch (ndev->vqs[vq].type) {
	case VIRTIO_NET_TX_QUEUE:
		virtio_net_tx_poke(ndev, vq);
		break;
	case VIRTIO_NET_RX_QUEUE:
		break;
	case VIRTIO_NET_CTRL_QUEUE:
		virtio_net_handle_ctrl(ndev, vq);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	}

	return rc;
}

static void virtio_net_status_changed(struct vmm_virtio_device *dev,
				      u32 new_status)
{
	u32 i, have_rx_queue = 0;
	struct virtio_net_dev *ndev = dev->emu_data;

	for (i = 0; i < ndev->max_queues; i++) {
		if (ndev->vqs[i].valid &&
		    (ndev->vqs[i].type == VIRTIO_NET_RX_QUEUE)) {
			have_rx_queue++;
		}
	}

	if (have_rx_queue &&
	    (new_status & VMM_VIRTIO_CONFIG_S_DRIVER_OK)) {
		ndev->can_receive = 1;
	} else {
		ndev->can_receive = 0;
	}
}

static void virtio_net_link_changed(struct vmm_netport *p)
{
	/* Nothing to do here because we have dummy link. */
}

static int virtio_net_can_receive(struct vmm_netport *p)
{
	struct virtio_net_dev *ndev = p->priv;

	return ndev->can_receive;
}

static int virtio_net_switch2port_xfer(struct vmm_netport *p,
				       struct vmm_mbuf *mb)
{
	int rc;
	u16 head = 0;
	u64 iov0_addr;
	u32 iov_cnt = 0, iov0_len, total_len = 0, pkt_len = 0;
	struct virtio_net_dev *ndev = p->priv;
	/* FIXME: Select correct RX queue here  */
	struct virtio_net_queue *q = &ndev->vqs[0];
	struct vmm_virtio_queue *vq = &q->vq;
	struct vmm_virtio_iovec *iov = q->iov;
	struct vmm_virtio_device *dev = ndev->vdev;
	struct vmm_virtio_net_hdr hdr;

	pkt_len = min(VIRTIO_NET_MTU, mb->m_pktlen);

	if (vmm_virtio_queue_available(vq)) {
		rc = vmm_virtio_queue_get_iovec(vq, iov,
						&iov_cnt, &total_len, &head);
		if (rc) {
			vmm_printf("%s: failed to get iovec (error %d)\n",
				   __func__, rc);
			return rc;
		}
	}

	memset(&hdr, 0, sizeof(hdr));
	if (iov_cnt == 1) {
		vmm_virtio_buf_to_iovec_write(dev, &iov[0], 1,
					      &hdr, sizeof(hdr));
		iov0_addr = iov[0].addr;
		iov0_len = iov[0].len;
		iov[0].addr += sizeof(hdr);
		iov[0].len -= sizeof(hdr);
		vmm_virtio_buf_to_iovec_write(dev, &iov[0], 1,
					      M_BUFADDR(mb), pkt_len);
		vmm_virtio_queue_set_used_elem(vq, head,
					       sizeof(hdr) + pkt_len);
		iov[0].addr = iov0_addr;
		iov[0].len = iov0_len;
	} else if (iov_cnt > 1) {
		vmm_virtio_buf_to_iovec_write(dev, &iov[0], 1,
					      &hdr, sizeof(hdr));
		vmm_virtio_buf_to_iovec_write(dev, &iov[1], iov_cnt - 1,
					      M_BUFADDR(mb), pkt_len);
		vmm_virtio_queue_set_used_elem(vq, head, iov[0].len + pkt_len);
	}

	if (vmm_virtio_queue_should_signal(vq)) {
		/* FIXME: Select correct RX queue here  */
		dev->tra->notify(dev, 0);
	}

	m_freem(mb);

	return VMM_OK;
}

static int virtio_net_read_config(struct vmm_virtio_device *dev,
				  u32 offset, void *dst, u32 dst_len)
{
	struct virtio_net_dev *ndev = dev->emu_data;
	u8 *src = (u8 *)&ndev->config;
	u32 i, src_len = sizeof(ndev->config);

	for (i = 0; (i < dst_len) && ((offset + i) < src_len); i++) {
		((u8 *)dst)[i] = src[offset + i];
	}

	return VMM_OK;
}

static int virtio_net_write_config(struct vmm_virtio_device *dev,
				   u32 offset, void *src, u32 src_len)
{
	struct virtio_net_dev *ndev = dev->emu_data;
	u8 *dst = (u8 *)&ndev->config;
	u32 i, dst_len = sizeof(ndev->config);

	for (i = 0; (i < src_len) && ((offset + i) < dst_len); i++) {
		dst[offset + i] = ((u8 *)src)[i];
	}

	return VMM_OK;
}

static int virtio_net_reset(struct vmm_virtio_device *dev)
{
	int rc, i;
	struct virtio_net_dev *ndev = dev->emu_data;

	for (i = 0; i < ndev->max_queues; i++) {
		if (ndev->vqs[i].valid) {
			rc = vmm_virtio_queue_cleanup(&ndev->vqs[i].vq);
			if (rc) {
				return rc;
			}
		}
		ndev->vqs[i].valid = 0;
	}
	ndev->can_receive = 0;

	return VMM_OK;
}

static int virtio_net_connect(struct vmm_virtio_device *dev,
			      struct vmm_virtio_emulator *emu)
{
	int i, rc;
	const char *attr;
	struct virtio_net_dev *ndev;
	struct vmm_netswitch *nsw;

	ndev = vmm_zalloc(sizeof(struct virtio_net_dev));
	if (!ndev) {
		vmm_printf("Failed to allocate virtio net device....\n");
		return VMM_EFAIL;
	}

	ndev->vdev = dev;
	vmm_snprintf(ndev->name, VMM_VIRTIO_DEVICE_MAX_NAME_LEN,
		     "%s", dev->name);
	ndev->port = vmm_netport_alloc(ndev->name, VIRTIO_NET_QUEUE_SIZE);
	if (!ndev->port) {
		vmm_free(ndev);
		return VMM_ENOMEM;
	}
	ndev->port->mtu = VIRTIO_NET_MTU;
	ndev->port->link_changed = virtio_net_link_changed;
	ndev->port->can_receive = virtio_net_can_receive;
	ndev->port->switch2port_xfer = virtio_net_switch2port_xfer;
	ndev->port->priv = ndev;

	ndev->config.max_virtqueue_pairs = dev->guest->vcpu_count;
	/* Total queus: max_virtqueue_pairs * 2 + 1 this is nothing but
	 *  (RX VQ + TX VQ) * 2 + CONFIG VQ.
	 */
	ndev->vqs = vmm_zalloc(sizeof(struct virtio_net_queue) *
				((ndev->config.max_virtqueue_pairs * 2) + 1));
	if (!ndev->vqs) {
		vmm_netport_free(ndev->port);
		vmm_free(ndev);
		return VMM_ENOMEM;
	}
	ndev->config.status = VMM_VIRTIO_NET_S_LINK_UP;
	ndev->cq = ndev->config.max_virtqueue_pairs * 2;
	ndev->max_queues = ndev->config.max_virtqueue_pairs * 2 + 1;
	dev->emu_data = ndev;

	for (i = 0; i < ndev->max_queues; i++) {
		ndev->vqs[i].num = i;
		ndev->vqs[i].valid = 0;
		ndev->vqs[i].ndev = ndev;
		if (i == ndev->cq) {
			ndev->vqs[i].type = VIRTIO_NET_CTRL_QUEUE;
		} else {
			if (i % 2) {
				INIT_NETPORT_LAZY(&ndev->vqs[i].lazy,
						  ndev->port,
						  VIRTIO_NET_TX_LAZY_BUDGET,
						  &ndev->vqs[i],
						  virtio_net_tx_lazy);
				ndev->vqs[i].type = VIRTIO_NET_TX_QUEUE;
			} else {
				ndev->vqs[i].type = VIRTIO_NET_RX_QUEUE;
			}
		}
	}

	rc = vmm_netport_register(ndev->port);
	if (rc) {
		vmm_free(ndev->vqs);
		vmm_netport_free(ndev->port);
		vmm_free(ndev);
		return rc;
	}

	for (i = 0; i < 6; i++) {
		ndev->config.mac[i] = vmm_netport_mac(ndev->port)[i];
	}

	if (vmm_devtree_read_string(dev->edev->node,
				    "switch", &attr) == VMM_OK) {
		nsw = vmm_netswitch_find(attr);
		if (!nsw) {
			vmm_printf("%s: Cannot find netswitch \"%s\"\n",
					__func__, attr);
		} else {
			vmm_netswitch_port_add(nsw, ndev->port);
		}
	}

	return VMM_OK;
}

static void virtio_net_disconnect(struct vmm_virtio_device *dev)
{
	struct virtio_net_dev *ndev = dev->emu_data;

	vmm_netport_unregister(ndev->port);
	vmm_free(ndev->vqs);
	vmm_netport_free(ndev->port);
	vmm_free(ndev);
}

struct vmm_virtio_device_id virtio_net_emu_id[] = {
	{.type = VMM_VIRTIO_ID_NET},
	{ },
};

struct vmm_virtio_emulator virtio_net = {
	.name = "virtio_net",
	.id_table = virtio_net_emu_id,

	/* VirtIO operations */
	.get_host_features      = virtio_net_get_host_features,
	.set_guest_features     = virtio_net_set_guest_features,
	.init_vq                = virtio_net_init_vq,
	.get_pfn_vq             = virtio_net_get_pfn_vq,
	.get_size_vq            = virtio_net_get_size_vq,
	.set_size_vq            = virtio_net_set_size_vq,
	.notify_vq              = virtio_net_notify_vq,
	.status_changed         = virtio_net_status_changed,

	/* Emulator operations */
	.read_config = virtio_net_read_config,
	.write_config = virtio_net_write_config,
	.reset = virtio_net_reset,
	.connect = virtio_net_connect,
	.disconnect = virtio_net_disconnect,
};

static int __init virtio_net_init(void)
{
	return vmm_virtio_register_emulator(&virtio_net);
}

static void __exit virtio_net_exit(void)
{
	vmm_virtio_unregister_emulator(&virtio_net);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
