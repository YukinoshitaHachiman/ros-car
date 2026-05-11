#include "iap_protocol.h"
#include "flash_partition.h"
#include "flash_ops.h"
#include "usart_debug.h"
#include <stdio.h>

// ─── Frame protocol constants ─────────────────────────────────────

#define FRAME_HEAD1  0xAA
#define FRAME_HEAD2  0x55

#define CMD_ERASE    0x45  // 'E'
#define CMD_DATA     0x44  // 'D'
#define CMD_FINISH   0x46  // 'F'
#define CMD_INFO     0x49  // 'I'

#define MAX_PAYLOAD  (FLASH_PAGE_SIZE + 4)  // data + page_idx(2) + crc(2)

// ─── Parser state ─────────────────────────────────────────────────

enum {
    ST_IDLE = 0,
    ST_HEAD2,
    ST_CMD,
    ST_LEN_L,
    ST_LEN_H,
    ST_DATA,
    ST_CRC_L,
    ST_CRC_H,
};

static uint8_t  rx_buf[MAX_PAYLOAD];
static uint8_t  rx_state;
static uint8_t  rx_cmd;
static uint16_t rx_len;
static uint16_t rx_idx;
static uint32_t total_written;

static void frame_reset(void)
{
    rx_state = ST_IDLE;
    rx_idx   = 0;
}

// ─── CRC16 ────────────────────────────────────────────────────────

static uint16_t crc16_update(uint16_t crc, uint8_t byte)
{
    uint8_t i;
    crc ^= (uint16_t)byte << 8;
    for (i = 0; i < 8; i++)
        crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    return crc;
}

static uint16_t crc16_buf(uint8_t *p, uint16_t n)
{
    uint16_t crc = 0;
    while (n--) crc = crc16_update(crc, *p++);
    return crc;
}

// ─── Response ─────────────────────────────────────────────────────

static void send_resp(uint8_t cmd, uint8_t status)
{
    uint8_t resp[] = { FRAME_HEAD1, FRAME_HEAD2, cmd, status };
    usart_debug_send_bytes(resp, sizeof(resp));
}

// ─── Command dispatch ─────────────────────────────────────────────

static void frame_dispatch(void)
{
    uint16_t crc_recv, crc_calc;
    uint8_t  page_idx;

    if (rx_len + 2 > MAX_PAYLOAD) {
        send_resp(rx_cmd, 0x02);
        return;
    }

    crc_recv = rx_buf[rx_len] | ((uint16_t)rx_buf[rx_len + 1] << 8);
    crc_calc = crc16_update(0, rx_cmd);
    crc_calc = crc16_update(crc_calc, (uint8_t)(rx_len & 0xFF));
    crc_calc = crc16_update(crc_calc, (uint8_t)(rx_len >> 8));
    crc_calc = crc16_buf(rx_buf, rx_len);

    if (crc_recv != crc_calc) {
        send_resp(rx_cmd, 0x03);
        return;
    }

    switch (rx_cmd) {
    case CMD_ERASE:
        if (storage_erase_all()) {
            total_written = 0;
            send_resp(CMD_ERASE, 0x00);
            printf("storage erased ok\r\n");
        } else {
            send_resp(CMD_ERASE, 0x01);
        }
        break;

    case CMD_DATA:
        if (rx_len < 2 + FLASH_PAGE_SIZE) {
            send_resp(CMD_DATA, 0x02);
            break;
        }
        page_idx = rx_buf[0] | ((uint16_t)rx_buf[1] << 8);
        if (storage_write_page(page_idx, &rx_buf[2])) {
            total_written += FLASH_PAGE_SIZE;
            send_resp(CMD_DATA, 0x00);
        } else {
            send_resp(CMD_DATA, 0x01);
            printf("write err page %d\r\n", page_idx);
        }
        break;

    case CMD_FINISH:
        storage_set_ready_flag();
        send_resp(CMD_FINISH, 0x00);
        printf("done, %lu bytes\r\n", total_written);
        break;

    case CMD_INFO:
        send_resp(CMD_INFO, 0x00);
        printf("IAP:bl=%dK ap=%dK st=%dK\r\n",
               BOOTLOADER_SIZE / 1024,
               APP_SIZE / 1024,
               STORAGE_SIZE / 1024);
        break;

    default:
        send_resp(rx_cmd, 0xFF);
        break;
    }
}

// ─── Public API ───────────────────────────────────────────────────

void iap_protocol_init(void)
{
    frame_reset();
    total_written = 0;
}

void iap_protocol_feed(uint8_t byte)
{
    switch (rx_state) {
    case ST_IDLE:
        if (byte == FRAME_HEAD1) rx_state = ST_HEAD2;
        break;

    case ST_HEAD2:
        if (byte == FRAME_HEAD2) rx_state = ST_CMD;
        else rx_state = ST_IDLE;
        break;

    case ST_CMD:
        rx_cmd  = byte;
        rx_state = ST_LEN_L;
        break;

    case ST_LEN_L:
        rx_len   = byte;
        rx_state = ST_LEN_H;
        break;

    case ST_LEN_H:
        rx_len  |= (uint16_t)byte << 8;
        rx_idx   = 0;
        rx_state = (rx_len == 0) ? ST_CRC_L : ST_DATA;
        if (rx_len > MAX_PAYLOAD) frame_reset();
        break;

    case ST_DATA:
        if (rx_idx < MAX_PAYLOAD) rx_buf[rx_idx++] = byte;
        if (rx_idx >= rx_len) rx_state = ST_CRC_L;
        break;

    case ST_CRC_L:
        rx_buf[rx_idx++] = byte;
        rx_state = ST_CRC_H;
        break;

    case ST_CRC_H:
        rx_buf[rx_idx] = byte;
        frame_dispatch();
        frame_reset();
        break;
    }
}
