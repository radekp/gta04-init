/*
 * GTA04 init for initramfs bootmenu
 * Copyright (c) 2012 Radek Polak
 *
 * gta04-init is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gta04-init is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with gta04-init; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <linux/fb.h>
#include <linux/input.h>

#include "run-init.h"

// Write string count bytes long to file
void writen_file(const char *path, const char *value, size_t count)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror(path);
        return;
    }
    write(fd, value, count);
    close(fd);
}

void write_file(const char *path, const char *value)
{
    writen_file(path, value, strlen(value));
}

#define BMP_WIDTH 128
#define BMP_HEIGHT 128
#define BMP_PIX_BYTES 4
#define BMP_LINE_LEN (BMP_WIDTH * BMP_PIX_BYTES)
#define BMP_SIZE (BMP_WIDTH * BMP_HEIGHT * BMP_PIX_BYTES)

int bmp_draw(const char *path, int left, int top, int fbclear)
{
    int filefd = -1;
    int fbfd = -1;
    char *filemap = NULL;
    char *fbmap = NULL;
    size_t fblen;
    struct fb_var_screeninfo screen_info;
    struct fb_fix_screeninfo fixed_info;
    struct stat st;
    int y;

    filefd = open(path, O_RDONLY);
    if (filefd == -1) {
        perror("open failed");
        goto cleanup;
    }

    if (fstat(filefd, &st) < 0) {
        perror("fstat failed");
        goto cleanup;
    }

    filemap = mmap(0, st.st_size, PROT_READ, MAP_SHARED, filefd, 0);
    if (filemap == MAP_FAILED) {
        close(filefd);
        perror("file mmap failed");
        goto cleanup;
    }

    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0) {
        perror("fb open failed");
        goto cleanup;
    }

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &screen_info) ||
        ioctl(fbfd, FBIOGET_FSCREENINFO, &fixed_info)) {
        perror("fb info failed");
        goto cleanup;
    }

    fblen = screen_info.yres_virtual * fixed_info.line_length;
    fbmap = mmap(NULL, fblen, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    if (fbmap == MAP_FAILED) {
        perror("fb mmap failed");
        goto cleanup;
    }

    if (fbclear) {
        memset(fbmap, 0, fblen);
    }

    for (y = 0; y < BMP_HEIGHT; y++) {
        memmove(fbmap + left * BMP_PIX_BYTES +
                (top + y) * fixed_info.line_length,
                filemap + st.st_size - (y + 1) * BMP_LINE_LEN, BMP_LINE_LEN);
    }

cleanup:

    if (fbmap && fbmap != MAP_FAILED)
        munmap(fbmap, fblen);

    if (fbfd >= 0)
        close(fbfd);

    if (filemap && filemap != MAP_FAILED)
        munmap(filemap, st.st_size);

    if (filefd >= 0)
        close(filefd);

    return 0;
}

static int mount_fs(const char *fstype, const char *device,
                    const char *mountpoint)
{
    printf("mounting %s %s %s\n", fstype, device, mountpoint);
    if (mount(device, mountpoint, fstype, 0, NULL) == 0) {
        return 0;
    }
    perror("mount failed");
    return -1;
}

// Compare src and dst files. Return 0 if their content is same. Copy src to
// dst and return 1 if they are different. On error returns negative error
// code.
static int update_file(const char *src, const char *dst)
{
    int res = 0;
    int src_fd = -1;
    int dst_fd = -1;
    struct stat st;
    int count, src_rb, dst_b;
    char src_buf[4096];
    char dst_buf[4096];

    if ((src_fd = open(src, O_RDONLY)) < 0) {
        goto err_open_src;
    }

    if ((dst_fd = open(dst, O_RDWR | O_CREAT, 00644)) < 0) {
        goto err_open_dst;
    }

    if (fstat(src_fd, &st) < 0) {
        goto err_stat;
    }
    // Make sure src and dst have same length
    if (ftruncate(dst_fd, st.st_size) < 0) {
        goto err_truncate;
    }

    for (;;) {
        src_rb = read(src_fd, src_buf, 4096);
        if (src_rb < 0) {
            goto err_read_src;
        }
        if (src_rb == 0) {
            break;
        }
        dst_b = 0;
        do {
            if ((count = read(dst_fd, dst_buf + dst_b, src_rb - dst_b)) < 0) {
                goto err_read_dst;
            }
            dst_b += count;
        } while (dst_b < src_rb);

        if (memcmp(src_buf, dst_buf, src_rb) == 0) {
            continue;
        }
        res = 1;
        if (lseek(dst_fd, -src_rb, SEEK_CUR) < 0) {
            goto err_seek;
        }
        dst_b = 0;
        do {
            if ((count = write(dst_fd, src_buf + dst_b, src_rb - dst_b)) < 0) {
                goto err_write;
            }
            dst_b += count;
        }
        while (dst_b < src_rb);
    }

cleanup:
    if (src_fd > 0) {
        close(src_fd);
    }
    if (dst_fd > 0) {
        close(dst_fd);
    }
    return res;

err_src:
    res = -1;
    printf("file %s ", src);
    goto cleanup;

err_dst:
    res = -2;
    printf("file %s ", dst);
    goto cleanup;

err_open_src:
    perror("open failed");
    goto err_src;

err_stat:
    perror("stat failed");
    goto err_src;

err_truncate:
    perror("truncate failed");
    goto err_dst;

err_open_dst:
    perror("open failed");
    goto err_dst;

err_read_src:
    perror("read failed");
    goto err_src;

err_read_dst:
    perror("read failed");
    goto err_dst;

err_seek:
    perror("seek failed");
    goto err_dst;

err_write:
    perror("write failed");
    goto err_dst;
}

static void run_rootfs_init(int check_kernel, const char *bootdev,
                            const char *bootdir)
{
    char *argv[2];
    argv[0] = "/sbin/init";
    argv[1] = NULL;
    const char *err;
    char dev_path[256];
    char logo_path[256];
    char uimage_path[256];
    char chrootdir[256];

    snprintf(dev_path, 256, "/real-root%s/dev", bootdir);
    snprintf(logo_path, 256, "/real-root%s/boot/logo.bmp", bootdir);
    snprintf(uimage_path, 256, "/real-root%s/boot/uImage", bootdir);
    snprintf(chrootdir, 256, ".%s", bootdir);

    printf("dev_path=%s\n", dev_path);
    printf("logo_path=%s\n", logo_path);
    printf("uimage_path=%s\n", uimage_path);
    printf("chrootdir=%s\n", chrootdir);

    // Check if we have the same kernel as on /real-root/boot
    // If not copy it to uImage and reboot
    if (check_kernel && update_file(uimage_path, "/fat/uImage") > 0) {
        printf("updated kernel from real-root and rebooting\n");
        if (umount("/fat")) {
            perror("umount /fat");
        }
        if (umount("/real-root")) {
            perror("umount /real-root");
        }
        reboot(LINUX_REBOOT_CMD_RESTART);
        sleep(60);
        return;
    }
    // Remove boodev. Distro should supply it on reboot. If not we fallback to
    // bootmenu.
    if (unlink("/fat/gta04-init/bootdev")) {
        perror("boodev unlink failed");
    }
    // Draw distribution logo if supplied
    bmp_draw(logo_path, 176, 256, 1);

    // Mount devtmpfs on real-root. During normal boot it is mounted
    // automatically by kernel, we do it too to be compatible.
    mount_fs("devtmpfs", "none", dev_path);

    err = run_init("/real-root", chrootdir, "/dev/console", "/sbin/init", argv);
    printf("run_init error: %s: %s\n", err, strerror(errno));
}

int main()
{
    int fd = -1;
    int ret, i;
    size_t rb;
    struct input_event ev;
    int x = -1;
    int y = -1;
    pid_t pid;
    char bootdevbuf[256];
    const char *choice_1 = "/fat/gta04-init/1.sh";
    const char *choice_2 = "/fat/gta04-init/2.sh";
    const char *choice_sd = "/dev/mmcblk0p2";
    const char *choice_nand = "ubi0:rootfs";
    const char *bootdev = NULL;
    char *bootdir = "";         // optional directory to chroot to

    printf("gta04-init\n");
    bmp_draw("/pic/sd.bmp", 56, 96, 1);
    bmp_draw("/pic/nand.bmp", 56 + 240, 96, 0);
    bmp_draw("/pic/1.bmp", 56, 320 + 96, 0);
    bmp_draw("/pic/2.bmp", 56 + 240, 320 + 96, 0);

    // Mount fat
    for (i = 0;; i++) {
        if (mount_fs("vfat", "/dev/mmcblk0p1", "/fat") < 0) {
            if (i > 5) {
                break;
            }
            sleep(1);
            continue;
        }
        if ((fd = open("/fat/gta04-init/bootdev", O_RDONLY)) < 0) {
            perror("/fat/gta04-init/bootdev");
            break;
        }
        // Read config - e.g. /dev/mmcblk0p2 /shr
        rb = read(fd, bootdevbuf, 255);
        if (rb > 0) {
            printf("bootdevbuf=%s\n", bootdevbuf);
            bootdev = bootdevbuf;
            bootdevbuf[rb] = 0;
            while (rb > 0 && (bootdevbuf[rb - 1] <= 32)) {  // remove whitespaces and new lines
                rb--;
                bootdevbuf[rb] = 0;
            }
            if ((bootdir = strchr(bootdev, ' ')) != NULL) {
                *bootdir = 0;
                bootdir++;
            }
            printf("bootdev=%s\n", bootdev);
            printf("bootdir=%s\n", bootdir);
        }
        close(fd);
        break;
    }

    // Let user select what he wants to boot
    while (bootdev == NULL) {
        if (fd < 0 && (fd = open("/dev/input/event0", O_RDWR)) < 0) {
            write_file("/dev/tty0", "failed to open touchscreen\n");
            break;
        }

        rb = read(fd, &ev, sizeof(ev));
        if (rb < (int)sizeof(struct input_event)) {
            perror("short read");
        }

        if (ev.type == EV_ABS && ev.code == ABS_X) {
            printf("ABS_X type=%d code=%d value=%d\n", ev.type, ev.code,
                   ev.value);
            x = ev.value;
        }
        if (ev.type == EV_ABS && ev.code == ABS_Y) {
            printf("ABS_Y type=%d code=%d value=%d\n", ev.type, ev.code,
                   ev.value);
            y = ev.value;
        }
        if (x < 0 || y < 0) {
            continue;
        }
        printf("x=%d, y=%d\n", x, y);
        if (y > 2000) {
            if (x > 2000) {
                bootdev = choice_nand;
            } else {
                bootdev = choice_sd;
            }
        } else if (x < 2000) {
            bootdev = choice_1;
        } else {
            bootdev = choice_2;
        }
        break;
    }

    // Run 1.sh or 2.sh from FAT partition, busybox must be there
    if (bootdev == choice_1 || bootdev == choice_2) {
        bmp_draw(bootdev == choice_1 ? "/pic/1.bmp" : "/pic/2.bmp", 176, 256,
                 1);
        printf("running /fat/gta04-init/busybox sh %s\n", bootdev);
        if (execl("/fat/gta04-init/busybox", "sh", bootdev, (char *)(NULL))
            == -1) {
            perror("busybox exec failed");
        }
        return 0;
    }
    // SD card
    if ((strstr(bootdev, "ubi0:") == NULL) &&
        ((mount_fs("ext4", bootdev, "/real-root") >= 0) ||
         (mount_fs("ext3", bootdev, "/real-root") >= 0) ||
         (mount_fs("btrfs", bootdev, "/real-root") >= 0))) {
        run_rootfs_init(1, bootdev, bootdir);
        return 0;
    }
    // Boot from NAND if chosen or SD mount failed
    if (mkdir("/sys", 755) == -1) {
        perror("mkdir /sys");
    }
    mount_fs("sysfs", "none", "/sys");
    pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return 0;
    }
    if (pid == 0) {
        if (execl
            ("/bin/ubiattach", "/bin/ubiattach", "-m", "4",
             (char *)(NULL)) == -1) {
            perror("ubiattach -m 4 failed");
            return 1;
        }
        return 0;
    }
    for (;;) {
        waitpid(pid, &ret, 0);
        if (mount_fs("ubifs", bootdev, "/real-root") >= 0) {
            run_rootfs_init(0, bootdev, bootdir);
        }
    }
    return 0;
}
