#include <task/task.h>
#include <mm/hhdm.h>
#include <mm/frame.h>
#include <mm/page.h>
#include <mm/heap.h>
#include <irq/gate.h>
#include <irq/irq.h>
#include <kprint.h>
#include <syscall/syscall.h>

#include <drivers/bus/pci.h>
#include <drivers/block/ahci/ahci.h>
#include <drivers/block/nvme/nvme.h>
#include <block/block.h>
#include <fs/partition.h>
#include <fs/vfs/dev.h>
#include <fs/vfs/vfs.h>
#include <drivers/framebuffer.h>

bool can_schedule;

struct List block_list;

extern heap_t shared_memory_heap;

tss_t tss[MAX_CPU_NUM];

task_t *tasks[MAX_TASK_NUM];
task_t *idle_tasks[MAX_CPU_NUM];

task_t *currents[MAX_CPU_NUM];

spinlock_t fork_lock;

uint64_t shared_memory_physical_address = 0;

hardware_intr_controller apic_timer_controller = {
    .install = ioapic_add,
    .enable = ioapic_enable,
};

extern uint64_t cpu_count;
static volatile uint32_t cpu_idx = 0;
spinlock_t cpu_alloc_lock;

// 新增：调度模式标志，true=RR，false=FCFS
bool is_rr = false;

// 调度策略函数指针
typedef task_t* (*schedule_func_t)(task_state_t state, uint32_t cpu_id);
static schedule_func_t schedule_func = NULL;

// 轮转调度实现
task_t *schedule_rr(task_state_t state, uint32_t cpu_id)
{
    task_t *task = NULL;
    for (size_t i = cpu_count; i < MAX_TASK_NUM; i++)
    {
        task_t *ptr = tasks[i];
        if (ptr == NULL)
            continue;
        if (current_task == ptr)
            continue;
        if (ptr->state != state)
            continue;
        if (ptr->on_cpu != cpu_id)
            continue;
        if (task == NULL || ptr->jiffies < task->jiffies)
            task = ptr;
    }
    if (task == NULL && state == TASK_READY)
        task = idle_tasks[cpu_id];
    return task;
}

// FCFS调度实现（非抢占式先来先服务，选择最早创建的READY进程）
task_t *schedule_fcfs(task_state_t state, uint32_t cpu_id)
{
    task_t *task = NULL;
    for (size_t i = cpu_count; i < MAX_TASK_NUM; i++)
    {
        task_t *ptr = tasks[i];
        if (ptr == NULL)
            continue;
        if (current_task == ptr)
            continue;
        if (ptr->state != state)
            continue;
        if (ptr->on_cpu != cpu_id)
            continue;
        // FCFS: 选择task_id最小的（最早创建的）
        if (task == NULL || ptr->task_id < task->task_id)
            task = ptr;
    }
    if (task == NULL && state == TASK_READY)
        task = idle_tasks[cpu_id];
    return task;
}

// 调度策略初始化接口
void task_rr_init() { schedule_func = schedule_rr; is_rr = true; }
void task_fcfs_init() { schedule_func = schedule_fcfs; is_rr = false; }

uint32_t alloc_cpuid()
{
    spin_lock(&cpu_alloc_lock);
    uint32_t idx = cpu_idx;
#ifdef ALL_IN_ONE
    idx = 0;
#else
    cpu_idx = (cpu_idx + 1) % cpu_count;
#endif
    spin_unlock(&cpu_alloc_lock);
    return idx;
}

task_t *get_free_task()
{
    for (uint64_t i = 0; i < MAX_TASK_NUM; i++)
    {
        if (tasks[i] == NULL)
        {
            task_t *task = (task_t *)phys_to_virt(alloc_frames(1));
            memset(task, 0, PAGE_SIZE);
            task->task_id = i;
            tasks[i] = task;
            return task;
        }
    }

    return NULL;
}

// 修改task_search为调用策略
task_t *task_search(task_state_t state, uint32_t cpu_id)
{
    if (schedule_func)
        return schedule_func(state, cpu_id);
    // 默认使用轮转
    return schedule_rr(state, cpu_id);
}

// 修改：根据调度模式决定是否抢占
void timer_handler(uint8_t irq, uint64_t param, struct pt_regs *regs)
{
    (void)irq;
    (void)param;
    (void)regs;
    current_task->jiffies++;
    if (is_rr)
        current_task->need_schedule = true; // RR下抢占
    // FCFS下不抢占
}

