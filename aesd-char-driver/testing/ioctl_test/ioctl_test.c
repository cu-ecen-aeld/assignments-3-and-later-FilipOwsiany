#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "aesd_ioctl.h"

int main() {
    int fd = open("/dev/aesdchar", O_RDWR);

    printf("Opening device file...\n");

    if (fd < 0) 
    { 
        perror("open"); 
        return 1; 
    }

    printf("Device file opened successfully.\n");

    write(fd, "test1\n", sizeof("test1\n") - 1);
    write(fd, "test2\n", sizeof("test2\n") - 1);
    write(fd, "test3\n", sizeof("test3\n") - 1);
    write(fd, "test4\n", sizeof("test4\n") - 1);

    struct aesd_seekto seekto;
    seekto.write_cmd = 1;        //point to the second write command 
    seekto.write_cmd_offset = 3; //point to the 4th byte of the second write command

    printf("Seeking to write_cmd: %u, write_cmd_offset: %u\n", seekto.write_cmd, seekto.write_cmd_offset);

    if (ioctl(fd, AESDCHAR_IOCSEEKTO, &seekto) < 0) 
    {
        perror("ioctl");
        return 1;
    }

    printf("Seek operation completed successfully.\n");

    char buffer[100] = {0};

    ssize_t bytes_read = 0;

    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes_read] = '\0'; // Null-terminate the string
        printf("Read data: %s\n", buffer);
    }
    
    buffer[bytes_read] = '\0'; // Null-terminate the string
    printf("Read data: %s\n", buffer);

    close(fd);

    printf("Device file closed.\n");

    return 0;
}