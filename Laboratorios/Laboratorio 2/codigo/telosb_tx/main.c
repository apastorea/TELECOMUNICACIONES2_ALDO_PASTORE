#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "cc2420.h"
#include "init_dev.h"

#include "net/netdev.h"
#include "net/netdev/ieee802154.h"
#include "net/netopt.h"
#include "net/ieee802154.h"

#include "iolist.h"
#include "byteorder.h"
#include "container.h"
#include "xtimer.h"

#include "test_utils/netdev_ieee802154_minimal.h"

static cc2420_t cc2420[CC2420_NUM];

int netdev_ieee802154_minimal_init_devs(netdev_event_cb_t cb)
{
    puts("Inicializando CC2420...");

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

static int send_packet(netdev_t *netdev,
                       uint8_t dest_id,
                       uint16_t app_seq)
{
    int res;
    netdev_ieee802154_t *dev;
    uint8_t dst[2] = { 0x00, dest_id };
    size_t dst_len = sizeof(dst);
    uint8_t mhr[IEEE802154_MAX_HDR_LEN];
    uint8_t flags;
    uint8_t *src;
    size_t src_len;
    le_uint16_t src_pan;
    le_uint16_t dst_pan = { 0 };

    char payload[32];
    int payload_len = snprintf(payload, sizeof(payload),
                               "NODE=%u SEQ=%u",
                               (unsigned)NODE_ID,
                               (unsigned)app_seq);
    if ((payload_len < 0) ||
        ((size_t)payload_len >= sizeof(payload))) {
        puts("Error creando payload");
        return -1;
    }

    iolist_t iol_data = {
        .iol_base = payload,
        .iol_len = (size_t)payload_len,
        .iol_next = NULL
    };

    dev = container_of(netdev, netdev_ieee802154_t, netdev);
    flags = (uint8_t)(dev->flags & NETDEV_IEEE802154_SEND_MASK);
    flags |= IEEE802154_FCF_TYPE_DATA;

    src_pan = byteorder_btols(byteorder_htons(dev->pan));
    if (dst_pan.u16 == 0) {
        dst_pan = src_pan;
    }

    if (dev->flags & NETDEV_IEEE802154_SRC_MODE_LONG) {
        src_len = IEEE802154_LONG_ADDRESS_LEN;
        src = dev->long_addr;
    }
    else {
        src_len = IEEE802154_SHORT_ADDRESS_LEN;
        src = dev->short_addr;
    }

    res = ieee802154_set_frame_hdr(mhr,
                                   src, src_len,
                                   dst, dst_len,
                                   src_pan, dst_pan,
                                   flags, dev->seq++);
    if (res < 0) {
        puts("Error preparando trama");
        return -1;
    }

    iolist_t iol_hdr = {
        .iol_next = &iol_data,
        .iol_base = mhr,
        .iol_len = (size_t)res
    };

    /* El driver copia cabecera y payload al FIFO del CC2420. */
    res = dev->netdev.driver->send(&dev->netdev, &iol_hdr);
    if (res <= 0) {
        printf("Error enviando trama: %d\n", res);
        return -1;
    }

    return 0;
}

int main(void)
{
    puts("");
    puts("====================================");
    puts(" Telecomunicaciones 2");
    puts(" TelosB TX - IEEE 802.15.4");
    puts("====================================");

    int res = netdev_ieee802154_minimal_init();
    if (res < 0) {
        puts("Error inicializando IEEE 802.15.4");
        return 1;
    }

    netdev_t *netdev = &cc2420[0].netdev.netdev;
    uint8_t short_addr[2] = { 0x00, (uint8_t)NODE_ID };

    res = netdev_ieee802154_minimal_set(netdev,
                                        NETOPT_ADDRESS,
                                        short_addr,
                                        sizeof(short_addr));
    if (res < 0) {
        printf("Error configurando direccion: %d\n", res);
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
        return 1;
    }

    printf("Nodo origen  : 00:%02X\n", (unsigned)NODE_ID);
    printf("Nodo destino : 00:%02X\n", (unsigned)DEST_ID);
#if CSMA_ENABLED
    puts("CSMA         : ON");
#else
    puts("CSMA         : OFF");
#endif

    uint16_t app_seq = 0;
    while (1) {
        printf("TX node=%u seq=%u -> 00:%02X\n",
               (unsigned)NODE_ID,
               (unsigned)app_seq,
               (unsigned)DEST_ID);

        if (send_packet(netdev, DEST_ID, app_seq) < 0) {
            puts("Fallo de transmision");
        }
        app_seq++;
        xtimer_sleep(1);
    }

    return 0;
}
