#include <stdlib.h>
#include <unistd.h>

extern void init_heap();

void _start()
{
    init_heap();

    int pid = 0;

    pid = fork();
    if (pid == 0)
    {
        execve("/usr/bin/aeui.exec", NULL, NULL);
        exit(-1);
    }

    /*// 启动 4 个测试进程
    for (int i = 0; i < 4; ++i) {
        pid = fork();
        if (pid == 0) {
            //char *argv[] = { "test.exec", NULL };
            execve("/usr/bin/test.exec", NULL, NULL);
            exit(-1);
        }
    }
    //*/

    // restart_shell:
    pid = fork();
    if (pid == 0)
    {
        execve("/usr/bin/shell.exec", NULL, NULL);
        exit(-1);
    }
    else
    {
        int status = 0;
        waitpid(pid, &status);
        // goto restart_shell;
    }

    while (1)
        __asm__ __volatile__("pause");
}
