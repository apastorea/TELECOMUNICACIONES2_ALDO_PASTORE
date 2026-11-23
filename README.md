# Telecomunicaciones 2 — Ingeniería Electrónica

Repositorio académico de **Aldo Pastore**, Facultad de Ciencias y Tecnología, Universidad Católica «Nuestra Señora de la Asunción» (Asunción, Paraguay).

Reúne código fuente, configuraciones, informes y evidencias experimentales de las prácticas de **Telecomunicaciones 2 (2026)**, desarrolladas principalmente con RIOT OS y placas TelosB.

## Laboratorios

| Práctica | Tema | Material |
| --- | --- | --- |
| [Laboratorio 1](./Laboratorios/Laboratorio%201/) | Entorno RIOT, Hello World, UART, shell y secuencia de LED temporizados | Código C, Makefile, informe, capturas y [video](https://youtu.be/YAHnWkMRIBE) |
| [Laboratorio 2](./Laboratorios/Laboratorio%202/) | Enlace punto a punto IEEE 802.15.4: distancia, obstáculos, RSSI/LQI y CSMA | TX/RX, configuración, datos crudos, resultados e informe |
| [Laboratorio 3](./Laboratorios/Laboratorio%203/) | Comunicación multisalto SENSOR → RELAY → SINK y control de reenvío con TTL | Tres aplicaciones, informe y evidencias |

**[Explorar los laboratorios](./Laboratorios/)**

## Entorno de desarrollo

- **Hardware:** TelosB con MSP430F1611 y transceptor CC2420.
- **Software:** RIOT OS 2026.04 y compilador MSP430.
- **Herramientas:** Ubuntu en WSL2, GoodFET BSL para grabación y pyterm para monitoreo UART.
- **Temas:** IEEE 802.15.4, tramas MAC, CSMA/CCA, RSSI, LQI, seguimiento de secuencias y reenvío.

Los procedimientos, parámetros, resultados y limitaciones se documentan en el README de cada laboratorio. Las mediciones corresponden a condiciones concretas y no deben extrapolarse como garantías generales de alcance o rendimiento.

---

**Autor:** Aldo Pastore  
**Asignatura:** Telecomunicaciones 2 · Ingeniería Electrónica  
**Institución:** Universidad Católica «Nuestra Señora de la Asunción»
