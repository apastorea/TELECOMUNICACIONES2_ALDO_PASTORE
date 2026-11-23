#pragma once

#include "cc2420_params.h"
#include "kernel_defines.h"

/* Cantidad de radios CC2420 disponibles en la placa. */
#define CC2420_NUM ARRAY_SIZE(cc2420_params)

/* Requerido por test_utils_netdev_ieee802154_minimal. */
#define NETDEV_IEEE802154_MINIMAL_NUMOF CC2420_NUM
