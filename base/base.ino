/*
  Arquitectura del Computador
  Proyecto: sistema de control de acceso

  Andry Gonzalez Cantoral | 2000924
  Rodrigo Guerra Gonzalez | 1086024
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>


/*
  Pines usados en el Arduino Uno:

  D0, D1   serial
  D2       buzzer
  D3       boton de diagnostico
  D4       servo
  D5       libre
  D6       RST del RFID
  D7-D9    DATA, CLOCK y LATCH de los 74HC595
  D10-D13  SPI del RFID (SS, MOSI, MISO, SCK)
  A0-A3    columnas del keypad
  A4, A5   SDA y SCL del LCD
*/

const byte PIN_BUZZER = 2;
const byte PIN_DIAG   = 3;
const byte PIN_SERVO  = 4;

const byte RFID_RST = 6;

const byte DATA  = 7;
const byte CLOCK = 8;
const byte LATCH = 9;

const byte RFID_SS = 10;

const byte columnas[4] = { A0, A1, A2, A3 };


// Dispositivos

LiquidCrystal_I2C lcd(0x27, 16, 2);

Servo puerta;

MFRC522 rfid(RFID_SS, RFID_RST);

const byte ANGULO_CERRADO = 0;
const byte ANGULO_ABIERTO = 90;


/*
  Los dos 74HC595 van en cadena: Q7' del primero entra al DS
  del segundo y los dos comparten CLOCK y LATCH.

  Primer registro:  bits 0-3 = filas del keypad, bits 4-7 sin usar
  Segundo registro: bits 0-7 = LEDs de estado 1 al 8
*/

// Las filas del keypad arrancan en HIGH
byte registro1 = B00001111;

// LEDs apagados
byte registro2 = B00000000;


// Un bit por LED, segun el estado en que este el sistema

const byte LED_INICIO         = B00000001;   // LED 1
const byte LED_ESPERANDO_RFID = B00000010;   // LED 2
const byte LED_VALIDANDO_RFID = B00000100;   // LED 3
const byte LED_ESPERANDO_PIN  = B00001000;   // LED 4
const byte LED_VALIDANDO_PIN  = B00010000;   // LED 5
const byte LED_AUTORIZADO     = B00100000;   // LED 6
const byte LED_DENEGADO       = B01000000;   // LED 7
const byte LED_ALARMA         = B10000000;   // LED 8


/*
  Teclas con funcion aparte de los digitos:
  *  borrar          #  confirmar
  A  nuevo usuario   B  cerrar puerta
  C  borrar todo     D  cancelar / apagar alarma
*/

char teclas[4][4] = {

  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}

};


// Variables del sistema

// UID que devuelve el RC522
byte uidLeido[4];
byte uidTam = 0;

// PIN que se va escribiendo en el keypad
char pinIngresado[5] = "";
byte posicionPIN = 0;

byte intentosFallidos = 0;


/*
  EEPROM:

  direccion 0  -> cuantos usuarios hay registrados
  direccion 1 en adelante -> 8 bytes por usuario,
  los primeros 4 son el UID y los otros 4 el PIN

  Los UID no van escritos en el programa, se registran desde el
  teclado y quedan guardados aunque se desconecte el Arduino.
*/

const byte MAX_USUARIOS  = 10;
const int  DIR_CANTIDAD  = 0;
const int  DIR_USUARIOS  = 1;
const byte TAM_REGISTRO  = 8;

// Usuario al que pertenece la tarjeta leida, -1 si ninguno
int usuarioActual = -1;


// Estados del sistema

enum EstadoSistema
{
  ESPERANDO_RFID,

  ESPERANDO_PIN,

  VALIDANDO_PIN,

  ACCESO_AUTORIZADO,

  ACCESO_DENEGADO,

  PUERTA_ABIERTA,

  ALARMA
};

EstadoSistema estado = ESPERANDO_RFID;


