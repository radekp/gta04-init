mkdir /sys
mount -t sysfs none /sys
/bin/ubiattach -m 4
mount -t ubifs ubi0:rootfs /mnt/rootfs
exec chroot /mnt/rootfs /sbin/init