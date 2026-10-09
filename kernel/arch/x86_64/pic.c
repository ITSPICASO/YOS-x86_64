#include "pic.h"
#include "io.h"
#include "serial.h"

void pic_disable(void) {
    /* Initialisation sequence en cascade (ICW1) */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2 : Remappage des vecteurs (pour eviter les conflits avec les exceptions 0-31) */
    outb(PIC1_DATA, 0x20); /* Master IRQ 0..7 -> Vecteurs 32..39 */
    io_wait();
    outb(PIC2_DATA, 0x28); /* Slave IRQ 8..15 -> Vecteurs 40..47 */
    io_wait();

    /* ICW3 : Configuration maitre/esclave */
    outb(PIC1_DATA, 4);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();

    /* ICW4 : Mode 8086 */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Masquage total de toutes les IRQ (0xFF) */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    serial_print("[+] PIC 8259 : Desactive et masque completement.\\n");
}