task_t *task_create(const char *name, void (*entry)(), uint64_t uid)
{
    task_t *task = get_free_task();

    task->kernel_stack = phys_to_virt(alloc_frames(STACK_SIZE / PAGE_SIZE)) + STACK_SIZE;
    struct pt_regs *task_frame = (struct pt_regs *)task->kernel_stack - 1;

    memset(task_frame, 0, sizeof(struct pt_regs));
    task_frame->rflags = (1UL << 9);
    task_frame->rbp = task->kernel_stack;
    task_frame->rsp = task->kernel_stack;
    task_frame->rip = (uint64_t)entry;
    task_frame->cs = SELECTOR_KERNEL_CS;
    task_frame->ss = SELECTOR_KERNEL_DS;
    task_frame->ds = SELECTOR_KERNEL_DS;
    task_frame->es = SELECTOR_KERNEL_DS;
    task->context = task_frame;

    strcpy(task->name, (char *)name);
    task->self_ref = (uint64_t)task;
    list_init(&task->list);
    task->parent_task_id = task->task_id;
    task->waitpid = 0;
    task->syscall_stack = phys_to_virt(alloc_frames(STACK_SIZE / PAGE_SIZE)) + STACK_SIZE;
    task->on_cpu = alloc_cpuid();
    task->pgdir = get_kernel_page_dir();
    task->state = TASK_READY;
    task->fpu = (fpu_context *)phys_to_virt(alloc_frames(1));
    memset(task->fpu, 0, PAGE_SIZE);
    task->fpu->mxscr = 0x1f80;
    task->fpu->fcw = 0x037f;

    task->thread = (task_thread_t *)(task + 1);
    task->thread->fs = SELECTOR_KERNEL_DS;
    task->thread->gs = SELECTOR_KERNEL_DS;
    task->thread->fsbase = 0;
    task->thread->gsbase = 0;
    task->thread->elf_mapping_start = 0;
    task->thread->elf_mapping_end = 0;

    task->signal = 0;
    task->blocked = 0;

    task->need_schedule = false;
    task->userspace_io_allowed = false;

    task->cwd = NULL;

    task->brk_start = USER_BRK_START;
    task->brk_end = USER_BRK_START;

    task->uid = uid;

    task->window = malloc(sizeof(task_window_t));
    memset(task->window, 0, sizeof(task_window_t));
    task->window->reference_count++;
    task->window->parent_task = task;

    task->status = 0;
    task->state = TASK_READY;

    printk("[SCHED] task_PID=%d name=%s allocated by CPU_core%d\n", task->task_id, task->name, task->on_cpu);
    return task;
}

void sys_yield()
{
    __asm__ __volatile__("int %0\n\t" ::"i"(APIC_TIMER_INTERRUPT_VECTOR));
}

void idle_thread()
{
    while (1)
    {
        sti();
        hlt();
    }
}

extern void mount_root();

extern uint64_t sys_execve(const char *name, char **argv, char **envp);

extern void iso9660_init();
extern void fatfs_init();

extern void kbd_init();
extern void mouse_init();

void init_thread()
{
    kdebug("init thread is running");

    cli();

    pci_init();

    vfs_init();

    ahci_init();
    nvme_init();

    fatfs_init();
    iso9660_init();

    partition_init();

    dev_init();

    mount_root();

    fb_init();

    sti();

    kbd_init();
    mouse_init();

    cli();

    sys_execve("/usr/bin/init.exec", NULL, NULL);

    kerror("load aeui.exec failed");

    while (1)
    {
        sti();
        hlt();
    }
}

void tss_init()
{
    uint64_t sp = phys_to_virt(alloc_frames(STACK_SIZE / PAGE_SIZE)) + STACK_SIZE;
    uint64_t offset = 10 + current_cpu_id * 2;
    set_tss64((uint32_t *)&tss[current_cpu_id], sp, sp, sp, sp, sp, sp, sp, sp, sp, sp);
    set_tss_descriptor(offset, &tss[current_cpu_id]);
    load_TR(offset);
}

bool task_initialized = false;

