#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "byteorder.h"
#include "cc2420.h"
#include "container.h"
#include "init_dev.h"
#include "iolist.h"
#include "msg.h"
#include "net/ieee802154.h"
#include "net/netdev.h"
#include "net/netdev/ieee802154.h"
#include "net/netopt.h"
#include "shell.h"
#include "thread.h"

#if (NODE_ID > 255) || (NEXT_HOP_ID > 255) || (FINAL_DEST_ID > 255)
#error "Los identificadores de nodo deben estar entre 0 y 255"
#endif

#define RADIO_MSG_ISR (0x4c32)
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
    uint32_t received;
    uint32_t forwarded;
    uint32_t duplicates;
    uint32_t ack_frames;
    uint32_t gaps;
    uint32_t out_of_order;
    uint32_t ttl_drops;
    uint32_t route_drops;
    uint32_t invalid;
    uint32_t send_errors;
    bool sequence_started;
    uint16_t expected_seq;
} relay_stats_t;

static cc2420_t cc2420[CC2420_NUM];
static kernel_pid_t radio_pid = KERNEL_PID_UNDEF;
static char radio_stack[RADIO_THREAD_STACKSIZE];
static msg_t radio_queue[RADIO_QUEUE_SIZE];
static uint8_t rx_buffer[IEEE802154_FRAME_LEN_MAX];
static duplicate_entry_t duplicate_cache[DUP_CACHE_SIZE];
static unsigned duplicate_next;
static relay_stats_t stats;

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

static int forward_packet(netdev_t *netdev, const app_packet_t *packet)
{
    netdev_ieee802154_t *dev = container_of(netdev,
                                             netdev_ieee802154_t,
                                             netdev);

    uint8_t dst[IEEE802154_SHORT_ADDRESS_LEN] = {
        0x00,
        (uint8_t)NEXT_HOP_ID
    };

    char payload[48];
    int payload_len = snprintf(payload,
                               sizeof(payload),
                               "SRC=%u DST=%u SEQ=%u HOPS=%u TTL=%u",
                               (unsigned)packet->src,
                               (unsigned)packet->dst,
                               (unsigned)packet->seq,
                               (unsigned)packet->hops,
                               (unsigned)packet->ttl);

    if ((payload_len < 0) || ((size_t)payload_len >= sizeof(payload))) {
        return -1;
    }

    uint8_t mhr[IEEE802154_MAX_HDR_LEN];
    uint8_t flags = (uint8_t)(dev->flags & NETDEV_IEEE802154_SEND_MASK);
    uint8_t *src;
    size_t src_len;
    le_uint16_t src_pan = byteorder_btols(byteorder_htons(dev->pan));
    le_uint16_t dst_pan = src_pan;

    flags |= IEEE802154_FCF_TYPE_DATA;
    flags |= IEEE802154_FCF_ACK_REQ;

    if (dev->flags & NETDEV_IEEE802154_SRC_MODE_LONG) {
        src = dev->long_addr;
        src_len = IEEE802154_LONG_ADDRESS_LEN;
    }
    else {
        src = dev->short_addr;
        src_len = IEEE802154_SHORT_ADDRESS_LEN;
    }

    int hdr_len = ieee802154_set_frame_hdr(mhr,
                                            src,
                                            src_len,
                                            dst,
                                            sizeof(dst),
                                            src_pan,
                                            dst_pan,
                                            flags,
                                            dev->seq++);

    if (hdr_len < 0) {
        return -1;
    }

    iolist_t iol_data = {
        .iol_base = payload,
        .iol_len = (size_t)payload_len,
        .iol_next = NULL
    };

    iolist_t iol_hdr = {
        .iol_base = mhr,
        .iol_len = (size_t)hdr_len,
        .iol_next = &iol_data
    };

    int res = dev->netdev.driver->send(&dev->netdev, &iol_hdr);
    return (res > 0) ? 0 : -1;
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
        printf("RELAY trama descartada len=%d\n", data_len);
        return;
    }

    uint8_t frame_type = rx_buffer[0] & IEEE802154_FCF_TYPE_MASK;

    if (frame_type == IEEE802154_FCF_TYPE_ACK) {
        stats.ack_frames++;
        return;
    }

    if (frame_type != IEEE802154_FCF_TYPE_DATA) {
        stats.invalid++;
        printf("RELAY tipo de trama no soportado: %u\n",
               (unsigned)frame_type);
        return;
    }

    size_t mhr_len = ieee802154_get_frame_hdr_len(rx_buffer);
    if ((mhr_len == 0) || (mhr_len >= (size_t)data_len)) {
        stats.invalid++;
        printf("RELAY cabecera invalida len=%d mhr=%u\n",
               data_len,
               (unsigned)mhr_len);
        return;
    }

    app_packet_t packet;
    const uint8_t *payload = rx_buffer + mhr_len;
    size_t payload_len = (size_t)data_len - mhr_len;

    if (!parse_packet(payload, payload_len, &packet)) {
        stats.invalid++;
        puts("RELAY payload invalido");
        return;
    }

    if (packet.dst != (uint8_t)FINAL_DEST_ID) {
        stats.route_drops++;
        printf("RELAY sin ruta dst=%u seq=%u\n",
               (unsigned)packet.dst,
               (unsigned)packet.seq);
        return;
    }

    if (packet_is_duplicate(packet.src, packet.seq)) {
        stats.duplicates++;
        printf("RELAY DUP src=%u seq=%u\n",
               (unsigned)packet.src,
               (unsigned)packet.seq);
        return;
    }

    stats.received++;
    update_sequence_stats(packet.seq);
    LED0_TOGGLE;

    printf("RELAY rx src=%u dst=%u seq=%u hops=%u ttl=%u "
           "RSSI=%d LQI=%u\n",
           (unsigned)packet.src,
           (unsigned)packet.dst,
           (unsigned)packet.seq,
           (unsigned)packet.hops,
           (unsigned)packet.ttl,
           rx_info.rssi,
           rx_info.lqi);

    if (packet.ttl == 0U) {
        stats.ttl_drops++;
        printf("RELAY descarta TTL=0 seq=%u\n", (unsigned)packet.seq);
        return;
    }

    packet.hops++;
    packet.ttl--;

    if (forward_packet(dev, &packet) == 0) {
        stats.forwarded++;
        LED2_TOGGLE;
        printf("RELAY fwd seq=%u -> 00:%02X hops=%u ttl=%u\n",
               (unsigned)packet.seq,
               (unsigned)NEXT_HOP_ID,
               (unsigned)packet.hops,
               (unsigned)packet.ttl);
    }
    else {
        stats.send_errors++;
        printf("RELAY error fwd seq=%u\n", (unsigned)packet.seq);
    }
}

