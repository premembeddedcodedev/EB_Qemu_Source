sudo losetup /dev/loop35 sd.img -o 1048576 --sizelimit 31456768
sudo -s mount /dev/loop35 mnt/boot/