void task_init()
{
    printk("[DEBUG] cpu_count = %lu\n", cpu_count);

    shared_memory_physical_address = alloc_frames(SHARED_MEMORY_SIZE / PAGE_SIZE);

    page_map_range_to(get_kernel_page_dir(), SHARED_MEMORY_SPACE, shared_memory_physical_address, SHARED_MEMORY_SIZE, USER_PTE_FLAGS);

    init_heap(&shared_memory_heap, SHARED_MEMORY_SPACE, SHARED_MEMORY_SIZE);

    memset(tasks, 0, sizeof(tasks));
    memset(idle_tasks, 0, sizeof(idle_tasks));

    spin_init(&cpu_alloc_lock);
    spin_init(&fork_lock);
    list_init(&block_list);

    for (uint64_t i = 0; i < cpu_count; i++)
    {
        idle_tasks[i] = task_create("idle", idle_thread, KERNEL_USER);
    }
    task_create("/usr/bin/init.exec", init_thread, NORMAL_USER);

    irq_register(APIC_TIMER_INTERRUPT_VECTOR, timer_handler, 0, &apic_timer_controller, "APIC TIMER");

    task_initialized = true;

    can_schedule = true;

    cli();

    write_kgsbase((uint64_t)idle_tasks[current_cpu_id]);

    // 选择调度算法
    // --- FCFS ---
    //task_fcfs_init();
    //printk("[SCHED] Algorithm: FCFS\n");

    // --- RR ---
    task_rr_init();
    printk("[SCHED] Algorithm: RR\n");

    task_switch_to(NULL, NULL, idle_tasks[current_cpu_id]);
}

extern void task_signal();

void task_switch_to(struct pt_regs *curr, task_t *prev, task_t *next)
{

    //输出进程调度信息
    if (prev && next && prev != next) {
        /*printk("[SCHED] Switch from PID=%d to PID=%d (prev jiffies=%lu, next jiffies=%lu)\n",
            prev->task_id, next->task_id, prev->jiffies, next->jiffies);*/
    }
    //

    if (curr != NULL && prev != NULL)
    {
        // memcpy(prev->context, curr, sizeof(struct pt_regs));
        prev->context = curr;
    }

    if (prev)
    {
        __asm__ __volatile__("movq %%fs, %0\n\t" : "=r"(prev->thread->fs));
        __asm__ __volatile__("movq %%gs, %0\n\t" : "=r"(prev->thread->gs));

        prev->thread->fsbase = read_fsbase();
        prev->thread->gsbase = read_gsbase();

        if (prev->state == TASK_RUNNING)
            prev->state = TASK_READY;

        __asm__ __volatile__("fxsave (%0)" ::"r"(prev->fpu));
    }

    if (!next)
        kerror("next should not be NULL");

    if (can_schedule)
    {
        __asm__ __volatile__("movq %0, %%cr3\n\t" ::"r"(next->pgdir->table));

        // Start to switch
        tss[current_cpu_id].rsp0 = next->kernel_stack;
        uint16_t value = next->userspace_io_allowed ? offsetof(tss_t, iomap) : 0xFFFF;
        tss[current_cpu_id].iomapbaseaddr = value;

        __asm__ __volatile__("movq %0, %%fs\n\t" ::"r"(next->thread->fs));
        __asm__ __volatile__("movq %0, %%gs\n\t" ::"r"(next->thread->gs));

        write_fsbase(next->thread->fsbase);
        write_gsbase(next->thread->gsbase);

        next->state = TASK_RUNNING;

        __asm__ __volatile__("fxrstor (%0)" ::"r"(next->fpu));

        currents[current_cpu_id] = next;
        write_kgsbase((uint64_t)next);

        task_signal();

        __asm__ __volatile__(
            "movq %0, %%rsp\n\t"
            "jmp ret_from_exception" ::"r"(next->context));
    }
}

void task_exit(int code)
{
    cli();

    task_t *task = current_task;

    free_frames(virt_to_phys((uint64_t)task->fpu), 1);

    free_frames(virt_to_phys(task->kernel_stack), STACK_SIZE / PAGE_SIZE);
    free_frames(virt_to_phys(task->syscall_stack), STACK_SIZE / PAGE_SIZE);

    if (task->window->reference_count <= 0)
    {
        if (task->window->ft_context)
        {
            flanterm_deinit(task->window->ft_context, free_frames_bytes);
        }

        free(task->window);
    }

    task->status = code;

    free_page_table_recursive(task->pgdir, 4);

    task_t *parent = get_task(task->parent_task_id);
    if (parent && parent->state == TASK_BLOCKING && (parent->waitpid == 0 || parent->waitpid == task->task_id))
    {
        task_unblock(parent, EOK);
    }

    // for (uint64_t i = 0; i < MAX_TASK_NUM; i++)
    // {
    //     task_t *task = tasks[i];
    //     if (!task)
    //         continue;
    //     if (task->parent_task_id == task->task_id)
    //         continue;
    //     if (task->parent_task_id == current_task->task_id)
    //         sys_kill(task->task_id, SIGKILL);
    // }

    task->state = TASK_DIED;

    task_switch_to(NULL, NULL, idle_tasks[current_cpu_id]);
}