static void radio_event_cb(netdev_t *dev, netdev_event_t event)
{
    if (event == NETDEV_EVENT_ISR) {
        msg_t msg = {
            .type = RADIO_MSG_ISR,
            .content.ptr = dev
        };

        if (msg_send(&msg, radio_pid) <= 0) {
            puts("RELAY interrupcion perdida");
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

    uint32_t expected = stats.received + stats.gaps;
    uint32_t prr_permille = expected ?
                            (stats.received * 1000UL) / expected : 0;

    printf("received=%lu forwarded=%lu duplicates=%lu ack_frames=%lu\n",
           (unsigned long)stats.received,
           (unsigned long)stats.forwarded,
           (unsigned long)stats.duplicates,
           (unsigned long)stats.ack_frames);
    printf("gaps=%lu out_of_order=%lu invalid=%lu\n",
           (unsigned long)stats.gaps,
           (unsigned long)stats.out_of_order,
           (unsigned long)stats.invalid);
    printf("ttl_drops=%lu route_drops=%lu send_errors=%lu\n",
           (unsigned long)stats.ttl_drops,
           (unsigned long)stats.route_drops,
           (unsigned long)stats.send_errors);
    printf("PRR_primer_salto=%lu.%lu%%\n",
           (unsigned long)(prr_permille / 10UL),
           (unsigned long)(prr_permille % 10UL));
    return 0;
}

SHELL_COMMAND(stats, "Muestra estadisticas del repetidor", stats_cmd);

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

SHELL_COMMAND(resetstats, "Reinicia estadisticas del repetidor", resetstats_cmd);

int main(void)
{
    LED0_OFF;
    LED1_OFF;
    LED2_OFF;

    puts("");
    puts("====================================");
    puts(" Telecomunicaciones 2");
    puts(" Lab 3 - TelosB RELAY");
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

#if CSMA_ENABLED
    bool csma = true;
#else
    bool csma = false;
#endif

    res = netdev->driver->set(netdev,
                              NETOPT_CSMA,
                              &csma,
                              sizeof(csma));
    if (res < 0) {
        printf("Error configurando CSMA: %d\n", res);
        LED2_ON;
        return 1;
    }

    printf("Nodo repetidor : 00:%02X\n", (unsigned)NODE_ID);
    printf("Siguiente salto: 00:%02X\n", (unsigned)NEXT_HOP_ID);
    printf("Destino final  : %u\n", (unsigned)FINAL_DEST_ID);
#if CSMA_ENABLED
    puts("CSMA/CCA       : ON");
#else
    puts("CSMA/CCA       : OFF");
#endif
    puts("LED0 cambia al recibir; LED2 cambia al reenviar");
    puts("Comandos: stats, resetstats");

    LED1_ON;

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, sizeof(line_buf));

    return 0;
}
