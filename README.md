# Emulador imsai4Teensy

Emulador IMSAI 8080 a Teensy 4.1 en modo Serial-only.

## Estado actual
- Proyecto PlatformIO para Teensy 4.1.
- BSP de tiempo (micros/delay).
- HAL minima para SIO1A sobre Serial USB.
- Core minimo 8080 enlazado (sim8080/simcore/simglb/imsai-sio2).
- ROM MPU-A embebida y cargada en D800-DFFF.

## Probar build actual
1. Abrir esta carpeta en PlatformIO.
2. Compilar y subir al Teensy 4.1.
3. Abrir monitor serie a 115200.
4. Esperar banner y prompt del monitor IMSAI.
