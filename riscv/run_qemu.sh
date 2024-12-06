tar -xvf Image.tar.gz

qemu-system-riscv64 -nographic -machine virt -kernel /home/praveenv/sambashare/Src/linux/arch/riscv/boot/Image -initrd /home/praveenv/sambashare/Src/busybox/initramfs/initramfs.cpio.gz -append "console=ttyS0"
#qemu-system-riscv64 -nographic -machine virt -kernel /home/praveenv/sambashare/EB_Qemu_Source/riscv/Image -append "root=/dev/vda rw console=ttyS0" -drive file=/home/praveenv/sambashare/EB_Qemu_Source/riscv/rootfs.img,format=raw,id=hd0

