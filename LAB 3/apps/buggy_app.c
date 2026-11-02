#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static void array_bug(void)
{
    int arr[5] = {10, 20, 30, 40, 50};

    for (int i = 0; i <= 5; i++) {
        printf("arr[%d] = %d\n", i, arr[i]);
    }
}

static char *use_after_free_bug(void)
{
    char *buf = malloc(32);

    if (buf == NULL) {
        perror("malloc");
        return NULL;
    }

    strcpy(buf, "Embedded Linux LAB-03");
    free(buf);

    return buf;
}

static int integer_overflow_bug(int a, int b)
{
    return a * b;
}

int main(void)
{
    printf("=== Bug 1: Array out-of-bounds ===\n");
    array_bug();

    printf("\n=== Bug 2: Use-after-free ===\n");
    char *p = use_after_free_bug();

    if (p != NULL) {
        printf("Freed buffer: %s\n", p);
    }

    printf("\n=== Bug 3: Integer overflow ===\n");
    volatile int a = 100000;
    volatile int b = 100000;
    int result = integer_overflow_bug(a, b);

    printf("%d * %d = %d\n", a, b, result);

    return 0;
}
