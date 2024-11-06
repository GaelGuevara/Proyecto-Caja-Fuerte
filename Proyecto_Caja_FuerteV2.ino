// Librerias

#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

//Pines para el motor, led infrarrojo y reconocimiento facial

const uint8_t motorPin1 = 2;
const uint8_t motorPin2 = 4;

const uint8_t flash = 16;
const uint8_t facialCheck = 17;

const uint8_t led = 5;

// Constantes para el trackeo de las teclas

const byte FILAS = 4;
const byte COLUMNAS = 3;
char keys[FILAS][COLUMNAS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};

byte pinesFilas[FILAS] = { 33, 25, 26, 27 };
byte pinesColumnas[COLUMNAS] = { 14, 12, 13 };

// Funciones especiales

Keypad teclado = Keypad(makeKeymap(keys), pinesFilas, pinesColumnas, FILAS, COLUMNAS);
LiquidCrystal_I2C lcd(0x27, 16, 2);
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);


// Funciones secundarias

void abrirPuerta();
void cerrarPuerta();
void menu();
void fn_menu(int pos, String menus[], byte sizemenu);
void lcdInit();
void resetVector(unsigned int posicion);
void NFCs(char estado, int pos);

int faceRec();

bool fn_refresh(int sizemenu);
bool claveNFC();

//--------------------------MENU-------------------------------------------

String tags[] = { "0", "0" };
int sizetags = sizeof(tags) / sizeof(tags[0]);

//-----------------------MINI-MENU-----------------------------------------

String miniMenu[] = { "Clave", "Menu" };
int size_miniMenu = sizeof(miniMenu) / sizeof(miniMenu[0]);

String miniMenu2[] = { "Clave", "Llave NFC", "Menu" };
int size_miniMenu2 = sizeof(miniMenu2) / sizeof(miniMenu2[0]);

//--------------------------MENU-------------------------------------------
String menu1[] = { "Cambiar clave", "Rec. Facial", "NFC" };
int sizemenu1 = sizeof(menu1) / sizeof(menu1[0]);

String menu2[] = { "Cambiar rostro", "Agregar Rostro", "s" };
int sizemenu2 = sizeof(menu2) / sizeof(menu2[0]);

String menu3[] = { "Agregar NFC", "Eliminar NFC", "Ident. NFC" };
int sizemenu3 = sizeof(menu3) / sizeof(menu3[0]);

String menu4[] = { "LLave 1", "Llave 2" };
int sizemenu4 = sizeof(menu4) / sizeof(menu4[0]);

String linea1, linea2;

//---------------------------------------------------------------------------

int indiceLCD = 9, indiceMenu = 0, contador = 0, aux = 0;

const int intervalo = 1500;

char TECLA, pass[16], ingresaPass[16], codigo[4] = { '0', '0', '0', '0' };

bool claveGuardada = false, clave = false, codigoPuesto = false, nfcPuesto = false, nfcTemporal = true, reset = true, llaveEncontrada = false;
bool hashPresionado = false, starPresionado = false, hsPresionados = false, salir = false, cambio = false;

bool strComp(bool cod);

unsigned long hTimeInit = 0, sTimeInit = 0, tiempoAnt = 0;

void setup() {
  Serial.begin(115200);
  pinMode(led,         INPUT_PULLDOWN);
  pinMode(facialCheck, INPUT_PULLDOWN);
  pinMode(flash,     OUTPUT);
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  nfc.begin();
  lcdInit();
}

