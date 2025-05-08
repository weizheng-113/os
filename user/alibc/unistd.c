#include <unistd.h>

int open(const char *name, int mode, int flags)
{
    return (int)enter_syscall((uint64_t)name, mode, flags, 0, 0, SYS_OPEN);
}

int close(int fd)
{
    return (int)enter_syscall(fd, 0, 0, 0, 0, SYS_CLOSE);
}

int mkfile(const char *name)
{
    return (int)enter_syscall((uint64_t)name, 0, 0, 0, 0, SYS_CREATFILE);
}

ssize_t read(int fd, void *buf, size_t count)
{
    return (ssize_t)enter_syscall(fd, (uint64_t)buf, count, 0, 0, SYS_READ);
}

ssize_t write(int fd, void *buf, size_t count)
{
    return (ssize_t)enter_syscall(fd, (uint64_t)buf, count, 0, 0, SYS_WRITE);
}

off_t lseek(int fd, off_t offset, int whence)
{
    return (off_t)enter_syscall(fd, offset, whence, 0, 0, SYS_LSEEK);
}

int waitpid(int pid, int *status)
{
    return enter_syscall(pid, (uint64_t)status, 0, 0, 0, SYS_WAITPID);
}

int getcwd(char *cwd)
{
    return enter_syscall((uint64_t)cwd, 0, 0, 0, 0, SYS_GETCWD);
}

int chdir(char *cwd)
{
    return enter_syscall((uint64_t)cwd, 0, 0, 0, 0, SYS_CHDIR);
}

int fork()
{
    return enter_syscall(0, 0, 0, 0, 0, SYS_FORK);
}

int execve(const char *name, char **argv, char **envp)
{
    return enter_syscall((uint64_t)name, (uint64_t)argv, (uint64_t)envp, 0, 0, SYS_EXECVE);
}

int getdents(int fd, dirent_t *dents, int max)
{
    return enter_syscall(fd, (uint64_t)dents, max, 0, 0, SYS_GETDENTS);
}

int getpid()
{
    return enter_syscall(0, 0, 0, 0, 0, SYS_GETPID);
}

int getppid()
{
    return enter_syscall(0, 0, 0, 0, 0, SYS_GETPPID);
}

void usleep(int ms)
{
    enter_syscall(ms, 0, 0, 0, 0, SYS_USLEEP);
}

uint64_t nanotime()
{
    return enter_syscall(0, 0, 0, 0, 0, SYS_NANOTIME);
}

int kill(int pid)
{
    return (int)enter_syscall(pid, 0, 0, 0, 0, SYS_KILL);
}

bool have_proc(int pid)
{
    return (bool)enter_syscall(pid, 0, 0, 0, 0, SYS_HAVE_PROC);
}

static unsigned long next = 1;

void srand(unsigned long seed)
{
    next = seed;
}

int rand(void)
{
    next = next * 1103515245 + 12345;
    return ((unsigned)(next / 65536) % 32768);
}