void actualizarRegistro()
{
  digitalWrite(LATCH, LOW);

  // El primer byte atraviesa la cadena y queda en el segundo
  // 74HC595, que es el de los LEDs
  shiftOut(DATA, CLOCK, MSBFIRST, registro2);

  // El segundo byte se queda en el primero, el del teclado
  shiftOut(DATA, CLOCK, MSBFIRST, registro1);

  digitalWrite(LATCH, HIGH);
}


void mostrarEstadoLED(byte valor)
{
  registro2 = valor;

  actualizarRegistro();
}


void apagarLEDs()
{
  mostrarEstadoLED(0);
}


void seleccionarFila(byte fila)
{
  // Todas las filas en HIGH y solo la seleccionada en LOW
  registro1 |= B00001111;

  bitClear(registro1, fila);

  actualizarRegistro();
}


char leerTeclado()
{
  for (byte fila = 0; fila < 4; fila++)
  {
    seleccionarFila(fila);

    delayMicroseconds(10);

    for (byte columna = 0; columna < 4; columna++)
    {
      if (digitalRead(columnas[columna]) == LOW)
      {
        delay(30);   // anti rebote

        if (digitalRead(columnas[columna]) == LOW)
        {
          char tecla = teclas[fila][columna];

          // No devolver nada hasta que suelten la tecla
          while (digitalRead(columnas[columna]) == LOW)
          {
            delay(5);
          }

          return tecla;
        }
      }
    }
  }

  return 0;
}


// Lectura y escritura en la EEPROM

byte cantidadUsuarios()
{
  byte n = EEPROM.read(DIR_CANTIDAD);

  // Una EEPROM sin usar trae 0xFF, por eso cualquier valor
  // arriba del maximo se cuenta como cero
  if (n > MAX_USUARIOS) return 0;

  return n;
}


int direccionDeUsuario(byte indice)
{
  return DIR_USUARIOS + indice * TAM_REGISTRO;
}


// Agrega el usuario al final de la lista
bool guardarUsuario(byte uid[4], char pin[4])
{
  byte n = cantidadUsuarios();

  if (n >= MAX_USUARIOS) return false;   // ya no cabe

  int base = direccionDeUsuario(n);

  for (byte i = 0; i < 4; i++) EEPROM.update(base + i, uid[i]);
  for (byte i = 0; i < 4; i++) EEPROM.update(base + 4 + i, pin[i]);

  EEPROM.update(DIR_CANTIDAD, n + 1);

  return true;
}


// Poniendo la cantidad en cero ya no se consulta nada, los bytes
// viejos se sobreescriben con el siguiente registro
void borrarUsuarios()
{
  EEPROM.update(DIR_CANTIDAD, 0);
}


// El buzzer es activo, el tono lo genera el mismo, por eso se
// prende y apaga con digitalWrite y no con tone()
void pitar(int duracion)
{
  digitalWrite(PIN_BUZZER, HIGH);
  delay(duracion);
  digitalWrite(PIN_BUZZER, LOW);
}


void sonidoCorrecto()
{
  pitar(80);
  delay(80);
  pitar(80);
}


void sonidoError()
{
  pitar(500);
}


// Devuelve true si hay tarjeta nueva, el UID queda en uidLeido
bool leerTarjeta()
{
  if (!rfid.PICC_IsNewCardPresent()) return false;
  if (!rfid.PICC_ReadCardSerial())   return false;

  uidTam = rfid.uid.size;

  for (byte i = 0; i < 4; i++)
  {
    uidLeido[i] = (i < uidTam) ? rfid.uid.uidByte[i] : 0;
  }

  Serial.print(F("UID: "));
  for (byte i = 0; i < uidTam; i++)
  {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();

  // Detener la tarjeta para que no se lea en bucle
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  return true;
}


bool tarjetaAutorizada()
{
  usuarioActual = -1;

  if (uidTam != 4) return false;   // aqui solo se guardan UID de 4 bytes

  byte n = cantidadUsuarios();

  for (byte u = 0; u < n; u++)
  {
    int base = direccionDeUsuario(u);
    bool igual = true;

    for (byte i = 0; i < 4; i++)
    {
      if (EEPROM.read(base + i) != uidLeido[i])
      {
        igual = false;
        break;
      }
    }

    if (igual)
    {
      usuarioActual = u;
      return true;
    }
  }

  return false;
}


// Captura del PIN

// Asterisco por cada digito escrito y guion por los que faltan
void dibujarPIN()
{
  lcd.setCursor(0, 1);

  for (byte i = 0; i < 4; i++)
  {
    lcd.print(i < posicionPIN ? '*' : '_');
  }
}


void mostrarPantallaPIN()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("INGRESE SU PIN");

  dibujarPIN();
}


