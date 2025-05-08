#include <stdlib.h>
#include <libsyscall.h>

void *alloc_shared_memory(size_t size)
{
    return (void *)enter_syscall(size, 0, 0, 0, 0, SYS_ALLOC_SHARED_MEMORY);
}

void free_shared_memory(void *ptr)
{
    enter_syscall((uint64_t)ptr, 0, 0, 0, 0, SYS_FREE_SHARED_MEMORY);
}