void loop() {
inicio:
  if (!digitalRead(led) || cambio) {
    lcd.noBlink();
    if (!digitalRead(led)) {
      cambio = true;
      delay(75);
      if (!digitalRead(led)) {
        Serial.println("Puerta ABIERTA");
        lcd.clear();
        lcd.setCursor(5, 0);
        lcd.print("PUERTA");
        lcd.setCursor(4, 1);
        lcd.print("ABIERTA");
        while (!digitalRead(led))
          ;
      }
    }
    if (cambio) {
      delay(75);
      int cont = 3;
      cambio = false;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CERRANDO PUERTA");
      Serial.println("Cerrando Puerta en:");
      tiempoAnt = millis();
      while (digitalRead(led)) {
        if (millis() - tiempoAnt >= 1000) {
          tiempoAnt = millis();
          lcd.setCursor(7, 1);
          lcd.print(cont);
          Serial.println(cont);
          cont--;
        }
        if (cont == -1) {
          lcd.clear();
          lcd.setCursor(5, 0);
          lcd.print("PUERTA");
          lcd.setCursor(4, 1);
          lcd.print("CERRADA");
          delay(intervalo);
          cerrarPuerta();
          lcd.clear();
          Serial.println("Puerta CERRADA");
          break;
        }
      }
    }
    goto inicio;
  }
  if (reset) {
    lcd.clear();
    if (!codigoPuesto) {
      lcd.setCursor(1, 0);
      lcd.print("Digitar codigo");
      lcd.setCursor(0, 1);
      lcd.print("para nueva clave");
      teclado.waitForKey();
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Codigo: ");
    } else {
      if (!claveGuardada) {
        lcd.setCursor(1, 0);
        lcd.print("Ingresar nueva");
        lcd.setCursor(5, 1);
        lcd.print("Clave");
      } else {
        (tags[0] != "0" || tags[1] != "0") ? nfcPuesto = true : nfcPuesto;
        (tags[0] == "0" && tags[1] == "0") ? nfcPuesto = false : nfcPuesto;
        Serial.println(nfcPuesto ? "puesto" : "no puesto");
        contador = 0;
        if (nfcPuesto) fn_menu(contador, miniMenu2, size_miniMenu2);
        else fn_menu(contador, miniMenu, size_miniMenu);
        while (1) {
          if (nfcPuesto) {
            if (fn_refresh(size_miniMenu2)) {
              fn_menu(contador, miniMenu2, size_miniMenu2);
            }

            if (starPresionado) {
              starPresionado = false;
              if (!contador) clave = true;
              else if (contador == 1) {
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("ESPERANDO ...");
                if (claveNFC()) {
                  lcd.setCursor(3, 0);
                  lcd.print("AUTORIZADO");
                  delay(intervalo);
                  /*
                  if (faceRec() == 1) {
                    lcd.setCursor(5, 0);
                    lcd.print("CARA");
                    lcd.setCursor(2, 0);
                    lcd.print("RECONOCIDA");
                    delay(intervalo);
                    */
                    lcd.clear();
                    lcd.setCursor(5, 0);
                    lcd.print("PUERTA");
                    lcd.setCursor(4, 1);
                    lcd.print("ABIERTA");
                    abrirPuerta();
                    tiempoAnt = millis();
                    while (digitalRead(led)) {
                      if (millis() - tiempoAnt >= 5000) {
                        cambio = true;
                        break;
                      }
                    }
                  /*
                  } else if (faceRec() == 0) {
                    lcd.setCursor(0, 0);
                    lcd.print("NO SE RECONOCIO ");
                    lcd.setCursor(4, 0);
                    lcd.print("CARA");
                    delay(intervalo);
                  }
                  */
                } else {
                  lcd.setCursor(2, 0);
                  lcd.print("NO AUTORIZADO");
                  delay(intervalo);
                }
                goto inicio;
              } else if (contador == 2) {
                hashPresionado = false;
                menu();
                goto inicio;
              }
            }
          } else {
            if (fn_refresh(size_miniMenu)) {
              fn_menu(contador, miniMenu, size_miniMenu);
            }
            if (starPresionado) {
              starPresionado = false;
              if (!contador) clave = true;
              else {
                hashPresionado = false;
                menu();
                goto inicio;
              }
            }
          }
          if (clave) {
            lcd.clear();
            lcd.setCursor(4, 0);
            lcd.print("Ingresar");
            lcd.setCursor(5, 1);
            lcd.print("Clave");
            clave = false;
            break;
          }
        }
      }
      hashPresionado = false;
      teclado.waitForKey();
      lcd.clear();
    }
    reset = false;
  }

  lcd.blink();
  if (teclado.getKeys()) {
    for (int i = 0; i < LIST_MAX; i++) {
      if (teclado.key[i].stateChanged) {
        TECLA = teclado.key[i].kchar;
        if (teclado.key[i].kstate == PRESSED) {
          if (TECLA != '#' && TECLA != '*') {
            (claveGuardada || !codigoPuesto) ? (ingresaPass[indiceLCD] = TECLA) : (pass[indiceLCD] = TECLA);
            lcd.setCursor(indiceLCD, 0);
            lcd.print(TECLA);
            if (!codigoPuesto) {
              indiceLCD++;
              if (indiceLCD > 12) {
                indiceLCD = 16;
                lcd.setCursor(indiceLCD, 0);
              } else {
                lcd.blink();
              }
            } else {
              (indiceLCD >= 16) ? indiceLCD = 16 : indiceLCD++;
            }
            Serial.println(indiceLCD);
          } else if (TECLA == '#') {
            hashPresionado = true;
            hTimeInit = millis();
          } else if (TECLA == '*') {
            starPresionado = true;
            sTimeInit = millis();
          }
          if (hashPresionado && starPresionado) {
            hTimeInit = 0;
            hsPresionados = true;
          } else {
            hsPresionados = false;
          }
        } else if (teclado.key[i].kstate == RELEASED) {
          if (TECLA == '#') {
            if ((millis() - hTimeInit) < 2000) {
              if (!codigoPuesto) {
                (indiceLCD == 16) ? indiceLCD = 12 : (indiceLCD <= 9) ? indiceLCD = 9
                                                : indiceLCD--;
              } else {
                (indiceLCD <= 0) ? indiceLCD = 0 : indiceLCD--;
              }
              Serial.println(indiceLCD);
              lcd.setCursor(indiceLCD, 0);
              lcd.print(" ");
              lcd.setCursor(indiceLCD, 0);
            }
            hashPresionado = false;
            hTimeInit = 0;
          } else if (TECLA == '*') {
            starPresionado = false;
          }
        }
      }
    }
  }

  if (!hashPresionado && starPresionado && !hsPresionados) {
    if ((millis() - sTimeInit) >= 100) {
      starPresionado = false;
      lcd.noCursor();
      lcd.noBlink();
      lcd.clear();
      if (!codigoPuesto) {
        resetVector(ingresaPass, indiceLCD);
        if (strComp(0)) {
          lcd.setCursor(5, 0);
          lcd.print("Codigo");
          lcd.setCursor(4, 1);
          lcd.print("Correcto");
          codigoPuesto = true;
        } else {
          lcd.setCursor(5, 0);
          lcd.print("Codigo");
          lcd.setCursor(3, 1);
          lcd.print("Incorrecto");
        }
      } else {
        if (indiceLCD < 4) {
          lcd.setCursor(5, 0);
          lcd.print("ERROR!");
          lcd.setCursor(0, 1);
          lcd.print("4 digitos minimo");
        } else {
          !claveGuardada ? resetVector(pass, indiceLCD) : (void)0;
          resetVector(ingresaPass, indiceLCD);
          if (claveGuardada) {
            if (strComp(1)) {
              lcd.setCursor(5, 0);
              lcd.print("Clave");
              lcd.setCursor(4, 1);
              lcd.print("Correcta");
              delay(intervalo);
              /*
              if (faceRec() == 1) {
                lcd.setCursor(5, 0);
                lcd.print("CARA");
                lcd.setCursor(2, 0);
                lcd.print("RECONOCIDA");
                delay(intervalo);
             */
                lcd.clear();
                lcd.setCursor(5, 0);
                lcd.print("PUERTA");
                lcd.setCursor(4, 1);
                lcd.print("ABIERTA");
                abrirPuerta();
                tiempoAnt= millis();
                while (digitalRead(led)) {
                  if (millis() - tiempoAnt >= 5000) {
                    cambio = true;
                    break;
                  }
                }
              /*
              } else if (faceRec == 0) {
                lcd.setCursor(0, 0);
                lcd.print("NO SE RECONOCIO ");
                lcd.setCursor(4, 0);
                lcd.print("CARA");
                delay(intervalo);
              }
              */
              goto final;
            } else {
              lcd.setCursor(5, 0);
              lcd.print("Clave");
              lcd.setCursor(3, 1);
              lcd.print("Incorrecta");
            }
          } else {
            lcd.setCursor(5, 0);
            lcd.print("Clave");
            lcd.setCursor(4, 1);
            lcd.print("Guardada");
            claveGuardada = true;
          }
        }
      }
final:
      for (int i = 0; i < 16; i++) {
        Serial.print(pass[i]);
      }
      Serial.println();
      for (int i = 0; i < 4; i++) {
        Serial.print(codigo[i]);
      }
      Serial.println();
      for (int i = 0; i < 16; i++) {
        Serial.print(ingresaPass[i]);
      }
      Serial.println();
      resetVector(ingresaPass, 0);
      !codigoPuesto ? indiceLCD = 9 : indiceLCD = 0;
      delay(intervalo);
      lcd.clear();
      reset = true;
    }
  }

  if (hashPresionado && !starPresionado && !hsPresionados && codigoPuesto) {
    if ((millis() - hTimeInit) >= 1500 && hTimeInit > 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(0, 0);
      indiceLCD = 0;
      hashPresionado = false;
    }
  }
}


