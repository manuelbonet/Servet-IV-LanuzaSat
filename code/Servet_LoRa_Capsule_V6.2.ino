#include <LoRa.h>

#include <SPIFFS.h>
#include "FS.h"
#include "SPI.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <TinyGPS++.h>

//Datos LoRa
#define voltajeMinimo 3700

#define INTERVALO_NORMAL 47*1000
#define INTERVALO_BATERIA_BAJA 1000*(60*4 + 37)

#define LORA_CS 18
#define LORA_RST 23
#define LORA_IRQ 26

#define LORA_SF 10

byte header = 0xAA;
byte mensajeID = 0x00;

unsigned long tiempoUltimoEnvio = 0;
int intervalo = INTERVALO_NORMAL;

//Datos SIM800L
#define pinSIM 1234
#define nombreAPN ""
#define userAPN ""
#define pwdAPN ""

#define APIKey "" //Thingspeak API

//Datos BME
#define utilizarBME true

Adafruit_BME280 bme;

//Datos DS18B20
OneWire oneWire(25);
DallasTemperature DS18B20(&oneWire);


//Datos GPS
TinyGPSPlus gps;


//Datos radiactividad

#define PIN_RADIACTIVIDAD 4
volatile unsigned long cuentasPorSegundo = 0;
unsigned int lecturas[60];

unsigned long tiempoUltimaRadiactividad = 0;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR deteccionParticula() {
  portENTER_CRITICAL_ISR(&mux);
  cuentasPorSegundo++;
  portEXIT_CRITICAL_ISR(&mux);
}

//Datos SPIFFS
#define FORMATEAR_SPIFFS_SI_NECESARIO true
bool grabarSPIFFS = true;
bool mostrarDatos = true;
bool borrarAntesSPIFFS = false;
bool formatearAntesSPIFFS = false;

void setup() {
  Serial.begin(115200);

  Serial.println("Servet Lanuza");

  delay(5000);
  delay(5000);

  //CONFIGURACIÓN LORA
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
  // LoRa.setSpreadingFactor(LORA_SF);

  if (!LoRa.begin(868E6)) {
    Serial.println("Inicialización LoRa fallida");
  }
  else {
    Serial.println("Inicialización LoRa satisfactoria");
  }

  LoRa.setSpreadingFactor(LORA_SF);

  //CONFIGURACIÓN SIM800L
  Serial2.begin(9600, SERIAL_8N1, 14, 13);

  if (hayConexionSerie()) {
    Serial.println("Hay conexión con el SIM800L");
  } else {
    Serial.println("No hay conexión con el SIM800L");
  }

  if (haySIM()) {
    Serial.println("Hay tarjeta SIM");
  } else {
    Serial.println("No hay tarjeta SIM");
  }

  Serial.println("Batería: " + String(bateria() / 1000.0, 3) + " V");

  //CONFIGURACIÓN BME280
  if (utilizarBME) {
    if (!bme.begin(0x76)) {
      Serial.println("Inicialización BME280 fallida");
    } else {
      Serial.println("Inicialización BME280 satisfactoria");
    }
  }
  else {
    Serial.println("BME280 no utilizado");
  }

  //CONFIGURACIÓN DS18B20
  DS18B20.begin();

  //CONFIGURACIÓN GPS
  Serial1.begin(9600, SERIAL_8N1, 12, 15);   //17-TX 18-RX

  uint8_t setNav[] = {
    0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00,
    0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xDC
  };

  for (int i = 0; i < sizeof(setNav) / sizeof(setNav[0]); i++) {
    Serial1.write(setNav[i]);
  }

  //LoRa.onReceive(onReceive);
  //LoRa.receive();

  //CONFIGURACIÓN SPIFFS
  if (!SPIFFS.begin(FORMATEAR_SPIFFS_SI_NECESARIO)) {
    Serial.println("Inicialización SPIFFS fallida");
  } else {
    Serial.println("Inicialización SPIFFS satisfactoria");
  }

  delay(2000);

  //CONFIGURACIÓN RADIACTIVIDAD
  pinMode(PIN_RADIACTIVIDAD, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_RADIACTIVIDAD), deteccionParticula, FALLING);
}

