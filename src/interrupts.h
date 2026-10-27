#include <stdint.h>
#pragma once

#define IDT_ENTRIES 256
#define KERNEL_CODE_SELECTOR 0x18

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];

int register_interrupt_kernel(uint8_t num, void *ptr) {
    uint64_t address = (uint64_t)ptr;

    idt[num].offset_low  = (uint16_t)(address & 0xFFFF);
    idt[num].selector    = KERNEL_CODE_SELECTOR;
    idt[num].ist         = 0;
    idt[num].type_attr   = 0x8E; /* Present, ring 0, 64-bit interrupt gate */
    idt[num].offset_mid  = (uint16_t)((address >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)(address >> 32);
    idt[num].reserved    = 0;

    return 0;
}

void lidt(void) {
    struct idtr idtr = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .base  = (uint64_t)idt
    };

    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
