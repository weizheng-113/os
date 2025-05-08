#include <syscall/syscall.h>
#include <acpi/acpi.h>
#include <kprint.h>
#include <task/task.h>
#include <irq/irq.h>
#include <irq/gate.h>
#include <task/signal.h>
#include <task/msg.h>
#include <acpi/acpi.h>
#include <mm/hhdm.h>
#include <mm/frame.h>
#include <drivers/ps2_kbd.h>
#include <drivers/ps2_mouse.h>
#include <drivers/framebuffer.h>
#include <lib/flanterm/backends/fb.h>

task_window_t *current_top_window = NULL;

spinlock_t shared_memory_alloc_lock;
heap_t shared_memory_heap;

uint64_t sys_exit(uint64_t code)
{
    task_exit((int)code);

    return 0xFFFFFFFFFFFFFFFF;
}

uint64_t sys_brk(uint64_t addr)
{
    page_directory_t *current_page_dir = get_current_page_dir();

    uint64_t new_brk = (addr + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    if (new_brk == 0)
        return current_task->brk_start;
    if (new_brk < current_task->brk_end)
        return 0;

    uint64_t start = current_task->brk_end;
    uint64_t size = new_brk - current_task->brk_end;

    page_map_range_to(current_page_dir, start, 0, size + PAGE_SIZE, USER_PTE_FLAGS);

    new_brk = start + size;

    current_task->brk_end = new_brk;

    return new_brk;
}

uint64_t sys_sbrk(uint64_t size)
{
    uint64_t retval = current_task->brk_end;

    if (size == 0)
        return retval;

    page_map_range_to(get_current_page_dir(), retval, 0, size, USER_PTE_FLAGS);

    current_task->brk_end = retval + size;

    return retval;
}

uint64_t sys_open(const char *name, uint64_t mode, uint64_t flags)
{
    (void)mode;
    (void)flags;

    uint64_t i;
    for (i = 3; i < MAX_FD_NUM; i++)
    {
        if (current_task->fds[i] == NULL)
            break;
    }

    if (i == MAX_FD_NUM)
    {
        return (uint64_t)-EFAULT;
    }

    current_task->fds[i] = vfs_open(name);

    return i;
}

uint64_t sys_read(uint64_t fd, uint64_t buf, uint64_t len)
{
    if (fd >= MAX_FD_NUM)
        return (uint64_t)-EBADF;
    if (fd == 0)
    {
        if (current_task->window == current_top_window)
        {
            uint8_t scancode = get_keyboard_input();
            *(uint8_t *)buf = scancode;
        }
        else
        {
            get_keyboard_input();
        }

        return 1;
    }

    if (!current_task->fds[fd])
        return (uint64_t)-EBADF;
    return vfs_read(current_task->fds[fd], (void *)buf, current_task->fds[fd]->offset, len);
}

void write_window(task_window_t *window, const char *buf, uint64_t len)
{
    flanterm_write(window->ft_context, (char *)buf, (size_t)len);
}

uint64_t sys_write(uint64_t fd, uint64_t buf, uint64_t len)
{
    if (fd >= MAX_FD_NUM)
        return (uint64_t)-EBADF;
    if (fd == 1 || fd == 2)
    {
        if (current_task->window)
        {
            write_window(current_task->window, (const char *)buf, len);
        }

        return len;
    }
    if (!current_task->fds[fd])
        return (uint64_t)-EBADF;
    return vfs_write(current_task->fds[fd], (void *)buf, current_task->fds[fd]->offset, len);
}

uint64_t sys_creatfile(const char *name)
{
    int ret = vfs_mkfile(name);
    if (ret < 0)
    {
        return (uint64_t)ret;
    }

    return sys_open(name, 0, 0);
}

uint64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence)
{
    if (fd >= MAX_FD_NUM)
        return (uint64_t)-EBADF;
    switch (whence)
    {
    case SEEK_SET:
        current_task->fds[fd]->offset = offset;
        break;
    case SEEK_CUR:
        current_task->fds[fd]->offset += offset;
        break;
    case SEEK_END:
        current_task->fds[fd]->offset += current_task->fds[fd]->size - offset;
        break;
    }

    return current_task->fds[fd]->offset;
}