void loop() {
  if (millis() > tiempoUltimaRadiactividad + 1000) {
    tiempoUltimaRadiactividad = millis();            // timestamp the message

    portENTER_CRITICAL(&mux);
    unsigned long lecturasNuevas = cuentasPorSegundo;
    cuentasPorSegundo = 0;
    portEXIT_CRITICAL(&mux);

    for (int i = 59; i > 0 ; i--) {
      lecturas[i] = lecturas[i - 1];
    }

    lecturas[0] = lecturasNuevas;
  }

  if (millis() > tiempoUltimoEnvio + intervalo) {
    tiempoUltimoEnvio = millis();            // timestamp the message

    enviarDatos();
  }

  while (Serial1.available()) {
    gps.encode(Serial1.read());
  }

  if (Serial.available()) {
    String comandoRaw;
    String comando;

    while (Serial.available()) {
      comandoRaw += (char)Serial.read();
    }

    comando = comandoRaw;
    comando.replace("\n", "");
    comando.replace("\r", "");
    comando.replace(" ", "_");
    comando.toUpperCase();

    if (comando != "SPIFFS_BORRAR" && borrarAntesSPIFFS == true) {
      Serial.println("Borrar SPIFFS cancelado");
      borrarAntesSPIFFS = false;
      return;
    }

    if (comando != "SPIFFS_FORMATEAR" && formatearAntesSPIFFS == true) {
      Serial.println("Formatear SPIFFS cancelado");
      formatearAntesSPIFFS = false;
      return;
    }

    if (comando == "AYUDA") {
      Serial.println("Comando AYUDA: Los comandos existentes son SPIFFS_ON, SPIFFS_OFF, SPIFFS?, SPIFFS_BORRAR, SPIFFS_FORMATEAR, SPIFFS_TAMANO, SERIE_ON, SERIE_OFF y SERIE?");
    }
    else if (comando == "SPIFFS_ON") {
      grabarSPIFFS = true;
      Serial.println("Comando SPIFFS_ON: Información guardada en SPIFFS");
    } else if (comando == "SPIFFS_OFF") {
      grabarSPIFFS = false;
      Serial.println("Comando SPIFFS_OFF: Información no guardada en SPIFFS");
    } else if (comando == "SPIFFS?") {
      if (grabarSPIFFS) {
        Serial.println("Comando SPIFFS?: SPIFFS activado");
      } else {
        Serial.println("Comando SPIFFS?: SPIFFS desactivado");
      }
    } else if (comando == "SPIFFS_BORRAR") {
      if (borrarAntesSPIFFS) {
        writeFile(SPIFFS, "/CanSat.txt", "");
        Serial.println("Comando SPIFFS_BORRAR: Archivo borrado de SPIFFS");
        borrarAntesSPIFFS = false;
      } else {
        Serial.println("Comando SPIFFS_BORRAR: Escribe el comando de nuevo para borrar el archivo");
        borrarAntesSPIFFS = true;
      }
    } else if (comando == "SPIFFS_FORMATEAR") {
      if (formatearAntesSPIFFS) {
        SPIFFS.format();
        Serial.println("Comando SPIFFS_FORMATEAR: memoria SPIFFS formateada");
        formatearAntesSPIFFS = false;
      } else {
        Serial.println("Comando SPIFFS_FORMATEAR: Escribe el comando de nuevo para formatear la memoria SPIFFS");
        formatearAntesSPIFFS = true;
      }
    } else if (comando == "SPIFFS_LEER") {
      Serial.println("Comando SPIFFS_LEER: Información guardada enviada por serie");
      Serial.println("[INICIO]");
      //Serial.print(readFile(SPIFFS, "/CanSat.txt"));
      readFile(SPIFFS, "/CanSat.txt");
      Serial.println("\n[FIN]");
    } else if (comando == "SPIFFS_TAMANO") {
      unsigned long tamano = fileSize(SPIFFS, "/CanSat.txt");
      Serial.println("Comando SPIFFS_TAMANO: " + String(tamano / 1024.0, 1) + " KB (" + String(tamano / 1507328.0 * 100.0, 1) + "%)");
    } else if (comando == "SERIE_ON") {
      mostrarDatos = true;
      Serial.println("Comando SERIE_ON: Datos mostrados por serie");
    } else if (comando == "SERIE_OFF") {
      mostrarDatos = false;
      Serial.println("Comando SERIE_OFF: Datos no mostrados por serie");
    } else if (comando == "SERIE?") {
      if (mostrarDatos) {
        Serial.println("Comando SERIE?: Serie activado");
      } else {
        Serial.println("Comando SERIE?: Serie desactivado");
      }
    } else {
      if (comandoRaw.indexOf("AT") == 0) {
        Serial.print(enviarAT(comandoRaw, 5000));
      } else {
        Serial.println("No entiendo el comando " + comando + ", escribe AYUDA para obtener la lista de comandos");
      }
    }
  }
}

