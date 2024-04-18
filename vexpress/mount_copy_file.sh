dd if=/dev/zero of=sd.img bs=4096 count=4096
mkfs.vfat sd.img
mount sd.img /mnt/tmp -o loop,rw
sudo -s mount sd.img /mnt/tmp -o loop,rw
cp arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb /mnt/tmp/
sudo cp arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb /mnt/tmp/
sudo cp arch/arm/boot/zImage /mnt/tmp/
sudo -s umount /mnt/tmp
