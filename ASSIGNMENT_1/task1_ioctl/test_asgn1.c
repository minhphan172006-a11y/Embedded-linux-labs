#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "asgn1_ioctl.h"

int main(void)
{
    int fd;
    int mode;
    char buffer[1024] = {0};
    char version[64] = {0};
    struct asgn1_stats stats;
    ssize_t n;

    printf("===== ASSIGNMENT 01 - TASK 1 TEST =====\n");

    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open /dev/asgn1");
        return 1;
    }

    /* Test 1: Reset buffer */
    printf("\n[TEST 1] RESET BUFFER\n");
    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0)
        perror("RESET_BUFFER");
    else
        printf("RESET_BUFFER: PASS\n");

    /* Test 2: Echo mode */
    printf("\n[TEST 2] ECHO MODE\n");
    mode = 1;
    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0)
        perror("SET_MODE ECHO");

    if (write(fd, "Hello Embedded Linux", 21) < 0) {
        perror("write ECHO");
    } else {
        lseek(fd, 0, SEEK_SET);
        n = read(fd, buffer, sizeof(buffer) - 1);

        if (n >= 0) {
            buffer[n] = '\0';
            printf("Read data: [%s]\n", buffer);
            printf("Echo mode: %s\n",
                   strcmp(buffer, "Hello Embedded Linux") == 0
                   ? "PASS" : "FAIL");
        } else {
            perror("read ECHO");
        }
    }

    /* Test 3: Kernel mode */
    printf("\n[TEST 3] KERNEL MODE\n");
    memset(buffer, 0, sizeof(buffer));
    mode = 0;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0)
        perror("SET_MODE KERNEL");

    if (write(fd, "Hello Driver", 12) < 0) {
        perror("write KERNEL");
    } else {
        lseek(fd, 0, SEEK_SET);
        n = read(fd, buffer, sizeof(buffer) - 1);

        if (n >= 0) {
            buffer[n] = '\0';
            printf("Read data: [%s]\n", buffer);
            printf("Kernel mode: %s\n",
                   strcmp(buffer, "[KERNEL]Hello Driver") == 0
                   ? "PASS" : "FAIL");
        } else {
            perror("read KERNEL");
        }
    }

    /* Test 4: Get statistics */
    printf("\n[TEST 4] GET STATISTICS\n");
    memset(&stats, 0, sizeof(stats));

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("GET_STATS");
    } else {
        printf("Open count      : %u\n", stats.open_count);
        printf("Write count     : %u\n", stats.write_count);
        printf("Read count      : %u\n", stats.read_count);
        printf("Buffer length   : %u\n", stats.buffer_len);
        printf("Last write size : %u\n", stats.last_write_size);
    }

    /* Test 5: Get version */
    printf("\n[TEST 5] GET VERSION\n");
    if (ioctl(fd, ASGN1_GET_VERSION, version) < 0)
        perror("GET_VERSION");
    else
        printf("Driver version: %s\n", version);

    /* Test 6: Invalid mode */
    printf("\n[TEST 6] INVALID MODE\n");
    mode = 99;
    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0)
        printf("Invalid mode rejected: %s\n",
               errno == EINVAL ? "PASS" : strerror(errno));
    else
        printf("Invalid mode: FAIL\n");

    /* Test 7: Reset and verify empty buffer */
    printf("\n[TEST 7] RESET AND VERIFY EMPTY BUFFER\n");
    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) {
        perror("RESET_BUFFER");
    } else {
        memset(buffer, 0, sizeof(buffer));
        lseek(fd, 0, SEEK_SET);
        n = read(fd, buffer, sizeof(buffer) - 1);

        if (n == 0)
            printf("Empty buffer after reset: PASS\n");
        else
            printf("Empty buffer after reset: FAIL (read=%zd)\n", n);
    }

    close(fd);

    printf("\n===== TEST COMPLETED =====\n");
    return 0;
}
