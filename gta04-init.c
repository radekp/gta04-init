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
#include <sys/stat.h>
#include <sys/mount.h>
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

const char *menu =
    "                   GTA04 boot\n"
    "\n"
    "+===========================+==============================+\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|        SSSS   DDDD        |  N   N   AAA   N   N  DDDD   |\n"
    "|       S       D   D       |  NN  N  A   A  NN  N  D   D  |\n"
    "|        SSSS   D   D       |  N N N  AAAAA  N N N  D   D  |\n"
    "|            S  D   D       |  N  NN  A   A  N  NN  D   D  |\n"
    "|        SSSS   DDDD        |  N   N  A   A  N   N  DDDD   |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "+===========================+==============================+\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|             1             |            2222              |\n"
    "|           1 1             |           2    2             |\n"
    "|         1   1             |               2              |\n"
    "|             1             |             2                |\n"
    "|             1             |           222222             |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "|                           |                              |\n"
    "+===========================+==============================+\n";

int main()
{
    int fd, i, ret;
    struct input_event ev;
    int x = -1;
    int y = -1;
    const char *choice = NULL;

    printf("gta04-init\n");
    write_file("/dev/tty0", menu);

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
                choice = "/fat/gta04-init/nand.sh";
            } else {
                choice = "/fat/gta04-init/sd.sh";
            }
        } else if (x < 2000) {
            choice = "/fat/gta04-init/1.sh";
        } else {
            choice = "/fat/gta04-init/2.sh";
        }
        break;
    }

    printf("running busybox shell %s\n", choice);
    execl("/bin/busybox", "sh", choice, (char *)(NULL));

    return 0;
}
