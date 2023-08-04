dd if=/dev/zero of=sd.img bs=1M count=128

# Do the bootloader thing. Put the file from position 8192 bytes into the image
echo Copying bootloader
sudo dd if=ext/Image of=sd.img bs=1k seek=8 conv=notrunc

(
echo n
echo p
echo 1
echo
echo +30M
echo n
echo p
echo 2
echo
echo +80M
echo n
echo p
echo
echo
echo
echo t
echo 1
echo b
echo t
echo 3
echo b
echo a
echo 1
echo w
) | fdisk sd.img > /dev/null

sleep 1

export start0=`echo p | sudo fdisk sd.img | grep sd.img1 | awk ' { print $3 } '`
export end0=`echo p | sudo fdisk sd.img | grep sd.img1 | awk ' { print $4 } '`
export start1=`echo p | sudo fdisk sd.img | grep sd.img2 | awk ' { print $2 } '`
export end1=`echo p | sudo fdisk sd.img | grep sd.img2 | awk ' { print $3 } '`
export start2=`echo p | sudo fdisk sd.img | grep sd.img3 | awk ' { print $2 } '`
export end2=`echo p | sudo fdisk sd.img | grep sd.img3 | awk ' { print $3 } '`

mkdir -p mnt
mkdir -p mnt/boot
mkdir -p mnt/rootfs
mkdir -p mnt/media

echo Creating boot partition from $start0 to $end0
export sectors0=`expr $end0 - $start0`
export size0=`expr $sectors0 \* 512`
export offset0=`expr $start0 \* 512`
sudo losetup -d /dev/loop34 > /dev/null 2>&1
echo Creating boot partition from $offset0 to $size0
sudo losetup /dev/loop34 sd.img -o $offset0 --sizelimit $size0
sudo mkfs.vfat /dev/loop34
sudo mount /dev/loop34 mnt/boot

echo Creating rootfs partition from $start1 to $end1
export sectors1=`expr $end1 - $start1`
export size1=`expr $sectors1 \* 512`
export offset1=`expr $start1 \* 512`
sudo losetup -d /dev/loop35 > /dev/null 2>&1
echo Creating roots partition from $offset1 to $size1
sudo losetup /dev/loop35 sd.img -o $offset1 --sizelimit $size1
sudo mkfs.ext2 /dev/loop35
sudo mount /dev/loop35 mnt/rootfs

echo Creating media partition from $start0 to $end0
sudo losetup -d /dev/loop36 > /dev/null 2>&1
export sectors2=`expr $end2 - $start2`
export size2=`expr $sectors2 \* 512`
export offset2=`expr $start2 \* 512`
echo Creating media partition from $offset2 to $size2
sudo losetup /dev/loop36 sd.img -o $offset2 --sizelimit $size2
sudo mkfs.vfat /dev/loop36
sudo mount /dev/loop36 mnt/media
