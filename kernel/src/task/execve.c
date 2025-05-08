#include <kprint.h>
#include <task/execve.h>
#include <mm/hhdm.h>
#include <mm/page.h>
#include <mm/heap.h>
#include <task/task.h>
#include <irq/gate.h>
#include <fs/vfs/vfs.h>
#include <syscall/syscall.h>

spinlock_t execve_lock = {.lock = 1};

void task_to_user_mode(uint64_t addr, uint64_t load_start, uint64_t load_end, char **argv, char **envp)
{
    struct pt_regs *iframe = current_task->context;

    iframe->cs = SELECTOR_USER_CS;
    iframe->ss = SELECTOR_USER_DS;
    iframe->ds = SELECTOR_USER_DS;
    iframe->es = SELECTOR_USER_DS;
    iframe->rflags = (0 << 12) | (0b10) | (1 << 9);
    iframe->rip = addr;

    iframe->rbp = USER_STACK_TOP - PAGE_SIZE;
    iframe->rsp = USER_STACK_TOP - PAGE_SIZE;

    page_map_range_to(get_current_page_dir(), USER_STACK_TOP - USER_STACK_SIZE, 0, USER_STACK_SIZE, USER_PTE_FLAGS);
    memset((void *)(USER_STACK_TOP - USER_STACK_SIZE), 0, USER_STACK_SIZE);

    iframe->rdi = 0;
    iframe->rsi = 0;
    iframe->rdx = 0;
    // if (argv != NULL)
    // {
    //     int argc = 0;

    //     char **dst_argv = (char **)(iframe->rsp - sizeof(char **) * 8);
    //     uint64_t str_addr = (uint64_t)dst_argv;

    //     for (argc = 0; argc < 8 && argv[argc] != NULL; ++argc)
    //     {
    //         if (argv[argc][0] == '\0')
    //             break;

    //         int argv_len = strlen(argv[argc]) + 1;
    //         strncpy((void *)str_addr, argv[argc], argv_len);
    //         str_addr -= argv_len;
    //         dst_argv[argc] = (char *)str_addr;
    //         ((char *)str_addr)[argv_len] = '\0';
    //     }

    //     iframe->rsp = str_addr - 8;
    //     iframe->rdi = argc;
    //     iframe->rsi = (uint64_t)dst_argv;
    // }

    current_task->thread->gs = SELECTOR_USER_DS;
    current_task->thread->fs = SELECTOR_USER_DS;

    __asm__ __volatile__("movq %0, %%fs\n\t"
                         "movq %0, %%gs\n\t" ::"r"(current_task->thread->fs),
                         "r"(current_task->thread->gs));

    current_task->thread->elf_mapping_start = load_start;
    current_task->thread->elf_mapping_end = load_end;

    __asm__ __volatile__("movq %0, %%rsp\n\t"
                         "jmp ret_from_exception" ::"r"(iframe));
}

extern uint64_t shared_memory_physical_address;

uint64_t sys_execve(const char *name, char **argv, char **envp)
{
    cli();

    spin_lock(&execve_lock);

    vfs_node_t node = vfs_open(name);
    if (!node){
        printk("[SCHED] execve: vfs_open failed for %s\n", name);
        return (uint64_t)-ENOENT;
    }

    uint8_t *buffer = (uint8_t *)malloc(sizeof(Elf64_Ehdr));
    if (!buffer)
    {
        printk("[SCHED] execve: malloc Elf64_Ehdr failed\n");
        vfs_close(node);
        spin_unlock(&execve_lock);
        return (uint64_t)-ENOMEM;
    }

    vfs_read(node, buffer, 0, sizeof(Elf64_Ehdr));

    char *fullpath = vfs_get_fullpath(node);

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)buffer;

    uint64_t e_entry = ehdr->e_entry;

    if (e_entry == 0)
    {
        // kerror("bad e_entry\n");
        printk("[SCHED] execve: bad e_entry for %s\n", fullpath);
        free(buffer);
        free(fullpath);
        vfs_close(node);
        spin_unlock(&execve_lock);
        return -EINVAL;
    }

    // 验证ELF魔数
    if (memcmp((void *)ehdr->e_ident, "\x7F"
                                      "ELF",
               4) != 0)
    {
        // kerror("Invalid ELF magic\n");
        printk("[SCHED] execve: Invalid ELF magic for %s\n", fullpath);
        free(buffer);
        free(fullpath);
        vfs_close(node);
        spin_unlock(&execve_lock);
        return -EINVAL;
    }

    // 检查架构和类型
    if (ehdr->e_ident[4] != 2 ||   // 64-bit
        ehdr->e_machine != 0x3E || // x86_64
        (ehdr->e_type != 2))
    {
        printk("[SCHED] execve: Unsupported ELF format for %s\n", fullpath);
        // kerror("Unsupported ELF format\n");
        free(buffer);
        free(fullpath);
        vfs_close(node);
        spin_unlock(&execve_lock);
        return -EINVAL;
    }

    if (current_task->pgdir == get_kernel_page_dir())
    {
        current_task->pgdir = clone_directory(get_kernel_page_dir());
    }

    __asm__ __volatile__("movq %0, %%cr3\n\t" ::"r"(current_task->pgdir->table) : "memory");

    uint64_t load_start = UINT64_MAX;
    uint64_t load_end = 0;

    // 处理程序头
    Elf64_Phdr *phdr = (Elf64_Phdr *)malloc(ehdr->e_phnum * sizeof(Elf64_Phdr));
    if (!buffer)
    {
        free(buffer);
        free(fullpath);
        vfs_close(node);
        spin_unlock(&execve_lock);
        return (uint64_t)-ENOMEM;
    }
    vfs_read(node, phdr, ehdr->e_phoff, ehdr->e_phnum * sizeof(Elf64_Phdr));
    for (int i = 0; i < ehdr->e_phnum; ++i)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        uint64_t seg_addr = phdr[i].p_vaddr;
        uint64_t seg_size = phdr[i].p_memsz;
        uint64_t file_size = phdr[i].p_filesz;
        uint64_t page_size = PAGE_SIZE;
        uint64_t page_mask = page_size - 1;

        // 计算对齐后的地址和大小
        uint64_t aligned_addr = seg_addr & ~page_mask;
        uint64_t size_diff = seg_addr - aligned_addr;
        uint64_t alloc_size = (seg_size + size_diff + page_mask) & ~page_mask;

        if (aligned_addr < load_start)
            load_start = aligned_addr;
        else if (aligned_addr + alloc_size > load_end)
            load_end = aligned_addr + alloc_size;

        page_directory_t *pgdir = get_current_page_dir();
        page_map_range_to(pgdir, aligned_addr, 0, alloc_size, USER_EXEC_PTE_FLAGS);

        vfs_read(node, (void *)seg_addr, phdr[i].p_offset, file_size);

        // 清零剩余内存
        if (seg_size > file_size)
        {
            memset((char *)aligned_addr + size_diff + file_size,
                   0, seg_size - file_size);
        }
    }

    free(phdr);
    free(buffer);

    vfs_close(node);

    strncpy(current_task->name, fullpath, TASK_NAME_LEN);

    free(fullpath);

    spin_unlock(&execve_lock);

    char buf[256];
    (void)buf;
    task_to_user_mode(e_entry, load_start, load_end, argv, envp);

    return 0;
}
