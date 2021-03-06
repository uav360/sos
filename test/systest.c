#include <libc.h>
#include "shell.h"
#include "uart.h"
#include "log.h"

s32 test_cpu_all(u32 argc, char **argv);
s32 test_timer_all(u32 argc, char **argv);
s32 test_gpio_all(u32 argc, char **argv);
s32 test_libc_all(u32 argc, char **argv);
s32 test_log_all(u32 argc, char **argv);
s32 test_os_all(u32 argc, char **argv);
s32 test_wdt_all(u32 argc, char **argv);

s32 sub_cmd_help();

struct shell_cmd_info sub_ci[] =  {
    { .name = "cpu",   .func = test_cpu_all,   .desc = ""},
    { .name = "timer", .func = test_timer_all, .desc = ""},
    { .name = "gpio",  .func = test_gpio_all,  .desc = ""},
    { .name = "libc",  .func = test_libc_all,  .desc = ""},
    { .name = "log",   .func = test_log_all,   .desc = ""},
    { .name = "os",    .func = test_os_all,    .desc = ""},
    { .name = "wdt",   .func = test_wdt_all,   .desc = ""},
    { .name = "help",  .func = sub_cmd_help,   .desc = ""},
};

PUBLIC void dump_mem(u32 addr, u32 word_nr)
{
    u32 i;
    u32 *p = (u32 *)addr;
    for(i = 0; i < word_nr; i++) {
        if (i % 4 == 0) {
            uart_printf("\n[0x%X]: ", &p[i]);
        }
        uart_printf("0x%X ", p[i]);
    }

    uart_printf("\n");
}

PRIVATE s32 sub_cmd_help()
{
    u32 i;
    for(i = 0; i < (sizeof(sub_ci)/sizeof(sub_ci[0])); i++) {
        PRINT_EMG("%s:\t\t\t%s\n", sub_ci[i].name, sub_ci[i].desc);
    }
    return 0;
}

PRIVATE static s32 get_cmd_index(char *cmd)
{
    u32 i;
    for(i = 0; i < (sizeof(sub_ci)/sizeof(sub_ci[0])); i++) {
        if (strcmp(sub_ci[i].name, cmd) == 0) {
            return i;
        }
    }
    return -1;
}

PUBLIC s32 systest(u32 argc, char **argv)
{
    u32 i;
    s32 ret;

    if ((i = get_cmd_index(argv[1])) == -1) {
        PRINT_EMG("illegal sub-cmd [%s]\n", argv[1]);
        sub_cmd_help();
        return -1;
    }

    ret = sub_ci[i].func(argc, argv);
    return ret;
}
