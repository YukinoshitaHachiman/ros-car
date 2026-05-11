#include "flash_ops.h"
#include "flash_partition.h"

// ─── Low-level flash ───────────────────────────────────────────────

uint8_t flash_erase_page(uint32_t addr)
{
    FLASH_Status s;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    s = FLASH_ErasePage(addr);
    FLASH_Lock();
    return (s == FLASH_COMPLETE);
}

uint8_t flash_write_buf(uint32_t addr, uint8_t *data, uint16_t len)
{
    FLASH_Status s;
    uint16_t *p = (uint16_t *)data;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    while (len >= 2) {
        s = FLASH_ProgramHalfWord(addr, *p++);
        if (s != FLASH_COMPLETE) { FLASH_Lock(); return 0; }
        addr += 2;
        len  -= 2;
    }
    if (len == 1) {
        uint16_t last = *((uint8_t *)p) | 0xFF00;
        s = FLASH_ProgramHalfWord(addr, last);
        if (s != FLASH_COMPLETE) { FLASH_Lock(); return 0; }
    }
    FLASH_Lock();
    return 1;
}

// ─── Storage partition ─────────────────────────────────────────────

uint8_t storage_erase_all(void)
{
    uint8_t i;
    for (i = 0; i < STORAGE_SIZE / FLASH_PAGE_SIZE; i++) {
        if (!flash_erase_page(STORAGE_ADDR + (uint32_t)i * FLASH_PAGE_SIZE))
            return 0;
    }
    return 1;
}

uint8_t storage_write_page(uint8_t idx, uint8_t *data)
{
    uint32_t addr;
    if (idx >= STORAGE_SIZE / FLASH_PAGE_SIZE) return 0;
    addr = STORAGE_ADDR + (uint32_t)idx * FLASH_PAGE_SIZE;
    if (!flash_erase_page(addr)) return 0;
    return flash_write_buf(addr, data, FLASH_PAGE_SIZE);
}

uint8_t storage_set_ready_flag(void)
{
    uint16_t flag = FW_FLAG_READY;
    flash_erase_page(FW_FLAG_ADDR);
    FLASH_Unlock();
    FLASH_ProgramHalfWord(FW_FLAG_ADDR, flag);
    FLASH_Lock();
    return 1;
}