uint64_t sys_ioctl(uint64_t fd, uint64_t cmd, uint64_t arg)
{
    if (fd >= MAX_FD_NUM)
        return (uint64_t)-EBADF;
}

uint64_t sys_close(uint64_t fd)
{
    if (fd >= MAX_FD_NUM)
        return (uint64_t)-EBADF;
    if (current_task->fds[fd] == NULL)
        return (uint64_t)-EBADF;
    vfs_close(current_task->fds[fd]);
    current_task->fds[fd] = NULL;
}

uint64_t sys_getdents(uint64_t fd, uint64_t buf, uint64_t size)
{
    if (fd >= MAX_FD_NUM)
        return (uint64_t)-EBADF;
    if (!current_task->fds[fd])
        return (uint64_t)-EBADF;
    if (current_task->fds[fd]->type != file_dir)
        return (uint64_t)-ENOTDIR;

    dirent_t *dents = (dirent_t *)buf;
    vfs_node_t node = current_task->fds[fd];
    uint64_t len = 0;
    list_foreach(node->child, i)
    {
        vfs_node_t node = (vfs_node_t)i->data;
        strncpy(dents[len].name, node->name, 255);
        dents[len].type = node->type;
        len++;
    }

    return len;
}

uint64_t sys_chdir(const char *dirname)
{
    vfs_node_t new_cwd = vfs_open(dirname);
    if (!new_cwd)
        return (uint64_t)-ENOENT;
    if (new_cwd->type != file_dir)
        return (uint64_t)-ENOTDIR;

    current_task->cwd = new_cwd;

    return 0;
}

uint64_t sys_getcwd(char *cwd)
{
    char *str = vfs_get_fullpath(current_task->cwd);
    strcpy(cwd, str);
    free(str);
    return 0;
}

extern uint64_t sys_execve(const char *, char **, char **);

void *real_memcpy(void *dst, void *src, long len)
{
    return memcpy(dst, src, len);
}

uint64_t switch_to_kernel_stack()
{
    return current_task->syscall_stack;
}

uint64_t sys_set_window(uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    current_task->window->window_buffer = arg1;
    current_task->window->width = arg2;
    current_task->window->height = arg3;

    struct limine_framebuffer *framebuffer = current_framebuffer;

    if (current_task->window->ft_context)
    {
        flanterm_deinit(current_task->window->ft_context, free_frames_bytes);
    }

    current_task->window->ft_context = flanterm_fb_init(
        alloc_frames_bytes,
        free_frames_bytes,
        (uint32_t *)current_task->window->window_buffer, current_task->window->width, current_task->window->height, current_task->window->width * sizeof(uint32_t),
        framebuffer->red_mask_size, framebuffer->red_mask_shift,
        framebuffer->green_mask_size, framebuffer->green_mask_shift,
        framebuffer->blue_mask_size, framebuffer->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0);

    return current_task->window->window_buffer;
}

bool sys_have_proc(uint64_t pid)
{
    if (pid >= MAX_TASK_NUM)
        return (uint64_t)false;

    return ((tasks[pid] != NULL) && tasks[pid]->state != TASK_DIED);
}

