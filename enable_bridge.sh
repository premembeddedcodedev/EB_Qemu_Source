ETH=enp0s3   #Ethernet interface in your HOST
USR=praveenv    #User name- whoami

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

        ifconfig br0 192.168.29.33 netmask 255.255.255.0 promisc up  #3
        ifconfig $ETH 192.168.29.34 netmask 255.255.255.0 promisc up #2

        tunctl -t tap0 -u $USR
        ifconfig tap0 192.168.29.35 netmask 255.255.255.0 promisc up #4
        brctl addif br0 tap0
else
        ifconfig tap0 down
        brctl delif br0 tap0

        ifconfig $ETH down
        brctl delif br0 enp2s0

        ifconfig br0 down
        brctl delbr br0

        ifconfig $ETH 192.168.29.34 netmask 255.255.255.0  #2
fi