void enviarDatos() {

  byte checksum = 0;

  //Datos en bruto
  float temperaturaRaw = utilizarBME ? bme.readTemperature() : 20.00;
  unsigned long presionRaw = utilizarBME ? bme.readPressure() : 101325;
  float humedadRaw = utilizarBME ? bme.readHumidity() : 50.00;

  DS18B20.requestTemperatures();
  float temperaturaExteriorRaw = DS18B20.getTempCByIndex(0);

  float latitudRaw = gps.location.lat();
  float longitudRaw = gps.location.lng();
  unsigned long altitudRaw = gps.altitude.meters();
  unsigned int satelitesRaw = gps.satellites.value();
  int horaHora = gps.time.hour();
  int horaMinuto = gps.time.minute();
  int horaSegundo = gps.time.second();
  unsigned long horaRaw = horaHora * 3600 + horaMinuto * 60 + horaSegundo;

  unsigned long radiactividadRaw = 0;
  for (int i = 0; i < 60; i++) {
    radiactividadRaw += lecturas[i];
    //Serial.print(lecturas[i]);
  }
  if ((floor(millis() / 1000) + 1) < 60) {
    radiactividadRaw *= 60.0 / (floor(millis() / 1000) + 1);
  }

  float voltajeBateriaRaw = bateria() / 1000.0;

  //Serial.println("Temperatura: " + String(temperaturaRaw));
  //Serial.println("Presión: " + String(presionRaw));
  //Serial.println("Humedad: " + String(humedadRaw));
  //Serial.println("Latitud: " + String(latitudRaw, 5));
  //Serial.println("Longitud: " + String(longitudRaw, 5));
  //Serial.println("Altitud: " + String(altitudRaw));
  //Serial.println("Satélites: " + String(satelitesRaw));
  //Serial.println("Hora: " + String(horaRaw));
  //Serial.println("Radiactividad: " + String(radiactividadRaw));

  //Datos procesados
  unsigned long temperatura = 100 * temperaturaRaw + 32768;
  unsigned long presion = presionRaw;
  unsigned long humedad = humedadRaw * 100;

  unsigned long temperaturaExterior = 100 * temperaturaExteriorRaw + 32768;

  unsigned long latitud = ((latitudRaw >= 0) ? latitudRaw : -latitudRaw) * 100000;
  unsigned long longitud = longitudRaw * 100000 + ((longitudRaw < 0) ? 36000000 : 0);
  unsigned long altitud = altitudRaw;
  unsigned long satelites = satelitesRaw;
  unsigned long hora = horaRaw;

  unsigned long radiactividad = radiactividadRaw;

  unsigned long voltajeBateria = voltajeBateriaRaw * 1000;

  //Serial.println("Temperatura enviada: " + String(temperatura));
  //Serial.println("Presión enviada: " + String(presion));
  //Serial.println("Humedad enviada: " + String(humedad));
  //Serial.println("Latitud enviada: " + String(latitud));
  //Serial.println("Longitud enviada: " + String(longitud));
  //Serial.println("Altitud enviada: " + String(altitud));
  //Serial.println("Satélites enviada: " + String(satelites));
  //Serial.println("Hora enviada: " + String(hora));
  //Serial.println("Radiactividad enviada: " + String(radiactividad));


  //Serial.println("Datos recogidos");

  byte datos[26];
  datos[0] = recortar(temperatura, 15, 8);
  datos[1] = recortar(temperatura, 7, 0);

  datos[2] = recortar(temperaturaExterior, 15, 8);
  datos[3] = recortar(temperaturaExterior, 7, 0);

  datos[4] = recortar(presion, 23, 16);
  datos[5] = recortar(presion, 15, 8);
  datos[6] = recortar(presion, 7, 0);

  datos[7] = recortar(humedad, 15, 8);
  datos[8] = recortar(humedad, 7, 0);

  datos[9] = recortar(latitud, 23, 16);
  datos[10] = recortar(latitud, 15, 8);
  datos[11] = recortar(latitud, 7, 0);

  datos[12] = recortar(longitud, 31, 24);
  datos[13] = recortar(longitud, 23, 16);
  datos[14] = recortar(longitud, 15, 8);
  datos[15] = recortar(longitud, 7, 0);

  datos[16] = recortar(altitud, 15, 8);
  datos[17] = recortar(altitud, 7, 0);

  datos[18] = recortar(satelites, 7, 0);

  datos[19] = recortar(hora, 23, 16);
  datos[20] = recortar(hora, 15, 8);
  datos[21] = recortar(hora, 7, 0);

  datos[22] = recortar(radiactividad, 15, 8);
  datos[23] = recortar(radiactividad, 7, 0);

  datos[24] = recortar(voltajeBateria, 15, 8);
  datos[25] = recortar(voltajeBateria, 7, 0);

  //Serial.println("Datos reordenados");

  for (int i = 0; i < sizeof(datos) / sizeof(datos[0]); i++) {
    checksum += datos[i];
    //Serial.println(datos[i], BIN);
  }

  //Serial.println("Checksum calculado");

  //Serial.print("Datos enviados: ");

  if (bateria() >= voltajeMinimo) {
    intervalo = INTERVALO_NORMAL;

    LoRa.beginPacket();                   // start packet
    LoRa.write(header);                   // add header
    //Serial.print((char)header);
    LoRa.write(mensajeID);                // add message ID
    //Serial.print((char)mensajeID);
    LoRa.write(checksum);        // add payload length
    //Serial.print((char)checksum);

    for (int i = 0; i < sizeof(datos) / sizeof(datos[0]); i++) {
      LoRa.write(datos[i]);
      //Serial.print((char)datos[i]);
    }

    LoRa.endPacket();                     // finish packet and send it
  } else {
    intervalo = INTERVALO_BATERIA_BAJA;
  }

  String paginaEnvio = "http://api.thingspeak.com/update?api_key=" + String(APIKey);
  paginaEnvio += "&field1=" + String(temperaturaExteriorRaw, 2);
  paginaEnvio += "&field2=" + String(presionRaw);
  paginaEnvio += "&field3=" + String(humedadRaw, 2);
  paginaEnvio += "&field4=" + String(latitudRaw, 5);
  paginaEnvio += "&field5=" + String(longitudRaw, 5);
  paginaEnvio += "&field6=" + String(altitudRaw);
  paginaEnvio += "&field7=" + String(radiactividadRaw);
  if (voltajeBateriaRaw != 3.7) {
    paginaEnvio += "&field8=" + String(voltajeBateriaRaw, 3);
  }

  if (conexionRed(pinSIM)) {
    if (conexionGPRS(nombreAPN, userAPN, pwdAPN)) {
      accederWeb(paginaEnvio);
    }
  }

  String datosString = "";
  datosString += String((unsigned int)header);
  datosString += ",";
  datosString += String((unsigned int)mensajeID);
  datosString += ",";
  datosString += String((unsigned int)checksum);
  datosString += ",";
  datosString += String(temperaturaRaw, 2);
  datosString += ",";
  datosString += String(temperaturaExteriorRaw, 2);
  datosString += ",";
  datosString += String(presionRaw);
  datosString += ",";
  datosString += String(humedadRaw, 2);
  datosString += ",";
  datosString += String(latitudRaw, 5);
  datosString += ",";
  datosString += String(longitudRaw, 5);
  datosString += ",";
  datosString += String(altitudRaw);
  datosString += ",";
  datosString += String(horaHora);
  datosString += ",";
  datosString += String(horaMinuto);
  datosString += ",";
  datosString += String(horaSegundo);
  datosString += ",";
  datosString += String(radiactividadRaw);
  datosString += ",";
  datosString += String(voltajeBateriaRaw, 3);
  datosString += "\r\n";

  if (mostrarDatos) {
    Serial.print(datosString);
  }

  if (grabarSPIFFS) {
    char datosChar[250];
    datosString.toCharArray(datosChar, 250);
    appendFile(SPIFFS, "/CanSat.txt", datosChar);
  }

  mensajeID++;                           // increment message ID
}

