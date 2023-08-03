kernel: qemu-system-aarch64 -kernel /mnt/mount_0/vmlinuz-4.14.0-3-arm64 -initrd /mnt/mount_0/initrd.img-4.14.0-3-arm64 -dtb /mnt/mount_0/bcm2837-rpi-3-b.dtb -M raspi3b -m 1024 -append "rw earlycon=pl011,0x3f201000 console=ttyAMA0 loglevel=8 root=/dev/mmcblk0p2 fsck.repair=yes net.ifnames=0 rootwait memtest=1" -drive file=2018-01-08-raspberry-pi-3-buster-PREVIEW.img,format=raw,if=sd -nographic

u-boot: qemu-system-aarch64 -M raspi3b  -kernel ./u-boot/u-boot.bin -serial null -serial stdio

qemu-system-aarch64 -M raspi3b  -kernel ./u-boot/u-boot.bin -serial null -serial stdio -sd 2018-01-08-raspberry-pi-3-buster-PREVIEW.img

setenv bootargs "8250.nr_uarts=1 root=/dev/sda2 rootwait console=ttyS0,115200n8"
setenv bootargs "8250.nr_uarts=1 console=ttyS0,115200 root=/dev/mmcblk0p2 rootwait rw"
fatload mmc 0:1 ${kernel_addr_r} Image
fatload mmc 0:1 ${fdt_addr_r} bcm2710-rpi-3-b-plus.dtb
booti ${kernel_addr_r} - ${fdt_addr_r}

fatload mmc 0:1 ${ramdisk_addr_r} uRamdisk
booti ${kernel_addr_r} ${ramdisk_addr_r} ${fdt_addr_r}

Adding usb-devices:
qemu-system-aarch64 -M raspi3b -cpu cortex-a53 -kernel ./u-boot/u-boot.bin -serial null -serial stdio -hda 2018-01-08-raspberry-pi-3-buster-PREVIEW.img -m 1G -smp 4 -usb -device usb-mouse -device usb-kbd -device usb-tablet
qemu-system-aarch64 -M raspi3b -cpu cortex-a53 -kernel ./u-boot/u-boot.bin -serial null -serial stdio -hda 2018-01-08-raspberry-pi-3-buster-PREVIEW.img -m 1G -smp 4 -usb -device usb-mouse -device usb-kbd -device usb-tablet -device usb-host,vendorid=0x08bb,productid=0x29c6 -device usb-host,vendorid=0x08bb,productid=0x29c6
