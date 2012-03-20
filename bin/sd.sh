mount -t ext3 /dev/mmcblk0p2 /mnt/rootfs
exec chroot /mnt/rootfs /sbin/init
