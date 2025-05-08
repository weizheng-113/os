#include <libsyscall.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <aether/window.h>

extern void init_heap();

extern int main(int argc, char **argv, char **envp);

void exit_current()
{
    exit(-1);
}

void alibc_start(int argc, char **argv, char **envp)
{
    signal(SIGQUIT, (uint64_t)exit_current);

    init_heap();

    if (!have_a_window())
    {
        window_init();

        char *proc_name = NULL;

        proc_name = malloc(256);
        if (argc >= 1)
        {
            free(proc_name);
            proc_name = argv[0];
        }
        else
        {
            sprintf(proc_name, "window-%d", getpid());
        }
        uint64_t window_buffer = create_window_and_get_buffer((const char *)proc_name, 100, 100, WINDOW_WIDTH, WINDOW_HEIGHT);
        set_window(window_buffer, WINDOW_WIDTH, WINDOW_HEIGHT - (BUTTON_SIZE + 10));
    }

    exit(main(argc, argv, envp));
}
