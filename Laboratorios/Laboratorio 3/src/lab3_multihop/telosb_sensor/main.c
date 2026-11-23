#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "byteorder.h"
#include "cc2420.h"
#include "container.h"
#include "init_dev.h"
#include "iolist.h"
#include "net/ieee802154.h"
#include "net/netdev.h"
#include "net/netdev/ieee802154.h"
#include "net/netopt.h"
#include "test_utils/netdev_ieee802154_minimal.h"
#include "xtimer.h"

#if (NODE_ID > 255) || (NEXT_HOP_ID > 255) || (FINAL_DEST_ID > 255)
#error "Los identificadores de nodo deben estar entre 0 y 255"
#endif

#if INITIAL_TTL > 255
#error "INITIAL_TTL debe estar entre 0 y 255"
#endif

static cc2420_t cc2420[CC2420_NUM];

int netdev_ieee802154_minimal_init_devs(netdev_event_cb_t cb)
{
    for (unsigned i = 0; i < CC2420_NUM; i++) {
        netdev_t *netdev = &cc2420[i].netdev.netdev;

        cc2420_setup(&cc2420[i], &cc2420_params[i], i);
        netdev->event_callback = cb;

        int res = netdev->driver->init(netdev);
        if (res < 0) {
            printf("Error inicializando CC2420: %d\n", res);
            return res;
        }
    }

    return 0;
}

static int send_packet(netdev_t *netdev, uint16_t app_seq)
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
                               "SRC=%u DST=%u SEQ=%u HOPS=0 TTL=%u",
                               (unsigned)NODE_ID,
                               (unsigned)FINAL_DEST_ID,
                               (unsigned)app_seq,
                               (unsigned)INITIAL_TTL);

    if ((payload_len < 0) || ((size_t)payload_len >= sizeof(payload))) {
        puts("Error creando payload");
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
        puts("Error preparando trama");
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

    int res = netdev_ieee802154_minimal_send(netdev, &iol_hdr);
    return (res > 0) ? 0 : -1;
}

int main(void)
{
    LED0_OFF;
    LED1_OFF;
    LED2_OFF;

    puts("");
    puts("====================================");
    puts(" Telecomunicaciones 2");
    puts(" Lab 3 - TelosB SENSOR");
    puts("====================================");

    int res = netdev_ieee802154_minimal_init();
    if (res < 0) {
        LED2_ON;
        return 1;
    }

    netdev_t *netdev = &cc2420[0].netdev.netdev;
    netdev_ieee802154_t *dev = container_of(netdev,
                                             netdev_ieee802154_t,
                                             netdev);

    uint8_t short_addr[IEEE802154_SHORT_ADDRESS_LEN] = {
        0x00,
        (uint8_t)NODE_ID
    };

    res = netdev_ieee802154_minimal_set(netdev,
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

    res = netdev_ieee802154_minimal_set(netdev,
                                         NETOPT_CSMA,
                                         &csma,
                                         sizeof(csma));
    if (res < 0) {
        printf("Error configurando CSMA: %d\n", res);
        LED2_ON;
        return 1;
    }

    dev->flags |= NETDEV_IEEE802154_ACK_REQ;

    printf("Nodo sensor    : 00:%02X\n", (unsigned)NODE_ID);
    printf("Siguiente salto: 00:%02X\n", (unsigned)NEXT_HOP_ID);
    printf("Destino final  : %u\n", (unsigned)FINAL_DEST_ID);
    printf("TTL inicial    : %u\n", (unsigned)INITIAL_TTL);
    printf("Periodo        : %u ms\n", (unsigned)TX_INTERVAL_MS);
#if CSMA_ENABLED
    puts("CSMA/CCA       : ON");
#else
    puts("CSMA/CCA       : OFF");
#endif

    LED1_ON;

    uint16_t app_seq = 0;

    while (1) {
        res = send_packet(netdev, app_seq);

        if (res == 0) {
            printf("SENSOR tx seq=%u next=%u final=%u\n",
                   (unsigned)app_seq,
                   (unsigned)NEXT_HOP_ID,
                   (unsigned)FINAL_DEST_ID);

            LED0_ON;
            xtimer_msleep(80);
            LED0_OFF;
        }
        else {
            printf("SENSOR error tx seq=%u\n", (unsigned)app_seq);
            LED2_ON;
        }

        app_seq++;
        xtimer_msleep(TX_INTERVAL_MS);
    }

    return 0;
}
