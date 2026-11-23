#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "cc2420.h"
#include "init_dev.h"
#include "msg.h"
#include "net/ieee802154.h"
#include "net/netdev.h"
#include "net/netdev/ieee802154.h"
#include "net/netopt.h"
#include "shell.h"
#include "thread.h"

#if NODE_ID > 255
#error "NODE_ID debe estar entre 0 y 255"
#endif

#define RADIO_MSG_ISR (0x4c31)
#define RADIO_QUEUE_SIZE (8U)
#define DUP_CACHE_SIZE (8U)

typedef struct {
    uint8_t src;
    uint8_t dst;
    uint16_t seq;
    uint8_t hops;
    uint8_t ttl;
} app_packet_t;

typedef struct {
    bool valid;
    uint8_t src;
    uint16_t seq;
} duplicate_entry_t;

typedef struct {
    uint32_t unique;
    uint32_t duplicates;
    uint32_t ack_frames;
    uint32_t gaps;
    uint32_t out_of_order;
    uint32_t invalid;
    bool sequence_started;
    uint16_t expected_seq;
} sink_stats_t;

static cc2420_t cc2420[CC2420_NUM];
static kernel_pid_t radio_pid = KERNEL_PID_UNDEF;
static char radio_stack[RADIO_THREAD_STACKSIZE];
static msg_t radio_queue[RADIO_QUEUE_SIZE];
static uint8_t rx_buffer[IEEE802154_FRAME_LEN_MAX];
static duplicate_entry_t duplicate_cache[DUP_CACHE_SIZE];
static unsigned duplicate_next;
static sink_stats_t stats;

static bool packet_is_duplicate(uint8_t src, uint16_t seq)
{
    for (unsigned i = 0; i < DUP_CACHE_SIZE; i++) {
        if (duplicate_cache[i].valid &&
            (duplicate_cache[i].src == src) &&
            (duplicate_cache[i].seq == seq)) {
            return true;
        }
    }

    duplicate_cache[duplicate_next].valid = true;
    duplicate_cache[duplicate_next].src = src;
    duplicate_cache[duplicate_next].seq = seq;
    duplicate_next = (duplicate_next + 1U) % DUP_CACHE_SIZE;
    return false;
}

static bool parse_packet(const uint8_t *payload,
                         size_t payload_len,
                         app_packet_t *packet)
{
    char text[56];

    if ((payload_len == 0) || (payload_len >= sizeof(text))) {
        return false;
    }

    memcpy(text, payload, payload_len);
    text[payload_len] = '\0';

    unsigned src;
    unsigned dst;
    unsigned seq;
    unsigned hops;
    unsigned ttl;

    if (sscanf(text,
               "SRC=%u DST=%u SEQ=%u HOPS=%u TTL=%u",
               &src,
               &dst,
               &seq,
               &hops,
               &ttl) != 5) {
        return false;
    }

    if ((src > 255U) || (dst > 255U) || (seq > 65535U) ||
        (hops > 255U) || (ttl > 255U)) {
        return false;
    }

    packet->src = (uint8_t)src;
    packet->dst = (uint8_t)dst;
    packet->seq = (uint16_t)seq;
    packet->hops = (uint8_t)hops;
    packet->ttl = (uint8_t)ttl;
    return true;
}

static void update_sequence_stats(uint16_t seq)
{
    if (!stats.sequence_started) {
        stats.sequence_started = true;
        stats.expected_seq = (uint16_t)(seq + 1U);
        return;
    }

    int16_t delta = (int16_t)(seq - stats.expected_seq);

    if (delta == 0) {
        stats.expected_seq++;
    }
    else if (delta > 0) {
        stats.gaps += (uint16_t)delta;
        stats.expected_seq = (uint16_t)(seq + 1U);
    }
    else {
        stats.out_of_order++;
    }
}

static void process_received_frame(netdev_t *dev)
{
    netdev_ieee802154_rx_info_t rx_info;
    int data_len = dev->driver->recv(dev,
                                     rx_buffer,
                                     sizeof(rx_buffer),
                                     &rx_info);

    if (data_len <= 0) {
        stats.invalid++;
        printf("SINK trama descartada len=%d\n", data_len);
        return;
    }

    uint8_t frame_type = rx_buffer[0] & IEEE802154_FCF_TYPE_MASK;

    if (frame_type == IEEE802154_FCF_TYPE_ACK) {
        stats.ack_frames++;
        return;
    }

    if (frame_type != IEEE802154_FCF_TYPE_DATA) {
        stats.invalid++;
        printf("SINK tipo de trama no soportado: %u\n",
               (unsigned)frame_type);
        return;
    }

    size_t mhr_len = ieee802154_get_frame_hdr_len(rx_buffer);
    if ((mhr_len == 0) || (mhr_len >= (size_t)data_len)) {
        stats.invalid++;
        printf("SINK cabecera invalida len=%d mhr=%u\n",
               data_len,
               (unsigned)mhr_len);
        return;
    }

    app_packet_t packet;
    const uint8_t *payload = rx_buffer + mhr_len;
    size_t payload_len = (size_t)data_len - mhr_len;

    if (!parse_packet(payload, payload_len, &packet)) {
        stats.invalid++;
        puts("SINK payload invalido");
        return;
    }

    if (packet.dst != (uint8_t)NODE_ID) {
        printf("SINK ignora dst=%u seq=%u\n",
               (unsigned)packet.dst,
               (unsigned)packet.seq);
        return;
    }

    if (packet_is_duplicate(packet.src, packet.seq)) {
        stats.duplicates++;
        printf("SINK DUP src=%u seq=%u RSSI=%d LQI=%u\n",
               (unsigned)packet.src,
               (unsigned)packet.seq,
               rx_info.rssi,
               rx_info.lqi);
        LED2_TOGGLE;
        return;
    }

    stats.unique++;
    update_sequence_stats(packet.seq);
    LED0_TOGGLE;

    printf("SINK rx src=%u dst=%u seq=%u hops=%u ttl=%u "
           "RSSI=%d LQI=%u\n",
           (unsigned)packet.src,
           (unsigned)packet.dst,
           (unsigned)packet.seq,
           (unsigned)packet.hops,
           (unsigned)packet.ttl,
           rx_info.rssi,
           rx_info.lqi);
}