bool claveNFC() {
  if (nfc.tagPresent()) {
    lcd.clear();
    NfcTag tag = nfc.read();
    Serial.println("Tag id: " + tag.getUidString());
    for (int i = 0; i < sizetags; i++) {
      if (tag.getUidString() == tags[i]) {
        return 1;
      }
    }
    return 0;
  }
}

void resetVector(char vector[], unsigned int posicion) {
  if (posicion != 15) {
    for (int i = posicion; i < 16; i++) {
      vector[i] = '-';
    }
  }
}

void menu() {
  contador = 0;
  fn_menu(contador, menu1, sizemenu1);
  while (!salir) {
    if (indiceMenu == 0) {
      if (fn_refresh(sizemenu1)) {
        fn_menu(contador, menu1, sizemenu1);
      }

      if (starPresionado) {
        switch (contador) {
          case 0:
            claveGuardada = false;
            lcd.clear();
            lcd.home();
            salir = true;
            break;
          case 1:
            indiceMenu = 1;
            contador = 0;
            fn_menu(contador, menu2, sizemenu2);
            break;
          case 2:
            indiceMenu = 2;
            contador = 0;
            fn_menu(contador, menu3, sizemenu3);
            break;
        }
        starPresionado = false;
      } else if (hashPresionado) {
        hashPresionado = false;
        reset = true;
        salir = true;
      }
    }

    if (indiceMenu == 1) {
      if (fn_refresh(sizemenu2)) {
        fn_menu(contador, menu2, sizemenu2);
      }
      if (starPresionado) {
        // FACE RECOGNITION
      }
    }

    if (indiceMenu == 2) {
      if (fn_refresh(sizemenu3)) {
        fn_menu(contador, menu3, sizemenu3);
      }

      if (starPresionado) {
        switch (contador) {
          case 0:
            if (tags[0] == "0") NFCs('A', 0);
            else if (tags[1] == "0") NFCs('A', 1);
            else {
              lcd.clear();
              lcd.setCursor(2, 0);
              lcd.print("No se pueden");
              lcd.setCursor(0, 1);
              lcd.print("mas de 2 llaves");
              delay(intervalo);
              contador = 0;
              fn_menu(contador, menu3, sizemenu3);
            }
            break;
          case 1:
            indiceMenu = 3;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(">");
            break;
          case 2:
            NFCs('I', 0);
            break;
        }
        contador = 0;
        starPresionado = false;
      }

      if (hashPresionado) {
        hashPresionado = false;
        indiceMenu = 0;
        contador = 0;
        fn_menu(contador, menu1, sizemenu1);
      }
    }

    if (indiceMenu == 3) {
      ((tags[0] != "0" && tags[1] == "0") || (tags[0] == "0" && tags[1] != "0")) ? aux = 1 : aux = 2;
      if (fn_refresh(aux)) {
        lcd.setCursor(0, 0);
        lcd.print(contador == 0 ? ">" : " ");
        lcd.setCursor(0, 1);
        lcd.print(contador == 1 ? ">" : " ");
      }

      if (nfcTemporal) {
        for (int i = 0; i < sizetags; i++) {
          if (tags[i] != "0") {
            lcd.setCursor(2, i);
            lcd.print("Llave ");
            lcd.print(i + 1);
            llaveEncontrada = true;
          }
        }
        if (tags[0] == "0" && tags[1] != "0") {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("> Llave 2");
        }
        nfcTemporal = false;
      }

      if (!llaveEncontrada) {
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("No hay llaves");
        delay(intervalo);
        indiceMenu = 2;
        fn_menu(contador, menu3, sizemenu3);
      }

      if (starPresionado) {
        starPresionado = false;
        nfcTemporal = true;
        llaveEncontrada = false;
        (tags[0] == "0" && tags[1] != "0") ? contador = 1 : contador;
        tags[contador] = "0";
        lcd.clear();
        lcd.setCursor(4, 0);
        lcd.print("Llave");
        lcd.setCursor(2, 1);
        lcd.print("Eliminada");
        delay(intervalo);
        indiceMenu = 2;
        contador = 0;
        fn_menu(contador, menu3, sizemenu3);
      }

      if (hashPresionado) {
        hashPresionado = false;
        nfcTemporal = true;
        llaveEncontrada = false;
        indiceMenu = 2;
        contador = 0;
        fn_menu(contador, menu3, sizemenu3);
      }
    }
  }
}

