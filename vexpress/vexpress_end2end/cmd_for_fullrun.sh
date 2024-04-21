# Enable nfs mount like below

/etc/exports file
/srv/nfs/rootfs *(rw,sync,no_root_squash,no_subtree_check,insecure)

exportfs -av

#refresh the export above file
exportfs -r

# restart nfs
sudo /etc/init.d/nfs-kernel-server restart

#run command for qemu for vexpress

# Here .35 ip is tap0 interface

qemu-system-arm -M vexpress-a9 -m 512M -net nic -net tap,ifname=tap0 -kernel ../u-boot/u-boot -nographic -append "root=/dev/nfs nfsroot=192.168.29.35:/srv/nfs/rootfs,tcp rw ip=192.168.29.34::192.168.29.35:255.255.255.0 init=/sbin/init"

# set the u-boot commands for the nfs boot

setenv ipaddr 192.168.29.31;setenv serverip 192.168.29.35;ping 192.168.29.35;setenv bootargs console=ttyAMA0 root=/dev/nfs rootfstype=nfs ip=dhcp nfsroot=192.168.29.35:/srv/nfs/rootfs,tcp;tftp 0x62008000 zImage;bootz 0x62008000
