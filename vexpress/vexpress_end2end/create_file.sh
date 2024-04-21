# rootfs folder is responsible
# refer link -> 
sudo mknod /srv/nfs/rootfs/dev/tty1 c 4 1
sudo mknod /srv/nfs/rootfs/dev/tty2 c 4 2
sudo mknod /srv/nfs/rootfs/dev/tty3 c 4 3
sudo mknod /srv/nfs/rootfs/dev/tty4 c 4 4
sudo mknod /srv/nfs/rootfs/dev/console c 5 1
sudo mkdir -p /srv/nfs/rootfs/tmp/
sudo mknod /srv/nfs/rootfs/dev/ttyAMA0 c 204 64
sudo mknod /srv/nfs/rootfs/dev/null c 1 3
