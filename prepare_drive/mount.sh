#boot folder extract
sudo losetup /dev/loop35 sd.img -o 1048576 --sizelimit 31456768
sudo -s mount /dev/loop35 mnt/boot/

#Rootfs folder extract
sudo losetup /dev/loop36 sd.img -o 32505856 --sizelimit 83885568
sudo -s mount /dev/loop36 mnt/rootfs/

#media folder extract
sudo losetup /dev/loop37 sd.img -o 116391936 --sizelimit 17825280
sudo -s mount /dev/loop37 mnt/media/
