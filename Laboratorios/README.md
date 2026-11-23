# Laboratorios — Telecomunicaciones 2

Esta carpeta reúne prácticas desarrolladas sobre **TelosB** y **RIOT OS**, con código, archivos de configuración, informes y evidencias de ejecución.

| Práctica | Objetivo principal | Acceso |
| --- | --- | --- |
| **1 — Puesta en marcha** | Compilar, programar, ejecutar UART y shell, y conmutar LED0/LED1/LED2 cada 500 ms nominales. | [Laboratorio 1](./Laboratorio%201/) · [Video](https://youtu.be/YAHnWkMRIBE) |
| **2 — Enlace IEEE 802.15.4** | Implementar TX/RX y observar la recepción en distintos montajes, con y sin CSMA. | [Laboratorio 2](./Laboratorio%202/) · [Resultados](./Laboratorio%202/resultados/Resultados_Laboratorio_2.xlsx) |
| **3 — Red multisalto** | Comunicar SENSOR, RELAY y SINK, analizar campos de trama, RSSI/LQI, PRR y TTL. | [Laboratorio 3](./Laboratorio%203/) · [Informe](./Laboratorio%203/Informe_Lab3_AldoPastore.pdf) |

Cada laboratorio conserva la estructura de sus archivos originales. Los README individuales incluyen el montaje, los resultados registrados y enlaces a los materiales principales.

**Reproducibilidad:** los proyectos se realizaron con RIOT OS 2026.04. Para compilarlos fuera de su carpeta de trabajo original hay que revisar la variable `RIOTBASE` y, cuando corresponda, las modificaciones locales al sistema RIOT.

**Autor:** Aldo Pastore — Telecomunicaciones 2, Ingeniería Electrónica  
**Institución:** Universidad Católica «Nuestra Señora de la Asunción», Paraguay
