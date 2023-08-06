./build/qemu-system-aarch64 -M raspi4b2g -serial stdio -kernel default_images/mount_0/kernel8.img -dtb default_images/mount_0/bcm2711-rpi-4-b.dtb -append 'printk.time=0 earlycon=pl011,0xfe201000 console=ttyAMA0'

./build/qemu-system-aarch64 -M raspi4b2g -m 2G -smp 4 -serial stdio -kernel default_images/mount_0/kernel8.img -dtb default_images/mount_0/bcm2711-rpi-4-b.dtb -append 'printk.time=0 earlycon=pl011,0xfe201000 console=ttyAMA0' -initrd initrd.img-4.14.0-3-arm64