bool leerPIN()
{
  char tecla = leerTeclado();

  if (tecla == 0) return false;

  if (tecla >= '0' && tecla <= '9')
  {
    if (posicionPIN < 4)
    {
      pinIngresado[posicionPIN] = tecla;
      posicionPIN++;
      pinIngresado[posicionPIN] = '\0';

      dibujarPIN();
    }
  }
  else if (tecla == '*')            // borrar
  {
    if (posicionPIN > 0)
    {
      posicionPIN--;
      pinIngresado[posicionPIN] = '\0';

      dibujarPIN();
    }
  }
  else if (tecla == '#')            // confirmar
  {
    if (posicionPIN == 4) return true;
  }

  return false;
}


bool validarPIN()
{
  if (usuarioActual < 0) return false;

  // Los 4 bytes del PIN van despues de los 4 del UID
  int base = direccionDeUsuario(usuarioActual) + 4;

  for (byte i = 0; i < 4; i++)
  {
    if (EEPROM.read(base + i) != pinIngresado[i]) return false;
  }

  return true;
}


// El servo solo se conecta mientras se mueve, asi no vibra ni
// consume corriente estando quieto, que importa porque comparte
// la alimentacion con lo demas
void moverPuerta(byte angulo)
{
  puerta.attach(PIN_SERVO);

  puerta.write(angulo);

  delay(600);          // que alcance a llegar a la posicion

  puerta.detach();
}


void abrirPuerta()
{
  moverPuerta(ANGULO_ABIERTO);
}


void cerrarPuerta()
{
  moverPuerta(ANGULO_CERRADO);
}


// Suena intermitente junto con el LED 8 y no para hasta que se
// presione la tecla D
void activarAlarma()
{
  bool encendido = false;

  while (true)
  {
    encendido = !encendido;

    digitalWrite(PIN_BUZZER, encendido ? HIGH : LOW);

    mostrarEstadoLED(encendido ? LED_ALARMA : 0);

    // Se revisa el teclado varias veces dentro de la pausa para
    // que la tecla responda sin esperar al parpadeo
    for (byte i = 0; i < 5; i++)
    {
      if (leerTeclado() == 'D')
      {
        digitalWrite(PIN_BUZZER, LOW);

        apagarLEDs();

        return;
      }

      delay(50);
    }
  }
}


/*
  Registro de usuario nuevo, entra con la tecla A desde la
  pantalla de espera: se lee la tarjeta, se pide un PIN de cuatro
  digitos y se guardan los ocho bytes en la EEPROM. Con D se
  cancela en cualquier parte.
*/

