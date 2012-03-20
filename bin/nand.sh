/bin/ubiattach /dev/ubi_ctrl -m 4
mount -t ubifs ubi0:rootfs /mnt/rootfs
exec chroot /mnt/rootfs /sbin/init
