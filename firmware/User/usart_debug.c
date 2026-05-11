#include "usart_debug.h"

typedef struct {
    uint8_t *buf;
    volatile uint16_t head;
    volatile uint16_t tail;
    uint16_t size;
} ring_buf_t;

static uint8_t tx_buf_mem[USART_DEBUG_TX_BUF];
static uint8_t rx_buf_mem[USART_DEBUG_RX_BUF];
static ring_buf_t tx_rb;
static ring_buf_t rx_rb;

static void rb_init(ring_buf_t *rb, uint8_t *buf, uint16_t size)
{
    rb->buf  = buf;
    rb->head = 0;
    rb->tail = 0;
    rb->size = size;
}

static uint8_t rb_put(ring_buf_t *rb, uint8_t data)
{
    uint16_t next = (rb->head + 1) % rb->size;
    if (next == rb->tail) return 0;
    rb->buf[rb->head] = data;
    rb->head = next;
    return 1;
}

static uint8_t rb_get(ring_buf_t *rb, uint8_t *data)
{
    if (rb->tail == rb->head) return 0;
    *data = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;
    return 1;
}

static uint16_t rb_available(ring_buf_t *rb)
{
    return (rb->head - rb->tail + rb->size) % rb->size;
}

void usart_debug_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    rb_init(&tx_rb, tx_buf_mem, USART_DEBUG_TX_BUF);
    rb_init(&rx_rb, rx_buf_mem, USART_DEBUG_RX_BUF);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    // PA9 = TX, PA10 = RX
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin  = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    usart.USART_BaudRate            = USART_DEBUG_BAUD;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART_DEBUG, &usart);

    nvic.NVIC_IRQChannel                   = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    USART_ITConfig(USART_DEBUG, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART_DEBUG, ENABLE);
}

void usart_debug_send_byte(uint8_t data)
{
    while (!rb_put(&tx_rb, data));
    USART_ITConfig(USART_DEBUG, USART_IT_TXE, ENABLE);
}

void usart_debug_send_bytes(uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        usart_debug_send_byte(buf[i]);
    }
}

uint16_t usart_debug_recv_bytes(uint8_t *buf, uint16_t max_len)
{
    uint16_t cnt = 0;
    while (cnt < max_len && rb_get(&rx_rb, &buf[cnt])) {
        cnt++;
    }
    return cnt;
}

uint8_t usart_debug_rx_available(void)
{
    return rb_available(&rx_rb) > 0;
}

void usart_debug_flush_rx(void)
{
    rx_rb.tail = rx_rb.head;
}

// printf retarget: called by printf -> _sys_write -> fputc
#if defined(__MICROLIB)
int fputc(int ch, FILE *f)
{
    (void)f;
    usart_debug_send_byte((uint8_t)ch);
    return ch;
}
#else
#pragma import(__use_no_semihosting)
struct __FILE { int handle; };
FILE __stdout;

void _sys_exit(int x)
{
    (void)x;
    while (1);
}

int fputc(int ch, FILE *f)
{
    (void)f;
    usart_debug_send_byte((uint8_t)ch);
    return ch;
}

int fgetc(FILE *f)
{
    uint8_t ch;
    (void)f;
    while (!rb_get(&rx_rb, &ch));
    return (int)ch;
}
#endif

void USART1_IRQHandler(void)
{
    uint8_t data;

    if (USART_GetITStatus(USART_DEBUG, USART_IT_RXNE) != RESET) {
        data = (uint8_t)USART_ReceiveData(USART_DEBUG);
        rb_put(&rx_rb, data);
        USART_ClearITPendingBit(USART_DEBUG, USART_IT_RXNE);
    }

    if (USART_GetITStatus(USART_DEBUG, USART_IT_TXE) != RESET) {
        if (rb_get(&tx_rb, &data)) {
            USART_SendData(USART_DEBUG, data);
        } else {
            USART_ITConfig(USART_DEBUG, USART_IT_TXE, DISABLE);
        }
        USART_ClearITPendingBit(USART_DEBUG, USART_IT_TXE);
    }
}