byte recortar(unsigned long numero, unsigned int inicio, unsigned int fin, unsigned int offset) { //inicio y fin desde el final, 0..n
  return ((numero & (unsigned long)round(pow(2, max(inicio, fin) + 1) - 1)) >> min(inicio, fin)) << offset;
}

byte recortar(unsigned long numero, unsigned int inicio, unsigned int fin) { //inicio y fin desde el final, 0..n
  return (numero & (unsigned long)round(pow(2, max(inicio, fin) + 1) - 1)) >> min(inicio, fin);
}

void readFile(fs::FS &fs, const char * path) {
  String fileData;
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    //return "ERROR";
    return;
  }
  while (file.available()) {
    Serial.print((char)file.read());
    //fileData += (char)file.read();
  }
  file.close();
  //return fileData;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    return;
  }
  file.print(message);
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    return;
  }
  file.print(message);
  file.close();
}

unsigned long fileSize(fs::FS &fs, const char * path) {
  File file = fs.open(path);
  if (!file) {
    return 0;
  }
  unsigned long tamano = file.size();
  file.close();
  return tamano;
}

String enviarAT(String texto, unsigned int timeout) {

  String datos = "";

  Serial2.println(texto);

  unsigned long inicio = millis();

  while (!Serial2.available()) {
    if (millis() > inicio + timeout) {
      return "";
    }
  }

  while (Serial2.available()) {
    datos += (char)Serial2.read();
  }

  return datos;
}

