bits 16
org 0x7c00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov [boot_drive], dl
    mov [0x8000], dl

    mov ax, 0x1000
    mov es, ax
    mov ah, 02h
    mov al, 0x40
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    xor bx, bx
    int 13h
    jc error

    xor ax, ax
    mov es, ax

    mov ax, 12h
    int 10h

    lgdt [gdtr]
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 08h:protected_mode

error:
    mov ax, 0003h
    int 10h
    cli
.halt:
    hlt
    jmp .halt

boot_drive:
    db 0

gdt:
    dq 0
    dq 0x00CF9A000000FFFF ;32 bit kernel
    dq 0x00CF92000000FFFF

    dq 0x00AF9A000000FFFF ;64 bit kernel
    dq 0x00CF92000000FFFF

    dq 0x00CFF2000000FFFF ; ring-3 data
    dq 0x00AFFA000000FFFF ; ring-3 code


gdt_end:

gdtr:
    dw gdt_end - gdt - 1
    dd gdt



bits 32

protected_mode:
    mov ax, 10h
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000

    ; Clear page tables.
    mov edi, 0x20000
    xor eax, eax
    mov ecx, 0x1800
    rep stosd

    ; PML4[0] -> PDPT.
    mov dword [0x20000], 0x21003

    ; PDPT[0] -> identity map page directory.
    mov dword [0x21000], 0x22003

    ; Identity-map first 256 MiB using 2 MiB pages.
    mov edi, 0x22000
    mov eax, 0x83
    mov ecx, 128

.map_pages:
    mov [edi], eax
    mov dword [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    loop .map_pages

    ; Enable PAE.
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Load PML4.
    mov eax, 0x20000
    mov cr3, eax

    ; Enable long mode.
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov dword [0x21008], 0x23003
    mov dword [0x21010], 0x24003
    mov dword [0x21018], 0x25003

    ; Enable paging.
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    jmp 18h:long_mode

bits 64

long_mode:
    mov ax, 20h
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    movzx edi, byte [0x8000]

    mov rsp, 0x90000
    jmp 0x10000

times 446-($-$$) db 0

db 0x80
db 0x01, 0x01, 0x00
db 0xDA
db 0xFE, 0xFF, 0xFF
dd 0x00000001
dd 0x0000003F

times 48 db 0

dw 0xAA55
