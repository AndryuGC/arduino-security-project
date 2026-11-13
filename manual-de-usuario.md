# Manual de usuario
## Sistema de control de acceso y seguridad

Arquitectura del Computador — Universidad Rafael Landívar

---

## 1. Qué hace este sistema

Es una terminal de control de acceso en miniatura. Para abrir la puerta hay que pasar dos filtros: primero una tarjeta RFID registrada, y después el PIN de cuatro dígitos que le corresponde a esa tarjeta. Si alguno de los dos falla, el acceso se deniega.

El sistema guarda los usuarios en la memoria EEPROM del Arduino, así que los datos no se pierden cuando se desconecta. También se pueden registrar o borrar usuarios desde el mismo teclado, sin necesidad de volver a cargar el programa.

Ocho LEDs muestran en qué estado está el programa en cada momento, y una pantalla LCD indica qué hacer en cada paso.

---

## 2. Qué trae el equipo

| Componente | Para qué sirve |
|---|---|
| Arduino UNO | Procesa todo el sistema |
| Lector RFID RC522 | Lee las tarjetas y llaveros |
| Teclado matricial 4x4 | Ingresar el PIN y los comandos |
| Pantalla LCD 16x2 | Muestra los mensajes |
| Servomotor SG90 | Abre y cierra la puerta |
| Dos registros 74HC595 | Manejan el teclado y los ocho LEDs |
| Ocho LEDs | Indican el estado del programa |
| Buzzer activo | Avisos sonoros y alarma |

---

## 3. Encendido

Conecte la alimentación del Arduino, ya sea con la batería de 9V o con un power bank por USB.

Al encender, la pantalla muestra:

```
SISTEMA ACCESO
INICIANDO...
```

Después de un segundo y medio pasa a la pantalla principal:

```
CONTROL ACCESO
PRESENTE TARJETA
```

Desde ahí el sistema queda esperando. La puerta se cierra sola al arrancar, para que el servomotor quede en la misma posición que la puerta física.

**Si es la primera vez que se usa**, la memoria está vacía y ninguna tarjeta va a funcionar. Hay que registrar al menos un usuario antes (sección 5).

---

## 4. Las teclas

El teclado usa esta distribución:

|  | Col 1 | Col 2 | Col 3 | Col 4 |
|---|---|---|---|---|
| **Fila 1** | 1 | 2 | 3 | A |
| **Fila 2** | 4 | 5 | 6 | B |
| **Fila 3** | 7 | 8 | 9 | C |
| **Fila 4** | * | 0 | # | D |

Los números sirven para el PIN. Las otras cuatro teclas tienen una función fija:

| Tecla | Qué hace |
|---|---|
| `*` | Borra el último dígito que escribió |
| `#` | Confirma el PIN o una operación |
| `A` | Registra un usuario nuevo |
| `B` | Cierra la puerta cuando está abierta |
| `C` | Borra todos los usuarios de la memoria |
| `D` | Cancela una operación o apaga la alarma |

---

## 5. Registrar un usuario

Desde la pantalla principal, presione `A`. La pantalla pide la tarjeta:

```
NUEVO USUARIO
ACERQUE TARJETA
```

Acerque la tarjeta o el llavero que quiere dar de alta. Si esa tarjeta ya estaba registrada, aparece `YA REGISTRADA` y la operación se cancela.

Cuando la tarjeta se lee bien, la pantalla pide el PIN:

```
NUEVO PIN
____
```

Escriba cuatro dígitos. Cada uno aparece como asterisco. Si se equivoca, `*` borra el último. Cuando tenga los cuatro, presione `#` para guardar.

```
USUARIO
REGISTRADO
```

El sistema regresa solo a la pantalla principal y la tarjeta ya queda habilitada.

En cualquier momento del proceso, `D` cancela y no guarda nada.

El sistema admite hasta diez usuarios. Si la memoria se llena, la pantalla avisa con `MEMORIA LLENA`.

---

## 6. Abrir la puerta

Este es el uso normal, el de todos los días.

**Paso 1.** Acerque su tarjeta al lector. Si está registrada:

```
INGRESE SU PIN
____
```

Si no lo está:

```
TARJETA INVALIDA
ACCESO DENEGADO
```

y después de un momento vuelve a la pantalla principal.

**Paso 2.** Escriba su PIN de cuatro dígitos. Los asteriscos van apareciendo conforme teclea. Con `*` borra si se equivocó.

**Paso 3.** Presione `#` para confirmar.