void syscall_handler(struct pt_regs *regs, struct pt_regs *user_regs)
{
    regs->rip = regs->rcx;
    regs->rflags = regs->r11;
    regs->cs = SELECTOR_USER_CS;
    regs->ss = SELECTOR_USER_DS;
    regs->rsp = (uint64_t)(user_regs + 1);

    uint64_t idx = regs->rax;

    uint64_t arg1 = regs->rdi;
    uint64_t arg2 = regs->rsi;
    uint64_t arg3 = regs->rdx;
    uint64_t arg4 = regs->r10;
    uint64_t arg5 = regs->r8;

    switch (idx)
    {
    case SYS_EXIT:
        regs->rax = sys_exit(arg1);
        break;
    case SYS_GETPID:
        regs->rax = current_task->task_id;
        break;
    case SYS_GETPPID:
        regs->rax = current_task->parent_task_id;
        break;
    case SYS_FORK:
        regs->rax = sys_fork(regs);
        break;
    case SYS_WAITPID:
        regs->rax = sys_waitpid(arg1, (int *)arg2);
        break;

    case SYS_SIGNAL:
        regs->rax = sys_signal(arg1, arg2, arg3);
        break;
    case SYS_SIGACTION:
        regs->rax = sys_sigaction(arg1, (sigaction_t *)arg2, (sigaction_t *)arg3);
        break;
    case SYS_SETMASK:
        regs->rax = sys_ssetmask(arg1);
        break;
    case SYS_SENDSIGNAL:
        sys_sendsignal(arg1, arg2);
        regs->rax = 0;
        break;

    case SYS_EXECVE:
        regs->rax = sys_execve((const char *)arg1, (char **)arg2, (char **)arg3);
        break;
    case SYS_USLEEP:
        usleep(arg1);
        regs->rax = 0;
        break;

    case SYS_IOPL:
        sys_iopl(arg1);
        regs->rax = 0;
        break;

    case SYS_BRK:
        regs->rax = sys_brk(arg1);
        break;
    case SYS_SBRK:
        regs->rax = sys_sbrk(arg1);
        break;

    case SYS_OPEN:
        regs->rax = sys_open((const char *)arg1, arg2, arg3);
        break;
    case SYS_READ:
        regs->rax = sys_read(arg1, arg2, arg3);
        break;
    case SYS_WRITE:
        regs->rax = sys_write(arg1, arg2, arg3);
        break;
    case SYS_CLOSE:
        regs->rax = sys_close(arg1);
        break;
    case SYS_CREATFILE:
        regs->rax = sys_creatfile((const char *)arg1);
        break;
    case SYS_LSEEK:
        regs->rax = sys_lseek(arg1, arg2, arg3);
        break;
    case SYS_IOCTL:
        regs->rax = sys_ioctl(arg1, arg2, arg3);
        break;
    case SYS_CHDIR:
        regs->rax = sys_chdir((const char *)arg1);
        break;
    case SYS_GETCWD:
        regs->rax = sys_getcwd((char *)arg1);
        break;
    case SYS_GETDENTS:
        regs->rax = sys_getdents(arg1, arg2, arg3);
        break;
    case SYS_NANOTIME:
        regs->rax = nanoTime();
        break;

    case SYS_WRITE_FB:
        regs->rax = sys_write_fb(arg1, arg2, arg3, arg4, arg5);
        break;
    case SYS_GET_FB_INFO:
        regs->rax = sys_get_fbinfo(arg1, arg2, arg3, arg4);
        break;
    case SYS_GET_MOUSE_XY:
        sys_get_mouse_xy((int32_t *)arg1, (int32_t *)arg2);
        regs->rax = 0;
        break;
    case SYS_MOUSE_CLICK:
        regs->rax = sys_mouse_click();
        break;

    case SYS_MSG_CREATE:
        regs->rax = msgpool_create();
        break;
    case SYS_MSG_DESTROY:
        msgpool_destroy(arg1);
        regs->rax = 0;
        break;
    case SYS_MSG_FIND:
        regs->rax = msgpool_find_by_flag(arg1, (int *)arg2);
        break;
    case SYS_MSG_SET_FLAGS:
        regs->rax = msgpool_set_flag(arg1, arg2);
        break;
    case SYS_MSG_READ:
        regs->rax = msgpool_read(arg1, (void *)arg2, arg3);
        break;
    case SYS_MSG_WRITE:
        regs->rax = msgpool_write(arg1, (const void *)arg2, arg3);
        break;

    case SYS_ALLOC_SHARED_MEMORY:
        spin_lock(&shared_memory_alloc_lock);
        regs->rax = (uint64_t)heap_alloc(&shared_memory_heap, arg1);
        spin_unlock(&shared_memory_alloc_lock);
        break;
    case SYS_FREE_SHARED_MEMORY:
        spin_lock(&shared_memory_alloc_lock);
        heap_free(&shared_memory_heap, (void *)arg1);
        spin_unlock(&shared_memory_alloc_lock);
        regs->rax = 0;
        break;
    case SYS_SET_WINDOW:
        regs->rax = sys_set_window(arg1, arg2, arg3);
        break;
    case SYS_HAVE_A_WINDOW:
        regs->rax = ((current_task->window->width != 0) && (current_task->window->height != 0));
        break;

    case SYS_KILL:
        regs->rax = sys_kill(arg1, SIGKILL);
        break;

    case SYS_HAVE_PROC:
        regs->rax = sys_have_proc(arg1);
        break;

    case SYS_WINDOW_SET_MINIMAL:
        if (arg1 > MAX_TASK_NUM)
        {
            regs->rax = (uint64_t)-EINVAL;
            break;
        }
        tasks[arg1]->window->minimal = true;
        regs->rax = 0;
        break;
    case SYS_WINDOW_SET_RESTORED:
        if (arg1 > MAX_TASK_NUM)
        {
            regs->rax = (uint64_t)-EINVAL;
            break;
        }
        tasks[arg1]->window->minimal = false;
        regs->rax = 0;
        break;
    case SYS_WINDOW_SET_TOP:
        if (arg1 > MAX_TASK_NUM)
        {
            regs->rax = (uint64_t)-EINVAL;
            break;
        }
        if (arg1 == 0)
        {
            current_top_window = NULL;
        }
        else
        {
            current_top_window = tasks[arg1]->window;
        }
        break;

    default:
        regs->rax = ((uint64_t)-ENOSYS);
        break;
    }
}

