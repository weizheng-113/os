#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int sleep_time = 1;
    if (argc > 1) sleep_time = atoi(argv[1]);
    usleep(sleep_time*1000);
    return 0;
}