Si el PIN es correcto, suenan dos pitidos cortos y la puerta se abre:

```
ACCESO
AUTORIZADO
```

**Paso 4.** La puerta se queda abierta todo el tiempo que haga falta. Cuando termine, presione `B` para cerrarla. El sistema regresa a la pantalla principal y queda listo para el siguiente usuario.

---

## 7. Cuando el PIN está mal

El sistema da tres oportunidades por cada tarjeta presentada.

**Primer y segundo intento.** Suena un pitido largo y la pantalla avisa:

```
PIN INCORRECTO
INTENTO 1/3
```

Después de un momento vuelve a pedir el PIN, con los dígitos ya borrados.

**Tercer intento.** Se acabaron las oportunidades:

```
ACCESO DENEGADO
```

y enseguida:

```
ACCESO BLOQUEADO
ALARMA ACTIVADA
```

La alarma suena de forma intermitente y el LED 8 parpadea al mismo ritmo. No se apaga sola ni tiene límite de tiempo.

**Para apagarla, presione `D`.** El sistema regresa a la pantalla principal con los intentos en cero.

El contador también se reinicia cada vez que se presenta una tarjeta nueva, así que un error de un usuario no le resta intentos al siguiente.

---

## 8. Borrar todos los usuarios

Presione `C` desde la pantalla principal. El sistema pide confirmación:

```
BORRAR TODO?
# = SI  D = NO
```

Con `#` se borran todos los usuarios registrados. Con `D` se cancela y no pasa nada.

Después del borrado la memoria queda vacía, así que ninguna tarjeta va a funcionar hasta registrar una de nuevo.

Esta operación no se puede deshacer.

---

## 9. Los ocho LEDs

Cada LED corresponde a un estado del programa, y solo hay uno encendido a la vez. Sirven para ver por dónde va el sistema sin mirar la pantalla.

| LED | Estado | Cuándo se enciende |
|---|---|---|
| 1 | Inicio | Mientras el sistema arranca |
| 2 | Esperando RFID | En la pantalla principal |
| 3 | Validando RFID | Al leer una tarjeta |
| 4 | Esperando PIN | Mientras escribe el PIN |
| 5 | Validando PIN | Al confirmar con `#` |
| 6 | Autorizado | Acceso concedido |
| 7 | Denegado | Tarjeta inválida o PIN incorrecto |
| 8 | Alarma | Parpadea con la alarma activa |

---

## 10. Los sonidos

| Sonido | Qué significa |
|---|---|
| Dos pitidos cortos | Operación correcta: acceso autorizado, usuario registrado, memoria borrada |
| Un pitido largo | Error: tarjeta inválida, PIN incorrecto, memoria llena |
| Pitidos intermitentes sin parar | Alarma por tres intentos fallidos |

---

## 11. Si algo no funciona

**La pantalla está encendida pero no se ve nada.**
Ajuste el potenciómetro de contraste que está en la parte de atrás del módulo de la pantalla.

**La tarjeta no se detecta.**
Acérquela más al lector, casi tocándolo, y manténgala quieta un segundo. Si acaba de usarla, sepárela unos segundos antes de volver a intentar: el lector solo detecta tarjetas que se presentan de nuevo.

**Una tarjeta registrada aparece como inválida.**
Lo más probable es que la memoria se haya borrado con la tecla `C`. Vuelva a registrarla con `A`.

**Las teclas responden con otro número.**
Es un problema del orden de los cables del teclado, no del programa. Hay que revisar las conexiones de filas y columnas.

**La pantalla parpadea o el sistema se reinicia cuando abre la puerta.**
El servomotor consume bastante corriente al arrancar. Conviene alimentarlo con una fuente independiente de 4.8 a 6 voltios, conectando el negativo con el del Arduino.

**No suena el buzzer.**
Revise que no tenga pegada la etiqueta protectora de fábrica sobre el agujero de salida.

---

## 12. Cosas que conviene saber

- Los usuarios se conservan aunque el equipo se quede sin energía. Puede desconectarlo y al volver a encenderlo las tarjetas siguen funcionando.
- La puerta nunca se cierra sola. Siempre hay que presionar `B`.
- La alarma no tiene temporizador. Solo se apaga con `D`.
- El PIN siempre es de cuatro dígitos, ni más ni menos.
- Cada tarjeta tiene su propio PIN. Dos usuarios pueden tener el mismo PIN sin que se confundan, porque el sistema primero identifica la tarjeta.

---

<!-- Espacio opcional para una foto del prototipo armado -->
