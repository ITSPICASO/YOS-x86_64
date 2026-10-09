CC = clang --target=x86_64-elf
LD = ld.lld
AS = nasm

CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -mno-red-zone -Wall -Wextra -Ikernel/include -mcmodel=kernel -mno-sse -mno-mmx -mno-80387
ASFLAGS = -f elf64

BUILD_DIR = build

ASM_BOOT_OBJS = $(BUILD_DIR)/multiboot_header.o $(BUILD_DIR)/boot.o

ASM_ARCH_OBJS = $(BUILD_DIR)/gdt_flush.o $(BUILD_DIR)/tss_flush.o $(BUILD_DIR)/idt_flush.o \
                 $(BUILD_DIR)/isr_stubs.o $(BUILD_DIR)/keyboard_stub.o $(BUILD_DIR)/mouse_stub.o \
                 $(BUILD_DIR)/switch_context.o $(BUILD_DIR)/apic_timer_stub.o \
                 $(BUILD_DIR)/syscall_entry.o $(BUILD_DIR)/e1000_stub.o

C_ARCH_OBJS   = $(BUILD_DIR)/gdt.o $(BUILD_DIR)/tss.o $(BUILD_DIR)/idt.o $(BUILD_DIR)/isr.o \
                 $(BUILD_DIR)/page_fault.o $(BUILD_DIR)/double_fault.o $(BUILD_DIR)/serial.o \
                 $(BUILD_DIR)/string.o $(BUILD_DIR)/multiboot2.o $(BUILD_DIR)/pmm.o \
                 $(BUILD_DIR)/vmm.o $(BUILD_DIR)/heap.o $(BUILD_DIR)/slab.o $(BUILD_DIR)/acpi.o \
                 $(BUILD_DIR)/pic.o $(BUILD_DIR)/apic.o $(BUILD_DIR)/ioapic.o $(BUILD_DIR)/pci.o \
                 $(BUILD_DIR)/ps2.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/mouse.o $(BUILD_DIR)/ide.o \
                 $(BUILD_DIR)/mbr.o $(BUILD_DIR)/gpt.o $(BUILD_DIR)/ahci.o $(BUILD_DIR)/task.o \
                 $(BUILD_DIR)/apic_timer.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/fb.o \
                 $(BUILD_DIR)/font.o $(BUILD_DIR)/wm.o $(BUILD_DIR)/rtc.o $(BUILD_DIR)/e1000.o \
                 $(BUILD_DIR)/net.o $(BUILD_DIR)/socket.o $(BUILD_DIR)/elf.o

C_FS_OBJS     = $(BUILD_DIR)/vfs.o $(BUILD_DIR)/fat32.o $(BUILD_DIR)/tar.o $(BUILD_DIR)/devfs.o \
                 $(BUILD_DIR)/procfs.o $(BUILD_DIR)/bcache.o

OBJS = $(ASM_BOOT_OBJS) $(ASM_ARCH_OBJS) $(C_ARCH_OBJS) $(C_FS_OBJS) $(BUILD_DIR)/kernel.o

all: yos.iso

# Boot ASM
$(BUILD_DIR)/multiboot_header.o: boot/multiboot_header.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/boot.o: boot/boot.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Kernel Arch ASM
$(BUILD_DIR)/%.o: kernel/arch/x86_64/%.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Kernel Arch C
$(BUILD_DIR)/%.o: kernel/arch/x86_64/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Kernel FS C
$(BUILD_DIR)/%.o: kernel/fs/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Kernel Core
$(BUILD_DIR)/kernel.o: kernel/kernel.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link Kernel ELF
$(BUILD_DIR)/yos.bin: $(OBJS) linker.ld
	$(LD) -n -T linker.ld -o $(BUILD_DIR)/yos.bin $(OBJS)
	@echo "[+] YOS.BIN linké b naja7!"

# ISO Target
yos.iso: $(BUILD_DIR)/yos.bin isodir/boot/grub/grub.cfg isodir/boot/initrd.tar
	cp $(BUILD_DIR)/yos.bin isodir/boot/yos.bin
	grub-mkrescue -o yos.iso isodir
	@echo "[+] ISO YOS.ISO générée b naja7!"

clean:
	rm -rf $(BUILD_DIR)/* yos.iso isodir/boot/yos.bin
