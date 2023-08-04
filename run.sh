
unxz sd.img.xz
sudo -s ../qemu_rpi4/build/qemu-system-aarch64 -M raspi3b  -kernel ./images/u-boot.bin -serial null -serial stdio -sd ./sd.img
