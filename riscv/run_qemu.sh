tar -xvf Image.tar.gz
#qemu-system-riscv64 -nographic -machine virt -kernel ./Image -initrd ./initramfs.cpio.gz   -append "console=ttyS0"
gunzip rootfs.img.gz
qemu-system-riscv64 -nographic -machine virt -kernel ./Image -append "root=/dev/vda rw console=ttyS0" -drive file=./rootfs.img,format=raw,id=hd0
