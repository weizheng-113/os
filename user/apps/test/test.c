#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main() {
    printf("PID %d running\n", getpid());
    //usleep(2000000); // 睡眠2秒，模拟运行
    int i=0;
    while (i < 100000000ULL) i++; // 简单延时，具体数值可根据实际调整
    printf("PID %d exiting\n", getpid());
    return 0;
}