bool strComp(bool cod) {
  if (cod) {
    for (int i = 0; i < 16; i++) {
      if (ingresaPass[i] != pass[i]) {
        return 0;
      }
    }
  } else {
    for (int i = 0; i < 4; i++) {
      if (ingresaPass[i + 9] != codigo[i]) {
        return 0;
      }
    }
  }
  return 1;
}

void fn_menu(int pos, String menus[], byte sizemenu) {
  lcd.noBlink();
  lcd.clear();
  linea1 = "";
  linea2 = "";

  if ((pos % 2) == 0) {

    lcd.setCursor(0, 0);
    lcd.print(">");
    linea1 = menus[pos];

    if (pos + 1 != sizemenu) {
      linea2 = menus[pos + 1];
    }

  } else {
    linea1 = menus[pos - 1];
    lcd.setCursor(0, 1);
    lcd.print(">");
    linea2 = menus[pos];
  }

  lcd.setCursor(2, 0);
  lcd.print(linea1);

  lcd.setCursor(2, 1);
  lcd.print(linea2);
}

bool fn_refresh(int sizemenu) {
  TECLA = teclado.getKey();
  if (TECLA == '2' || TECLA == '8' || TECLA == '*' || TECLA == '#') {
    if (TECLA == '2') {
      (contador <= 0) ? contador = 0 : contador--;
    }

    if (TECLA == '8') {
      (contador >= sizemenu - 1) ? contador = sizemenu - 1 : contador++;
    }

    if (TECLA == '*') starPresionado = true;

    if (TECLA == '#') hashPresionado = true;
    return true;
  }
  return false;
}

