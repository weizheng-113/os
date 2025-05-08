#pragma once

#include <acpi/acpi.h>
#include <irq/irq.h>

#define KB_BUF_SIZE 64

#define PORT_KB_DATA 0x60
#define PORT_KB_STATUS 0x64
#define PORT_KB_CMD 0x64

#define KBCMD_WRITE_CMD 0x60
#define KBCMD_READ_CMD 0x20

#define KB_INIT_MODE 0x47

#define KB_SEND2MOUSE 0xd4
#define MOUSE_EN 0xf4

#define KB_EN_MOUSE_INTFACE 0xa8

#define KBSTATUS_IBF 0x02
#define KBSTATUS_OBF 0x01

#define wait_KB_write() \
    while (io_in8(PORT_KB_STATUS) & KBSTATUS_IBF)

#define wait_KB_read() \
    while (io_in8(PORT_KB_STATUS) & KBSTATUS_OBF)

struct keyboard_buf
{
    uint8_t *p_head;
    uint8_t *p_tail;
    int32_t count;
    bool ctrl;
    bool shift;
    bool alt;
    bool caps;
    uint8_t buf[KB_BUF_SIZE];
};

extern struct keyboard_buf kb_fifo;

void parse_scan_code(uint8_t x);
uint8_t get_keyboard_input();

void kbd_init();
