echo "booting QtMoko v42 squashfs image"

echo "populating bin"

# populate /bin with busybox symlinks for useful commands
/fat/gta04-init/busybox ln -s /fat/gta04-init/busybox /bin/mount
/fat/gta04-init/busybox ln -s /fat/gta04-init/busybox /bin/chroot
/fat/gta04-init/busybox ln -s /fat/gta04-init/busybox /bin/mkdir
/fat/gta04-init/busybox ln -s /fat/gta04-init/busybox /bin/cp
/fat/gta04-init/busybox ln -s /fat/gta04-init/busybox /bin/sleep

# gta04-init mounts /dev/mmcblk0p1 as /fat for us
echo "mounting squashfs"
mount -t squashfs -o loop /fat/qtmoko-debian-gta04-v42.squashfs /real-root

echo "mounting sysfs"
mount -t sysfs none /real-root/sys

echo "mounting proc"
mount -t proc none /real-root/proc

echo "mounting dev"
mount -t devtmpfs none /real-root/dev

echo "mounting tmp"
mount -t tmpfs none /real-root/tmp

echo "copying files to var"
mkdir -p /real-root/tmp/var
cp -r /real-root/var/* /real-root/tmp/var

echo "binding var"
mount -o bind /real-root/tmp/var /real-root/var

/fat/gta04-init/busybox ls /real-root/var

/fat/gta04-init/busybox sleep 5

echo "binding home"
mkdir -p /fat/qtmoko-squashfs/home
mount -o bind /fat/qtmoko-squashfs/home /real-root/home

exec chroot /real-root /sbin/init.sh
