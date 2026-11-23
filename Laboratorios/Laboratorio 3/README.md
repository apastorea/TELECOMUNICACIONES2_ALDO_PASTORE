# Laboratorio 3 — Comunicación multisalto IEEE 802.15.4

**Plataforma:** Tres nodos TelosB  
**Sistema operativo:** RIOT OS 2026.04

## Descripción

Se desarrolló y verificó una comunicación de dos saltos con tres roles: **SENSOR (ID 1)**, **RELAY (ID 2)** y **SINK (ID 3)**. El repetidor recibe y reenvía tramas destinadas al nodo final. Se examinaron los campos de secuencia, saltos y TTL, junto con indicadores RSSI, LQI y la estimación del porcentaje de recepción de paquetes (PRR).

Se realizaron dos mediciones de recepción con distinta configuración de CCA y una prueba complementaria con `TTL=0`, que permitió comprobar el descarte de mensajes en el RELAY.

Aquí se conservan los proyectos `telosb_sensor`, `telosb_relay` y `telosb_sink`, la documentación y las evidencias originales.

**Autor:** Aldo Pastore — Telecomunicaciones 2.
