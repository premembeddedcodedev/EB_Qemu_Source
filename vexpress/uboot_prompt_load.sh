fatload mmc 0:0 0x60000000 zImage
fatload mmc 0:0 0x62008000 initramfs.gz.uboot
fatload mmc 0:0 0x62600000 vexpress-v2p-ca9.dtb
bootz 0x60000000 0x62008000 0x62600000