void registrarUsuario()
{
  // Paso 1, leer la tarjeta

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("NUEVO USUARIO");

  lcd.setCursor(0, 1);
  lcd.print("ACERQUE TARJETA");


  byte uidNuevo[4];
  bool tarjetaLista = false;


  while (!tarjetaLista)
  {
    if (leerTeclado() == 'D') return;   // cancelar

    if (leerTarjeta())
    {
      if (uidTam != 4)
      {
        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("UID NO VALIDO");

        sonidoError();

        delay(1500);

        return;
      }

      // No dejar registrar dos veces la misma tarjeta
      if (tarjetaAutorizada())
      {
        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("YA REGISTRADA");

        sonidoError();

        delay(1500);

        return;
      }

      for (byte i = 0; i < 4; i++) uidNuevo[i] = uidLeido[i];

      tarjetaLista = true;
    }
  }


  // Paso 2, pedir el PIN

  posicionPIN = 0;
  pinIngresado[0] = '\0';


  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("NUEVO PIN");

  dibujarPIN();


  bool pinListo = false;


  while (!pinListo)
  {
    char tecla = leerTeclado();

    if (tecla == 0) continue;

    if (tecla == 'D') return;          // cancelar

    if (tecla >= '0' && tecla <= '9')
    {
      if (posicionPIN < 4)
      {
        pinIngresado[posicionPIN] = tecla;
        posicionPIN++;
        pinIngresado[posicionPIN] = '\0';

        dibujarPIN();
      }
    }
    else if (tecla == '*')             // borrar
    {
      if (posicionPIN > 0)
      {
        posicionPIN--;
        pinIngresado[posicionPIN] = '\0';

        dibujarPIN();
      }
    }
    else if (tecla == '#')             // confirmar
    {
      if (posicionPIN == 4) pinListo = true;
    }
  }


  // Paso 3, guardar

  lcd.clear();

  if (guardarUsuario(uidNuevo, pinIngresado))
  {
    lcd.setCursor(0, 0);
    lcd.print("USUARIO");

    lcd.setCursor(0, 1);
    lcd.print("REGISTRADO");

    sonidoCorrecto();
  }
  else
  {
    lcd.setCursor(0, 0);
    lcd.print("MEMORIA LLENA");

    sonidoError();
  }

  delay(1500);
}


// Entra con la tecla C desde la espera y pide confirmar con #
void borrarTodo()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("BORRAR TODO?");

  lcd.setCursor(0, 1);
  lcd.print("# = SI  D = NO");


  while (true)
  {
    char tecla = leerTeclado();

    if (tecla == '#')
    {
      borrarUsuarios();

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("MEMORIA");

      lcd.setCursor(0, 1);
      lcd.print("BORRADA");

      sonidoCorrecto();

      delay(1500);

      return;
    }

    if (tecla == 'D') return;          // cancelar
  }
}


// Deja todo listo para empezar de nuevo la autenticacion
void reiniciarSistema()
{
  uidTam = 0;

  usuarioActual = -1;

  posicionPIN = 0;

  pinIngresado[0] = '\0';


  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("CONTROL ACCESO");

  lcd.setCursor(0, 1);
  lcd.print("PRESENTE TARJETA");


  mostrarEstadoLED(LED_ESPERANDO_RFID);

  estado = ESPERANDO_RFID;
}


