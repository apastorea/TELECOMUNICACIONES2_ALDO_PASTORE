#include <stdio.h>
#include <stdint.h>

#include "cc2420.h"
#include "init_dev.h"
#include "shell.h"

#include "net/netdev.h"
#include "net/netopt.h"

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

int main(void)
{
    puts("");
    puts("====================================");
    puts(" Telecomunicaciones 2");
    puts(" TelosB RX - IEEE 802.15.4");
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

    printf("Nodo receptor : 00:%02X\n", (unsigned)NODE_ID);
    puts("Esperando paquetes...");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, sizeof(line_buf));

    return 0;
}
