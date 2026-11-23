# Laboratorio 2 — Enlace punto a punto IEEE 802.15.4

**Objetivo:** implementar la comunicación inalámbrica entre dos nodos TelosB y registrar su comportamiento ante cambios de distancia, presencia de obstáculos y configuración de CSMA.

**Hardware:** 2 × TelosB (MSP430F1611 y CC2420) · **Software:** RIOT OS 2026.04.

## Montaje y programas

- **Transmisor TX (nodo 1, dirección `00:01`):** genera tramas IEEE 802.15.4 hacia el nodo 2 con carga útil `NODE=1 SEQ=s`. El código utiliza un intervalo nominal de un segundo y permite alternar `CSMA=1` y `CSMA=0`.
- **Receptor RX (nodo 2, dirección `00:02`):** inicializa el CC2420 y utiliza la interfaz de recepción preparada para el registro experimental.

La configuración documentada utiliza **canal 26**, **PAN ID `0x0023`**, potencia de transmisión predeterminada y monitor serie `pyterm` a **9600 bit/s**. El registro normalizado de recepción incluye secuencia, RSSI y LQI; véase la nota de reproducibilidad.

## Condiciones registradas

Resumen de [`condiciones.csv`](./datos_crudos/condiciones.csv):

| Serie | Distancia | CSMA | Condición | Resultado registrado |
| --- | ---: | --- | --- | --- |
| A | 0,5 m | ON | Línea de vista | Sin pérdidas observadas |
| B | 3 m | ON | Línea de vista | Sin pérdidas observadas |
| C | 3 m | OFF | Línea de vista | 120 pérdidas de 131 paquetes esperados |
| D-ON | 3 m | ON | Obstáculo controlado | 108 pérdidas de 110 paquetes esperados |
| D-OFF | 3 m | OFF | Obstáculo controlado | 106 pérdidas de 110 paquetes esperados |

Estos valores describen sesiones concretas; no permiten afirmar, por sí solos, una relación causal general entre activar CSMA y mejorar la recepción.

Los cálculos y la presentación ampliada se encuentran en la [planilla de resultados](./resultados/Resultados_Laboratorio_2.xlsx) y el [informe](./informe/Informe_Laboratorio2_Teleco2_Aldo_Pastore.pdf).

## Organización de archivos

- [`codigo/telosb_tx/`](./codigo/telosb_tx/): programa transmisor y Makefile.
- [`codigo/telosb_rx/`](./codigo/telosb_rx/): programa receptor y Makefile.
- [`configuracion/configuracion_versiones.txt`](./configuracion/configuracion_versiones.txt): versiones y parámetros.
- [`datos_crudos/condiciones.csv`](./datos_crudos/condiciones.csv): condiciones de las series.
- [`resultados/`](./resultados/): libro Excel con resultados.
- [`informe/`](./informe/): memoria técnica.

## Reproducibilidad

En una copia independiente del repositorio debe indicarse la ruta a RIOT porque los Makefiles utilizan una ruta relativa heredada del espacio de trabajo original:

```bash
cd "Laboratorios/Laboratorio 2/codigo/telosb_tx"
make BOARD=telosb RIOTBASE="$HOME/RIOT" NODE_ID=1 DEST_ID=2 CSMA=1
```

Para el receptor, ingresar en `codigo/telosb_rx` y compilar con `RIOTBASE="$HOME/RIOT" NODE_ID=2`.

**Dependencia documentada:** el archivo de configuración registra una modificación local del manejo de recepción en RIOT, con validación y extracción del payload para producir líneas `RX node=n seq=s RSSI=r LQI=q`. La configuración menciona una carpeta `patches/`, pero dicha carpeta **no figura entre los archivos publicados en este laboratorio**. Por ello, reproducir exactamente las salidas RX puede exigir recuperar e incorporar ese parche al árbol RIOT.

---

**Autor:** Aldo Pastore — Telecomunicaciones 2 · Ingeniería Electrónica
