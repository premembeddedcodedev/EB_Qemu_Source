#qemu-system-arm -machine vexpress-a9 -cpu cortex-a9 -m 128M -dtb arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb -kernel arch/arm/boot/zImage -nographic
#qemu-system-arm -machine vexpress-a9 -cpu cortex-a9 -m 128M -dtb arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb -kernel ../u-boot/u-boot -nographic
#qemu-system-arm -machine vexpress-a9 -cpu cortex-a9 -m 512M -dtb arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb -kernel ../u-boot/u-boot -sd sd.img -nographic
qemu-system-arm -machine vexpress-a9 -cpu cortex-a9 -m 1G -dtb arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb -kernel ../u-boot/u-boot -sd sd.img -nographic -append "root=/dev/sda console=ttyAMA0" -no-reboot -net nic
#qemu-system-arm -M vexpress-a9 -kernel arch/arm/boot/zImage -nographic

