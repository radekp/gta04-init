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
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <linux/fb.h>
#include <linux/input.h>

// Write string count bytes long to file
void writen_file(const char *path, const char *value, size_t count)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
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

int fb_draw(const char *path, int left, int top, int fbclear)
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

int main()
{
    int fd, i, ret;
    struct input_event ev;
    int x = -1;
    int y = -1;
    const char *choice = NULL;

    printf("gta04-init\n");
    fb_draw("/pic/sd.bmp", 56, 96, 1);
    fb_draw("/pic/nand.bmp", 56 + 240, 96, 0);
    fb_draw("/pic/1.bmp", 56, 320 + 96, 0);
    fb_draw("/pic/2.bmp", 56 + 240, 320 + 96, 0);

    // Mount the FAT boot partition
    for (i = 0;; i++) {
        ret = mount("/dev/mmcblk0p1", "/fat", "vfat", 0, NULL);
        if (ret == 0) {
            break;
        }
        if (i > 50) {
            perror("fat mount failed");
            write_file("/dev/tty0", "failed to mount fat\n");
            execl("/bin/busybox", "sh", (char *)(NULL));
        }
        usleep(100 * 1000);
    }
    // Let user select what he wants to boot
    if ((fd = open("/dev/input/event0", O_RDONLY)) < 0) {
        write_file("/dev/tty0", "failed to open touchscreen\n");
    }

    for (;;) {
        size_t rb = read(fd, &ev, sizeof(ev));

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
                fb_draw("/pic/nand.bmp", 176, 256, 1);
                choice = "/fat/gta04-init/nand.sh";
            } else {
                fb_draw("/pic/sd.bmp", 176, 256, 1);
                choice = "/fat/gta04-init/sd.sh";
            }
        } else if (x < 2000) {
            fb_draw("/pic/1.bmp", 176, 256, 1);
            choice = "/fat/gta04-init/1.sh";
        } else {
            fb_draw("/pic/2.bmp", 176, 256, 1);
            choice = "/fat/gta04-init/2.sh";
        }
        break;
    }

    printf("running busybox shell %s\n", choice);
    execl("/bin/busybox", "sh", choice, (char *)(NULL));

    return 0;
}