// MSR寄存器地址定义
#define MSR_EFER 0xC0000080         // EFER MSR寄存器
#define MSR_STAR 0xC0000081         // STAR MSR寄存器
#define MSR_LSTAR 0xC0000082        // LSTAR MSR寄存器
#define MSR_SYSCALL_MASK 0xC0000084 // SYSCALL_MASK MSR寄存器

void syscall_init()
{
    spin_init(&shared_memory_alloc_lock);

    uint64_t efer;

    // 1. 启用 EFER.SCE (System Call Extensions)
    efer = rdmsr(MSR_EFER);
    efer |= 1; // 设置 SCE 位
    wrmsr(MSR_EFER, efer);

    uint16_t cs_sysret_cmp = SELECTOR_USER_CS - 16;
    uint16_t ss_sysret_cmp = SELECTOR_USER_DS - 8;
    uint16_t cs_syscall_cmp = SELECTOR_KERNEL_CS;
    uint16_t ss_syscall_cmp = SELECTOR_KERNEL_DS - 8;

    if (cs_sysret_cmp != ss_sysret_cmp)
    {
        kerror("Sysret offset is not valid (1)");
        return;
    }

    if (cs_syscall_cmp != ss_syscall_cmp)
    {
        kerror("Syscall offset is not valid (2)");
        return;
    }

    // 2. 设置 STAR MSR
    uint64_t star = 0;
    star = ((uint64_t)(SELECTOR_USER_DS - 8) << 48) | // SYSRET 的基础 CS
           ((uint64_t)SELECTOR_KERNEL_CS << 32);      // SYSCALL 的 CS
    wrmsr(MSR_STAR, star);

    // 3. 设置 LSTAR MSR (系统调用入口点)
    wrmsr(MSR_LSTAR, (uint64_t)syscall_exception);

    // 4. 设置 SYSCALL_MASK MSR (RFLAGS 掩码)
    wrmsr(MSR_SYSCALL_MASK, (1 << 9));
}
