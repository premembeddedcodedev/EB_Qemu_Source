#!/usr/bin/bash -x

KERNEL=/home/praveenv/sambashare/Src/linux/arch/riscv/boot/Image
DRIVE=/home/praveenv/sambashare/Src/bkup_busybox/busybox/rootfs.img

qemu-system-riscv64 \ 
	-nographic -machine virt \
	-kernel	/home/praveenv/sambashare/Src/linux/arch/riscv/boot/Image \
	-append "root=/dev/vda rw console=ttyS0" \
	-drive file=/home/praveenv/sambashare/Src/bkup_busybox/busybox/rootfs.img,format=raw,id=hd0

#-device virtio-blk-device,drive=hd0 \
#$*
