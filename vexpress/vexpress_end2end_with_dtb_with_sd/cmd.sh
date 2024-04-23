qemu-system-arm -machine vexpress-a9 -cpu cortex-a9 -m 1G -kernel ./u-boot -sd ./a9rootfs.ext3 -nographic -append "root=/dev/mmcblk0 console=ttyAMA0"
ext4load mmc 0:0 0x60100000 zImage;ext4load mmc 0:0 0x62000000 vexpress-v2p-ca9.dtb;setenv bootargs console=ttyAMA0 root=/dev/mmcblk0;bootz 0x60100000 - 0x62000000
