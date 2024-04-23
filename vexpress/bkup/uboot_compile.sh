#remove libfdt to compile the u-boot
sudo apt-get autoremove libfdt-dev
make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabi- vexpress_ca9x4_defconfig