String enviarAT(String texto, String respuesta, unsigned int tiempo, unsigned int intentos) {

  String datos;
  unsigned int i = 0;

  do {
    Serial2.println(texto);

    datos = "";

    unsigned long inicio = millis();

    while (!Serial2.available()) {
      if (millis() > inicio + 2000) {
        //Serial.println("TIMEOUT (" + texto + "): " + String((millis() - inicio) / 1000) + " s");
        return "";
      }
    }

    while (Serial2.available()) {
      datos += (char)Serial2.read();
    }
    //Serial.print(datos);
    if (datos.indexOf(respuesta) == -1) {
      delay(tiempo);
      i ++;
    }

  } while (datos.indexOf(respuesta) == -1 && i < intentos);

  if (i >= intentos) {
    //Serial.println ("TIMEOUT (" + texto + "): " + String(i) + " intentos");
    return "";
  } else {
    return datos;
  }
}

String enviarAT(String texto, String respuesta1, String respuesta2, unsigned int tiempo, unsigned int intentos) {

  String datos;
  unsigned int i = 0;

  do {
    Serial2.println(texto);

    datos = "";

    unsigned long inicio = millis();

    while (!Serial2.available()) {
      if (millis() > inicio + 2000) {
        //Serial.println("TIMEOUT (" + texto + "): " + String((millis() - inicio) / 1000) + " s");
        return "";
      }
    }

    while (Serial2.available()) {
      datos += (char)Serial2.read();
    }
    //Serial.print(datos);
    if (datos.indexOf(respuesta1) == -1 && datos.indexOf(respuesta2) == -1) {
      delay(tiempo);
      i ++;
    }

  } while (datos.indexOf(respuesta1) == -1 && datos.indexOf(respuesta2) == -1 && i < intentos);

  if (i >= intentos) {
    //Serial.println ("TIMEOUT (" + texto + "): " + String(i) + " intentos");
    return "";
  } else {
    return datos;
  }
}

String enviarAT(String texto, String respuesta1, String respuesta2, String respuesta3, unsigned int tiempo, unsigned int intentos) {

  String datos;
  unsigned int i = 0;

  do {
    Serial2.println(texto);

    datos = "";

    unsigned long inicio = millis();

    while (!Serial2.available()) {
      if (millis() > inicio + 2000) {
        //Serial.println("TIMEOUT (" + texto + "): " + String((millis() - inicio) / 1000) + " s");
        return "";
      }
    }

    while (Serial2.available()) {
      datos += (char)Serial2.read();
    }
    //Serial.print(datos);
    if (datos.indexOf(respuesta1) == -1 && datos.indexOf(respuesta2) == -1 && datos.indexOf(respuesta3) == -1) {
      delay(tiempo);
      i ++;
    }

  } while (datos.indexOf(respuesta1) == -1 && datos.indexOf(respuesta2) == -1 && datos.indexOf(respuesta3) == -1 && i < intentos);

  if (i >= intentos) {
    //Serial.println ("TIMEOUT (" + texto + "): " + String(i) + " intentos");
    return "";
  } else {
    return datos;
  }
}