void setup()
{
  Serial.begin(9600);


  // LCD

  lcd.init();

  lcd.backlight();


  // 74HC595

  pinMode(DATA, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(LATCH, OUTPUT);

  actualizarRegistro();


  // Columnas del keypad

  for (byte i = 0; i < 4; i++)
  {
    pinMode(columnas[i], INPUT_PULLUP);
  }


  // Buzzer

  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(PIN_BUZZER, LOW);


  // Boton de diagnostico

  pinMode(PIN_DIAG, INPUT_PULLUP);


  // La puerta arranca cerrada

  cerrarPuerta();


  // RFID

  SPI.begin();

  rfid.PCD_Init();


  // Pantalla de arranque

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("SISTEMA ACCESO");

  lcd.setCursor(0, 1);
  lcd.print("INICIANDO...");

  mostrarEstadoLED(LED_INICIO);

  Serial.print(F("Usuarios en EEPROM: "));
  Serial.println(cantidadUsuarios());

  delay(1500);

  reiniciarSistema();
}


void loop()
{
  // Unidad de control: la variable "estado" dice en que parte
  // del proceso va el sistema

  switch (estado)
  {

    // Estado 0, esperando RFID

    case ESPERANDO_RFID:
    {
      // En la espera tambien sirven las teclas de administracion
      char tecla = leerTeclado();

      if (tecla == 'A')
      {
        registrarUsuario();

        reiniciarSistema();

        break;
      }

      if (tecla == 'C')
      {
        borrarTodo();

        reiniciarSistema();

        break;
      }


      if (leerTarjeta())
      {
        mostrarEstadoLED(LED_VALIDANDO_RFID);

        delay(300);   // para alcanzar a ver el LED 3

        if (tarjetaAutorizada())
        {
          intentosFallidos = 0;      // empieza una autenticacion nueva

          mostrarPantallaPIN();

          mostrarEstadoLED(LED_ESPERANDO_PIN);

          estado = ESPERANDO_PIN;
        }
        else
        {
          estado = ACCESO_DENEGADO;
        }
      }

      break;
    }


    // Estado 1, esperando PIN

    case ESPERANDO_PIN:

      if (leerPIN())
      {
        estado = VALIDANDO_PIN;
      }

      break;


    // Estado 2, validando PIN

    case VALIDANDO_PIN:

      mostrarEstadoLED(LED_VALIDANDO_PIN);

      delay(300);   // para alcanzar a ver el LED 5

      if (validarPIN())
      {
        estado = ACCESO_AUTORIZADO;
      }
      else
      {
        intentosFallidos++;

        if (intentosFallidos >= 3)
        {
          // En el tercer intento pasa a denegado y de ahi a la alarma
          estado = ACCESO_DENEGADO;
        }
        else
        {
          // Primer o segundo intento, se deja reintentar
          lcd.clear();

          lcd.setCursor(0, 0);
          lcd.print("PIN INCORRECTO");

          lcd.setCursor(0, 1);
          lcd.print("INTENTO ");
          lcd.print(intentosFallidos);
          lcd.print("/3");

          mostrarEstadoLED(LED_DENEGADO);

          sonidoError();

          delay(1500);

          posicionPIN = 0;
          pinIngresado[0] = '\0';

          mostrarPantallaPIN();

          mostrarEstadoLED(LED_ESPERANDO_PIN);

          estado = ESPERANDO_PIN;
        }
      }

      break;


    // Estado 3, acceso autorizado

    case ACCESO_AUTORIZADO:

      // "ACCESO AUTORIZADO" son 17 caracteres y la linea es de
      // 16, por eso va partido en dos

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("ACCESO");

      lcd.setCursor(0, 1);
      lcd.print("AUTORIZADO");


      mostrarEstadoLED(LED_AUTORIZADO);

      sonidoCorrecto();

      abrirPuerta();


      estado = PUERTA_ABIERTA;

      break;


    // Estado 4, acceso denegado

    case ACCESO_DENEGADO:

      mostrarEstadoLED(LED_DENEGADO);

      if (usuarioActual < 0)
      {
        // La tarjeta no esta registrada

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("TARJETA INVALIDA");

        lcd.setCursor(0, 1);
        lcd.print("ACCESO DENEGADO");

        sonidoError();

        delay(1500);

        apagarLEDs();

        reiniciarSistema();
      }
      else
      {
        // Fallo el tercer intento de PIN

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("ACCESO DENEGADO");

        sonidoError();

        delay(1500);

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("ACCESO BLOQUEADO");

        lcd.setCursor(0, 1);
        lcd.print("ALARMA ACTIVADA");

        estado = ALARMA;
      }

      break;


    // Estado 5, puerta abierta
    // Se queda abierta hasta que presionen B, como pide el enunciado

    case PUERTA_ABIERTA:

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("PUERTA ABIERTA");

      lcd.setCursor(0, 1);
      lcd.print("B = CERRAR");


      while (leerTeclado() != 'B')
      {
        // esperando la tecla de cierre
      }


      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("CERRANDO...");

      cerrarPuerta();

      apagarLEDs();

      intentosFallidos = 0;


      reiniciarSistema();

      break;


    // Estado 6, alarma
    // activarAlarma() no regresa hasta que presionen D

    case ALARMA:

      activarAlarma();

      intentosFallidos = 0;

      reiniciarSistema();

      break;
  }
}
