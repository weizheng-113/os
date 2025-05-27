#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int sleep_time = 2;
    usleep(sleep_time*1000);
    return 0;
}