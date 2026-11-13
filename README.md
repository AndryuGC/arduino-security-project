# Sistema de control de acceso y seguridad

Proyecto de aplicación 2S2026 — Variante A
Arquitectura del Computador
Facultad de Ingeniería, Universidad Rafael Landívar

**Integrantes**

| Nombre | Carné |
|---|---|
| Andry González Cantoral | 2000924 |
| Rodrigo Guerra González | 1086024 |

---

## Descripción

Terminal de control de acceso construida sobre un Arduino UNO. El sistema identifica a los usuarios con una tarjeta RFID y les pide un PIN de cuatro dígitos antes de autorizar la apertura de la puerta, que se acciona con un servomotor.

Las credenciales se guardan en la memoria EEPROM del microcontrolador, por lo que se conservan aunque el equipo se quede sin energía. Los usuarios se registran y se eliminan desde el mismo teclado matricial, sin tener que volver a cargar el programa.

El estado interno del programa se representa físicamente con ocho LEDs manejados por registros de desplazamiento 74HC595.

---

## Componentes

- Arduino UNO
- Módulo RFID RC522 con tarjetas y llaveros
- Teclado matricial 4x4
- Pantalla LCD 16x2 con módulo I2C
- Servomotor SG90
- Dos registros de desplazamiento 74HC595
- Ocho LEDs con resistencias de 220 Ω
- Buzzer activo
- Protoboard, cables y alimentación independiente

---

## Contenido del repositorio

```
base/              Código fuente del sistema
diagnostics/       Sketch de prueba de componentes
docs/              Manual de usuario, diagrama de bloques y tabla de pines
3d-printing/       Modelos del prototipo físico
```

El manual de usuario está en [docs/manual-usuario.md](docs/manual-usuario.md).

---

## Asignación de pines

| Pin | Conectado a |
|---|---|
| D2 | Buzzer activo |
| D3 | Botón de diagnóstico |
| D4 | Señal del servomotor |
| D5 | Libre |
| D6 | RC522 — RST |
| D7 | 74HC595 — DATA (compartido) |
| D8 | 74HC595 — CLOCK (compartido) |
| D9 | 74HC595 — LATCH (compartido) |
| D10 | RC522 — SDA / SS |
| D11 | RC522 — MOSI |
| D12 | RC522 — MISO |
| D13 | RC522 — SCK |
| A0 a A3 | Columnas del teclado |
| A4 | LCD — SDA |
| A5 | LCD — SCL |

Las cuatro filas del teclado no van directo al Arduino: las maneja el primer 74HC595 por sus salidas Q0 a Q3. El segundo registro, encadenado al primero, controla los ocho LEDs de estado.

---

## Estados del sistema

Cada estado enciende un solo LED, con el valor binario que se envía al registro de desplazamiento.

| Estado | Valor | LED |
|---|---|---|
| Inicio | 00000001 | 1 |
| Esperando RFID | 00000010 | 2 |
| Validando RFID | 00000100 | 3 |
| Esperando PIN | 00001000 | 4 |
| Validando PIN | 00010000 | 5 |
| Autorizado | 00100000 | 6 |
| Denegado | 01000000 | 7 |
| Alarma | 10000000 | 8 |

---

## Cómo cargar el programa

1. Abrir `base/base.ino` en el IDE de Arduino o en VS Code.
2. Instalar las bibliotecas `LiquidCrystal_I2C` y `MFRC522` desde el gestor de bibliotecas.
3. Seleccionar la placa Arduino UNO y el puerto correspondiente.
4. Cargar el programa.
5. Abrir el monitor serial a **9600 baudios**. Ahí se imprime el UID de cada tarjeta leída y la cantidad de usuarios guardados al arrancar.

Las bibliotecas `Wire`, `SPI`, `Servo` y `EEPROM` ya vienen incluidas con el IDE.

La primera vez hay que registrar un usuario con la tecla `A`, porque la memoria arranca vacía. El procedimiento completo está en el manual.