void NFCs(char estado, int pos) {
  if (estado == 'A') {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ESPERANDO ...");
    if (nfc.tagPresent()) {
      lcd.clear();
      NfcTag tag = nfc.read();
      Serial.println("Tag id: " + tag.getUidString());
      tags[pos] = tag.getUidString();
      lcd.setCursor(4, 0);
      lcd.print("Llave");
      lcd.setCursor(3, 1);
      lcd.print("Agregada");
      delay(intervalo);
      lcd.clear();
    }
  } else if (estado == 'I') {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ESPERANDO ...");
    if (nfc.tagPresent()) {
      lcd.clear();
      NfcTag tag = nfc.read();
      Serial.println("Tag id: " + tag.getUidString());
      for (int i = 0; i < sizetags; i++) {
        if (tag.getUidString() == tags[i]) {
          lcd.setCursor(4, 0);
          lcd.print("Llave");
          lcd.setCursor(10, 0);
          lcd.print(i + 1);
          lcd.setCursor(3, 1);
          lcd.print("Encontrada");
          delay(intervalo);
          lcd.clear();
          goto salir;
        }
      }
      lcd.setCursor(4, 0);
      lcd.print("Llave no");
      lcd.setCursor(3, 1);
      lcd.print("Encontrada");
      delay(intervalo);
      lcd.clear();
    }
  }
salir:
  contador = 0;
  fn_menu(contador, menu3, sizemenu3);
}

int faceRec() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("RECONOCIENDO...");
  tiempoAnt = millis();
  while (millis() - tiempoAnt < 30000) {
    fn_refresh(1);
    if (digitalRead(facialCheck)) {
      lcd.clear();
      return 1;
      if (hashPresionado) {
        hashPresionado = false;
        return 2;
      }
    }
  }
  lcd.clear();
  return 0;
}

void abrirPuerta() {
  digitalWrite(motorPin1, HIGH);
  digitalWrite(motorPin2, LOW);
  delay(5000);
  digitalWrite(motorPin1, LOW);
}

void cerrarPuerta() {
  digitalWrite(motorPin1, LOW);
  digitalWrite(motorPin2, HIGH);
  delay(5000);
  digitalWrite(motorPin2, LOW);
}

void lcdInit() {
  lcd.backlight();
  lcd.init();
  lcd.home();
  lcd.noCursor();
  lcd.noBlink();
}
