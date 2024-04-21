# Refer link : https://server-tutorials.net/how-to-install-and-configure-tftp-server-on-ubuntu-20-04/
sudo apt-get install tftpd-hpa
ls /var/lib/tftpboot/
sudo -s rm /var/lib/tftpboot/u-boot.bin
cp linux/arch/arm/boot/zImage /var/lib/tftpboot/
sudo systemctl restart tftpd-hpa
#sudo chown -R nobody:nogroup /tftp
#sudo chmod -R 777 /tftp
sudo vim /etc/default/tftpd-hpa # add --create to the flags
sudo systemctl start tftpd-hpa
sudo systemctl enable tftpd-hpa
sudo ufw allow tftp
