#create sh file and refer below linke : https://stackoverflow.com/questions/67522041/qemu-network-not-working-needed-for-tftp-booting

set -x

ETH=enp2s0   #Ethernet interface in your HOST
USR=junno    #User name- whoami

if test -z $1 ; then
        echo need a arg: down/up
        exit
fi

if [ "up" = $1 ] ; then

        brctl addbr br0

        ifconfig $ETH down
        brctl addif br0 $ETH

        #Close Spanning Tree Protocol
        brctl stp br0 off

        ifconfig br0 10.8.8.1 netmask 255.255.255.0 promisc up  #3
        ifconfig $ETH 10.8.8.10 netmask 255.255.255.0 promisc up #2

        tunctl -t tap0 -u $USR
        ifconfig tap0 10.8.8.11 netmask 255.255.255.0 promisc up #4
        brctl addif br0 tap0
else
        ifconfig tap0 down
        brctl delif br0 tap0

        ifconfig $ETH down
        brctl delif br0 enp2s0

        ifconfig br0 down
        brctl delbr br0

        ifconfig $ETH 10.8.8.10 netmask 255.255.255.0  #2
fi

#Add below lines $vi ~/.bashrc

alias ifup_tap='sudo ~/ProjectWork/bin/tuntap.sh up'
alias ifdown_tap='sudo ~/ProjectWork/bin/tuntap.sh down'

# Host

~/ProjectWork/bin$ ifdown_tap
~/ProjectWork/bin$ ifup_tap

# Qemu command to run 

~/ProjectWork/bin$ sudo qemu-system-arm -M vexpress-a9 -m 512 -nographic -net nic -net tap,ifname=tap0,script=no -kernel u-boot.elf

# commands in u-boot

=>setenv serverip 10.8.8.1; setenv ipaddr 10.8.8.2; setenv netmask 255.255.255.0;
=>ping 10.8.8.1
 host 10.8.8.1 is alive

//For initramfs
=>setenv bootargs "console=ttyAMA0,115200 root=/dev/ram rw"
=>tftp 0x60000000 zImage-initramfs-qemuarma9.bin
=>tftp 0x70000000 vexpress-v2p-ca9-qemuarma9.dtb
=>bootz 0x60000000 - 0x70000000