static void radio_event_cb(netdev_t *dev, netdev_event_t event)
{
    if (event == NETDEV_EVENT_ISR) {
        msg_t msg = {
            .type = RADIO_MSG_ISR,
            .content.ptr = dev
        };

        if (msg_send(&msg, radio_pid) <= 0) {
            puts("SINK interrupcion perdida");
        }
        return;
    }

    switch (event) {
        case NETDEV_EVENT_RX_COMPLETE:
            process_received_frame(dev);
            break;

        case NETDEV_EVENT_LINK_UP:
            break;

        default:
            break;
    }
}

static void *radio_thread(void *arg)
{
    netdev_t *dev = arg;
    msg_init_queue(radio_queue, RADIO_QUEUE_SIZE);

    while (1) {
        msg_t msg;
        msg_receive(&msg);

        if (msg.type == RADIO_MSG_ISR) {
            dev->driver->isr(dev);
        }
    }

    return NULL;
}

static int stats_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint32_t expected = stats.unique + stats.gaps;
    uint32_t prr_permille = expected ?
                            (stats.unique * 1000UL) / expected : 0;

    printf("unique=%lu duplicates=%lu ack_frames=%lu gaps=%lu\n",
           (unsigned long)stats.unique,
           (unsigned long)stats.duplicates,
           (unsigned long)stats.ack_frames,
           (unsigned long)stats.gaps);
    printf("out_of_order=%lu invalid=%lu\n",
           (unsigned long)stats.out_of_order,
           (unsigned long)stats.invalid);
    printf("PRR_estimado=%lu.%lu%%\n",
           (unsigned long)(prr_permille / 10UL),
           (unsigned long)(prr_permille % 10UL));
    return 0;
}

SHELL_COMMAND(stats, "Muestra estadisticas del receptor", stats_cmd);

static int resetstats_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    memset(&stats, 0, sizeof(stats));
    memset(duplicate_cache, 0, sizeof(duplicate_cache));
    duplicate_next = 0;
    puts("Estadisticas reiniciadas");
    return 0;
}

SHELL_COMMAND(resetstats, "Reinicia estadisticas del receptor", resetstats_cmd);

int main(void)
{
    LED0_OFF;
    LED1_OFF;
    LED2_OFF;

    puts("");
    puts("====================================");
    puts(" Telecomunicaciones 2");
    puts(" Lab 3 - TelosB SINK");
    puts("====================================");

    cc2420_setup(&cc2420[0], &cc2420_params[0], 0);
    netdev_t *netdev = &cc2420[0].netdev.netdev;
    netdev->event_callback = radio_event_cb;

    radio_pid = thread_create(radio_stack,
                              sizeof(radio_stack),
                              THREAD_PRIORITY_MAIN - 1,
                              0,
                              radio_thread,
                              netdev,
                              "radio");

    if (radio_pid <= KERNEL_PID_UNDEF) {
        puts("Error creando hilo de radio");
        LED2_ON;
        return 1;
    }

    int res = netdev->driver->init(netdev);
    if (res < 0) {
        printf("Error inicializando CC2420: %d\n", res);
        LED2_ON;
        return 1;
    }

    uint8_t short_addr[IEEE802154_SHORT_ADDRESS_LEN] = {
        0x00,
        (uint8_t)NODE_ID
    };

    res = netdev->driver->set(netdev,
                              NETOPT_ADDRESS,
                              short_addr,
                              sizeof(short_addr));
    if (res < 0) {
        printf("Error configurando direccion: %d\n", res);
        LED2_ON;
        return 1;
    }

    printf("Nodo receptor: 00:%02X\n", (unsigned)NODE_ID);
    puts("LED0 cambia con cada paquete valido");
    puts("LED2 cambia al detectar un duplicado");
    puts("Comandos: stats, resetstats");

    LED1_ON;

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, sizeof(line_buf));

    return 0;
}
