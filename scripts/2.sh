echo "Hi, this is gta04-initramfs" > /dev/tty0
echo "" > /dev/tty0
echo "By default we can boot from SD and NAND/ubifs" > /dev/tty0
echo "but you can extend booting to other devices." > /dev/tty0
echo "" > /dev/tty0
echo "Please check 1.sh script for squashfs booting" > /dev/tty0
echo "example." > /dev/tty0
echo "" > /dev/tty0
echo "1.sh script must be on boot mmcblk0p1 partition" > /dev/tty0
echo "in gta04-init directory." > /dev/tty0
echo "" > /dev/tty0
echo "You should find there also statically compiled" > /dev/tty0
echo "busybox for your needs." > /dev/tty0
echo "" > /dev/tty0
echo "You can now access initamfs shell on serial line" > /dev/tty0
exec /fat/gta04-init/busybox sh