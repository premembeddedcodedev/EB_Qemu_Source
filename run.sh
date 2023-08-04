unxz qemu-bin/qemu-system-aarch64.xz
unxz sd.img.xz
sudo -s ./qemu-bin/qemu-system-aarch64 -M raspi3b  -kernel ./images/u-boot.bin -serial null -serial stdio -sd ./sd.img
