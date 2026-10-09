#include "bcache.h"
#include "ahci.h"
#include "serial.h"

static bcache_entry_t cache_pool[BCACHE_CAPACITY];
static bcache_entry_t *head = NULL; /* Le plus récemment utilisé (MRU) */
static bcache_entry_t *tail = NULL; /* Le moins récemment utilisé (LRU) */

static uint64_t hits = 0;
static uint64_t misses = 0;

static void list_remove(bcache_entry_t *entry) {
    if (entry->prev) entry->prev->next = entry->next;
    else head = entry->next;

    if (entry->next) entry->next->prev = entry->prev;
    else tail = entry->prev;

    entry->prev = NULL;
    entry->next = NULL;
}

static void list_push_front(bcache_entry_t *entry) {
    entry->prev = NULL;
    entry->next = head;

    if (head) head->prev = entry;
    head = entry;

    if (!tail) tail = entry;
}

void bcache_init(void) {
    serial_print("[*] Etape 60 : Initialisation du Buffer Cache (LRU)...\n");

    for (int i = 0; i < BCACHE_CAPACITY; i++) {
        cache_pool[i].lba = 0;
        cache_pool[i].valid = false;
        cache_pool[i].dirty = false;
        cache_pool[i].prev = NULL;
        cache_pool[i].next = NULL;

        list_push_front(&cache_pool[i]);
    }

    hits = 0;
    misses = 0;
    serial_print("[+] Buffer Cache : 64 blocs alloues en RAM avec succes.\n");
}

bool bcache_read(uint32_t lba, uint8_t *buffer) {
    /* 1. Chercher dans le cache */
    bcache_entry_t *cur = head;
    while (cur) {
        if (cur->valid && cur->lba == lba) {
            /* CACHE HIT */
            hits++;
            for (int i = 0; i < BCACHE_BLOCK_SIZE; i++) {
                buffer[i] = cur->data[i];
            }
            /* Remonter au début (MRU) */
            list_remove(cur);
            list_push_front(cur);
            return true;
        }
        cur = cur->next;
    }

    /* 2. CACHE MISS : Évincer le plus ancien (tail) */
    misses++;
    bcache_entry_t *victim = tail;

    /* Flush si dirty */
    if (victim->valid && victim->dirty) {
        ahci_write_sectors(0, victim->lba, 1, victim->data);
        victim->dirty = false;
    }

    /* Lire depuis le disque AHCI */
    if (!ahci_read_sectors(0, lba, 1, victim->data)) {
        return false;
    }

    victim->lba = lba;
    victim->valid = true;
    victim->dirty = false;

    for (int i = 0; i < BCACHE_BLOCK_SIZE; i++) {
        buffer[i] = victim->data[i];
    }

    /* Mettre à jour la priorité LRU */
    list_remove(victim);
    list_push_front(victim);

    return true;
}

bool bcache_write(uint32_t lba, const uint8_t *buffer) {
    bcache_entry_t *cur = head;
    while (cur) {
        if (cur->valid && cur->lba == lba) {
            for (int i = 0; i < BCACHE_BLOCK_SIZE; i++) {
                cur->data[i] = buffer[i];
            }
            cur->dirty = true;
            list_remove(cur);
            list_push_front(cur);
            return true;
        }
        cur = cur->next;
    }

    /* Pas dans le cache : prendre victim */
    bcache_entry_t *victim = tail;
    if (victim->valid && victim->dirty) {
        ahci_write_sectors(0, victim->lba, 1, victim->data);
    }

    victim->lba = lba;
    victim->valid = true;
    victim->dirty = true;
    for (int i = 0; i < BCACHE_BLOCK_SIZE; i++) {
        victim->data[i] = buffer[i];
    }

    list_remove(victim);
    list_push_front(victim);
    return true;
}

void bcache_flush(void) {
    bcache_entry_t *cur = head;
    int flushed = 0; (void)flushed;
    while (cur) {
        if (cur->valid && cur->dirty) {
            ahci_write_sectors(0, cur->lba, 1, cur->data);
            cur->dirty = false;
            flushed++;
        }
        cur = cur->next;
    }
}
