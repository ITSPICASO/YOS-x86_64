# 🌌 YOS-x86_64 — 64-bit Modular Operating System

![Architecture](https://img.shields.io/badge/Architecture-x86__64%20Long%20Mode-blue?style=for-the-badge)
![Status](https://img.shields.io/badge/Kernel-Higher--Half%20Ring%200%2F3-green?style=for-the-badge)
![GUI](https://img.shields.io/badge/UI-Compositing%20Window%20Manager-orange?style=for-the-badge)
![Build](https://img.shields.io/badge/Compiler-Clang%20%2F%20NASM-purple?style=for-the-badge)

**YOS-x86_64** is a modern, modular 64-bit operating system kernel developed from scratch for the `x86_64` architecture, featuring hardware multitasking, a graphical compositing window manager, and an interactive shell terminal.

---

## ✨ Key Features

- **Core Architecture**:
  - Pure 64-bit Long Mode bootloader sequence.
  - Higher-Half Kernel mapping using 4-level PML4 paging.
  - Physical Memory Manager (PMM bitmap) & Virtual Memory Manager (VMM).
  - Dynamic Kernel Heap allocator (Slab & Buddy allocators).
  - Interrupt handling with IDT, PIC/IOAPIC, and custom ISR/IRQ dispatchers.
  - Ring 3 User Mode transitions with `sysenter` / `syscall` support.

- **Graphical Window Manager (WM)**:
  - Linear Framebuffer (UEFI GOP / VESA fallback) supporting 32-bit TrueColor (1024x768).
  - Software double-buffering (`back_buffer` composition) to eliminate screen tearing.
  - Fully interactive **Start Menu** with hover highlights and application launching.
  - Interactive **Terminal Window** featuring a low-latency localized redraw engine.
  - Window drag-and-drop, focus stacking, and taskbar integration.

- **Storage & File Systems**:
  - Virtual File System (VFS) abstraction layer.
  - Virtual filesystems: `devfs`, `procfs`, and TAR Ramdisk support.
  - SATA AHCI driver & IDE storage controller integration.
  - Block cache (`bcache`) for optimized disk I/O.

- **Hardware Drivers**:
  - PS/2 Keyboard & Mouse driver with real-time cursor tracking.
  - Real-Time Clock (RTC) and APIC/PIT timers.
  - Intel e1000 Gigabit Network Card driver stub.
  - Serial port (COM1) debugging console.

---

## 🖥️ Mini Shell Commands

The integrated Terminal shell includes interactive built-in commands:

| Command | Description |
|---|---|
| `help` | Lists available commands |
| `uname` / `uname -a` | Displays kernel version and target machine |
| `ls` / `ls -l /` | Lists files in root directory (`devfs`, `procfs`, `hello.elf`) |
| `clear` | Clears the terminal screen |
| `about` | Displays YOS system information and release build |
| `reboot` | Triggers a hardware CPU reset via 8042 controller |

---

## 🛠️ Build & Run

### Prerequisites
Make sure you have the following tools installed on your Linux / WSL environment:
- `clang` (configured for freestanding ELF64 targets)
- `nasm`
- `make`
- `xorriso` & `grub-mkrescue`
- `qemu-system-x86_64`

### Compilation
To compile the kernel and generate the bootable ISO:
```bash
make clean
make yos.iso