task_t *get_task(uint64_t pid)
{
    for (uint64_t i = 0; i < MAX_TASK_NUM; i++)
    {
        if (tasks[i] == NULL)
            continue;
        if (tasks[i]->task_id == pid)
            return tasks[i];
    }

    return NULL;
}

int task_block(task_t *task, struct List *blist, task_state_t state, int timeout_ms)
{
    if (blist == NULL)
    {
        blist = &block_list;
    }

    list_add_to_behind(blist, &task->list);
    if (timeout_ms > 0)
    {
        // todo
    }

    task->state = state;

    if (current_task == task)
    {
        sti();

        sys_yield();

        __asm__ __volatile__("pause");
    }

    return task->status;
}

void task_unblock(task_t *task, int reason)
{
    list_del(&task->list);

    task->status = reason;
    task->state = TASK_READY;
}

uint64_t sys_fork(struct pt_regs *regs)
{
    can_schedule = false;

    spin_lock(&fork_lock);

    task_t *child = get_free_task();
    strcpy(child->name, current_task->name);

    child->self_ref = (uint64_t)child;
    list_init(&child->list);
    child->on_cpu = alloc_cpuid();

    child->uid = current_task->uid;

    child->parent_task_id = current_task->task_id;

    child->state = TASK_READY;
    child->fpu = (fpu_context *)phys_to_virt(alloc_frames(1));
    memcpy(child->fpu, current_task->fpu, PAGE_SIZE);

    child->pgdir = clone_directory(get_current_page_dir());

    child->kernel_stack = phys_to_virt(alloc_frames(STACK_SIZE / PAGE_SIZE)) + STACK_SIZE;
    child->syscall_stack = phys_to_virt(alloc_frames(STACK_SIZE / PAGE_SIZE)) + STACK_SIZE;

    child->thread = (task_thread_t *)(child + 1);
    memcpy(child->thread, current_task->thread, sizeof(task_thread_t));

    uint64_t stack = (uint64_t)phys_to_virt(alloc_frames(STACK_SIZE / PAGE_SIZE)) + STACK_SIZE;
    stack -= sizeof(struct pt_regs);
    struct pt_regs *task_frame = (struct pt_regs *)stack;
    memcpy(task_frame, regs, sizeof(struct pt_regs));
    task_frame->rax = 0;

    child->context = task_frame;

    child->status = 0;

    memcpy(child->actions, current_task->actions, sizeof(child->actions));
    child->signal = current_task->signal;
    child->blocked = current_task->blocked;

    child->need_schedule = false;

    child->brk_start = USER_BRK_START;
    child->brk_end = USER_BRK_START;

    child->window = malloc(sizeof(task_window_t));
    memset(child->window, 0, sizeof(task_window_t));
    child->window->reference_count++;
    child->window->parent_task = child;

    child->cwd = current_task->cwd;

    spin_unlock(&fork_lock);

    can_schedule = true;

    printk("[SCHED] fork: task_PID=%d allocated by CPU_core%d\n", child->task_id, child->on_cpu);

    return child->task_id;
}

uint64_t sys_waitpid(uint64_t pid, int *status)
{
    task_t *child = NULL;

    while (1)
    {
        bool has_child = false;

        for (uint64_t i = cpu_count; i < MAX_TASK_NUM; i++)
        {
            task_t *ptr = tasks[i];
            if (ptr == NULL)
                continue;

            if (ptr->parent_task_id != current_task->task_id)
                continue;

            if (pid != ptr->task_id && pid != 0)
                continue;

            if (ptr->state == TASK_DIED)
            {
                child = ptr;
                tasks[i] = NULL;
                goto rollback;
            }

            has_child = true;

            break;
        }
        if (has_child)
        {
            current_task->waitpid = pid;
            task_block(current_task, NULL, TASK_BLOCKING, 0);
            continue;
        }

        break;
    }

    return -1;

rollback:
    *status = (int)child->status;
    uint32_t ret = child->task_id;

    current_task->waitpid = 0;

    free_frames((uint64_t)child, 1);

    return ret;
}

void sys_iopl(uint64_t level)
{
    if (level > 3)
    {
        return;
    }

    bool allow = (level == 3);

    current_task->userspace_io_allowed = allow;

    uint16_t value = allow ? offsetof(tss_t, iomap) : 0xFFFF;

    tss[current_cpu_id].iomapbaseaddr = value;
}