bool hayConexionSerie() {
  if (enviarAT("AT", "OK", 100, 1).indexOf("OK") != -1) {
    return true;
  } else {
    return false;
  }
}

bool haySIM() {
  if (hayConexionSerie()) {
    if (enviarAT("AT+CPIN?", "SIM PIN", "READY", "+CME ERROR: 10", 200, 5).indexOf("10") != -1) {
      return false;
    } else {
      return true;
    }
  } else {
    return false;
  }
}

unsigned int bateria() {
  if (hayConexionSerie()) {
    int voltaje;
    int intentos = 0;
    do {
      String respuesta = enviarAT("AT+CBC", "OK", 200, 5);
      //Serial.println(respuesta);
      int inicio = respuesta.lastIndexOf(",") + 1;
      voltaje = respuesta.substring(inicio).toInt();
      if (voltaje < 100) {
        delay(500);
      }
      intentos ++;
    } while (intentos < 10 && voltaje < 100);

    if (voltaje < 100) {
      voltaje = 3700;
    }

    return voltaje;
  } else {
    return 3700;
  }
}

bool conexionRed(int pin) {
  if (enviarAT("AT+CPIN=\"" + String(pin) + "\"", "READY", "+CME ERROR: 3", 200, 5) == "") {
    return false;
  }

  //delay(1000);

  if (enviarAT("AT+CREG?", "0,1", "0,5", 3000, 10) == "") {
    return false;
  }

  return true;
}

bool conexionGPRS(String APN, String USER, String PWD) {
  if (enviarAT("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "OK", 200, 5) == "") {
    return false;
  }

  //delay(1000);

  if (enviarAT("AT+SAPBR=3,1,\"APN\",\"" + APN + "\"", "OK", 200, 5) == "") {
    return false;
  }

  //delay(1000);

  if (enviarAT("AT+SAPBR=3,1,\"USER\",\"" + USER + "\"", "OK", 200, 5) == "") {
    return false;
  }

  //delay(1000);

  if (enviarAT("AT+SAPBR=3,1,\"PWD\",\"" + PWD + "\"", "OK", 200, 5) == "") {
    return false;
  }

  //delay(1000);

  if (enviarAT("AT+SAPBR=1,1", "OK", "+CME ERROR: 3", 200, 5) == "") {
    return false;
  }

  //delay(1000);

  if (enviarAT("AT+SAPBR=2,1", "1,1", 1000, 20) == "") {
    return false;
  }

  //delay(1000);

  if (enviarAT("AT+CGATT=1", "OK", 200, 5) == "") {
    return false;
  }

  return true;
}

String accederWeb(String URL) {
  bool HTTPS = false;

  if (URL.substring(0, 8) == "https://") {
    HTTPS = true;
  }

  if (enviarAT("AT+HTTPINIT", "OK", "+CME ERROR: 3", 200, 5) == "") {
    return "";
  }

  //delay(1000);

  if (enviarAT("AT+HTTPSSL=" + String(HTTPS ? "1" : "0"), "OK", 200, 5) == "") {
    return "";
  }

  //delay(1000);

  if (enviarAT("AT+HTTPPARA=\"CID\",1", "OK", 200, 5) == "") {
    return "";
  }

  //delay(1000);

  if (enviarAT("AT+HTTPPARA=\"URL\",\"" + URL + "\"", "OK", 200, 5) == "") {
    return "";
  }

  //delay(1000);

  if (enviarAT("AT+HTTPACTION=0", "OK", 200, 5) == "") {
    return "";
  }

  while (Serial2.available()) {
    Serial2.read();
  }

  String datos = enviarAT("AT+HTTPREAD", "+", 200, 5);

  unsigned long inicio = datos.indexOf("\r", 20) + 1;

  while (Serial2.available()) {
    Serial2.read();
  }

  return datos.substring(inicio);
}
