# Reference https://medium.com/@kiky.tokamuro/creating-initramfs-5cca9b524b5a
find . -print0 | cpio --null -ov --format=newc > initramfs.cpio 
#gzip ./initramfs.cpio

qemu-system-arm -machine vexpress-a9 -m 1G -dtb ./vexpress-v2p-ca9.dtb -kernel ./zImage -initrd ./rootfs/initramfs.cpio -nographic -append "console=ttyAMA0"
