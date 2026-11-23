# Laboratorio 1 — Instalación, Hello World y LED temporizados

**Objetivo:** preparar RIOT OS para TelosB y verificar compilación, grabación, UART, shell y temporización de los tres LED.

**Plataforma:** TelosB / MSP430F1611 · **RIOT:** 2026.04 · **UART:** 9600 bit/s.

## Implementación

Se verificó primero el ejemplo oficial **Hello World** y luego se construyó la aplicación propia **telosb_hello_led**. Su código utiliza `ztimer_periodic` para seleccionar los indicadores **LED0 → LED1 → LED2** a intervalos nominales de **500 ms**. Los tres LED parten apagados y LED0 se activa en el primer vencimiento del temporizador. El ciclo completo nominal es de **1,5 s**.

La shell permite ejecutar `help` e `info` sin detener la secuencia luminosa.

## Resultados observados

| Comprobación | Evidencia |
| --- | --- |
| Compilación para `BOARD=telosb` | Correcta |
| Grabación mediante GoodFET BSL | 16.502 bytes programados; reinicio correcto |
| Tamaños del ejecutable | `text=15364`, `data=1138`, `bss=1036` bytes |
| Memoria estimada | Flash: 16.502 B; RAM estática: 2.174 B |
| Consola serie | Mensajes de RIOT y respuestas válidas de `help` e `info` |
| Conmutación LED | Demostrada en el video anexo |

La duración de 500 ms es la programada en el firmware, no una medida instrumental independiente.

## Archivos del laboratorio

- [Código de la aplicación](./src/main.c) y [Makefile](./src/Makefile).
- [Evidencias de compilación, programación y UART](./Evidencia/).
- [Informe técnico](./Informe_Lab1_Telecomunicaciones2_Aldo_Pastore.pdf).
- [Guía original](./%5BWSN-RIOT%5D%20Lab1_HelloWorldLED.pdf).

## Video de demostración

**[Ver secuencia de LED en YouTube](https://youtu.be/YAHnWkMRIBE)**. Por medio del video anexo se observa la conmutación sucesiva de los tres LED de la placa TelosB.

## Compilación

Desde Ubuntu, con RIOT instalado en `$HOME/RIOT`, ingresar en `Laboratorios/Laboratorio 1/src` y ejecutar:

```bash
make BOARD=telosb clean all
```

Para programar la placa y abrir la terminal se utiliza el puerto correspondiente a su conexión USB, por ejemplo `PORT=/dev/ttyUSB0`, después de verificarlo.

---

**Autor:** Aldo Pastore — Telecomunicaciones 2 · Ingeniería Electrónica
