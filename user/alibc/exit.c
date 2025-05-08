#include <libsyscall.h>
#include <aether/window.h>

extern char *proc_name;

void exit(int code)
{
    enter_syscall(code, 0, 0, 0, 0, SYS_EXIT);
}

void abort()
{
    exit(-1);
}
