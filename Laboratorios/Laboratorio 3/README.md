# Laboratorio 3 — Comunicación multisalto IEEE 802.15.4

**Objetivo:** implementar una ruta de dos saltos con tres TelosB y verificar el reenvío, los campos de control y la entrega de mensajes en el destino final.

**Hardware:** 3 × TelosB (MSP430F1611 y CC2420) · **Software:** RIOT OS 2026.04.

## Topología

```text
SENSOR (ID 1) ──────► RELAY (ID 2) ──────► SINK (ID 3)
  HOPS=0, TTL=2        HOPS=1, TTL=1       Destino final
```

En el montaje, **SENSOR–RELAY** y **SENSOR–SINK** estaban separados aproximadamente **5 m** cada uno, mientras que **RELAY–SINK** estaban a **1 m**. Se verificó una ruta lógica configurada de dos saltos; no se ensayó el alcance máximo ni se descartó experimentalmente un enlace directo.

| Nodo | Programa | Función |
| --- | --- | --- |
| SENSOR (1) | [`telosb_sensor`](./src/lab3_multihop/telosb_sensor/) | Genera `SRC`, `DST`, `SEQ`, `HOPS` y `TTL`. |
| RELAY (2) | [`telosb_relay`](./src/lab3_multihop/telosb_relay/) | Valida tramas, detecta duplicados y reenvía si el TTL lo permite. |
| SINK (3) | [`telosb_sink`](./src/lab3_multihop/telosb_sink/) | Recibe y contabiliza paquetes destinados al nodo 3. |

En el RELAY se incrementa `HOPS` y se decrementa `TTL` antes del reenvío. Las consolas de RELAY y SINK admiten `stats` y `resetstats`; los registros incluyen RSSI y LQI. Las tramas ACK MAC observadas no equivalen a confirmaciones extremo a extremo.

## Resultados obtenidos

| Ensayo | Recibidos por RELAY | Reenvíos aceptados | Únicos en SINK | PRR estimado en SINK |
| --- | ---: | ---: | ---: | ---: |
| `CSMA=0` | 182 | 182 | 182 | 99,4 % |
| `CSMA=1` | 227 | 227 | 227 | 100,0 % |
| `INITIAL_TTL=0` | 63 | 0 | 0 | No estimable |

En el ensayo con `TTL=0`, el repetidor recibió **63 paquetes y descartó los 63**, sin retransmitir ninguno. La recepción del primer salto fue correcta, pero no hubo entregas de aplicación al SINK.

El PRR indicado procede de los números de secuencia observados en cada ventana. La diferencia entre 99,4 % y 100,0 % no permite atribuir una mejora exclusivamente a CCA; con cero mensajes recibidos en SINK no es válido estimar un PRR a partir de los huecos de secuencia.

## Material disponible

- [Código SENSOR / RELAY / SINK](./src/lab3_multihop/).
- [Capturas de las tres pruebas](./evidencias/).
- [Informe técnico](./Informe_Lab3_AldoPastore.pdf).
- [Enunciado](./%5BWSN-RIOT%5D%20Lab3_MultiHop_Comm.pdf).

## Ejemplo de compilación

El código permite configurar identificadores, siguiente salto, destino final, TTL, intervalo de envío y CCA. Desde Ubuntu, con RIOT ubicado en `$HOME/RIOT`:

```bash
cd "Laboratorios/Laboratorio 3/src/lab3_multihop/telosb_sensor"
make BOARD=telosb RIOTBASE="$HOME/RIOT" NODE_ID=1 NEXT_HOP_ID=2 \
  FINAL_DEST_ID=3 INITIAL_TTL=2 TX_INTERVAL_MS=1000 CSMA=1
```

Antes de grabar, comprobar qué puerto USB corresponde a cada placa para no sobrescribir el firmware de otro nodo.

---

**Autor:** Aldo Pastore — Telecomunicaciones 2 · Ingeniería Electrónica
