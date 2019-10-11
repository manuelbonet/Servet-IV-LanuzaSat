#include <LoRa.h>
#include <Wire.h>
#include "SSD1306.h"

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define LORASCK     5
#define LORAMISO    19
#define LORAMOSI    27
#define LORACS      18
#define LORARST     23
#define DI0     26
#define BAND    868E6
#define LORA_SF 10

#define SDSCK     14
#define SDMISO    2
#define SDMOSI    15
#define SDCS      13

#define useBME true

Adafruit_BME280 bme; // I2C

SSD1306 display(0x3c, 21, 22);

String DataWriteSD = "";          // String de escritura SD
bool ErrorSD = false;              // False = no error
bool borrarAntesSD = false;

// SD CARD

void readFile(fs::FS &fs, const char * path) {
  String fileData;
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    //return "ERROR";
    ErrorSD = true;
    return;
  }
  while (file.available()) {
    Serial.print((char)file.read());
    //fileData += (char)file.read();
  }
  file.close();
  ErrorSD = false;
  //return fileData;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    ErrorSD = true;
    return;
  }
  file.print(message);
  file.close();
  ErrorSD = false;
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  //Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    //Serial.println("Failed to open file for appending");
    ErrorSD = true;
    return;
  }
  if (file.print(message)) {
    //Serial.print("Message appended,");
    //Serial.println (message);
    ErrorSD = false;

  } else {
    //Serial.println("Append failed");
    ErrorSD = true;
  }
  file.close();
}

unsigned long fileSize(fs::FS &fs, const char * path) {
  File file = fs.open(path);
  if (!file) {
    ErrorSD = true;
    return 0;
  }
  unsigned long tamano = file.size();
  file.close();
  return tamano;
  ErrorSD = false;
}

void cbk(int packetSize) {
  Serial.println("HE RECIBIDO ALGO");
  bool mensajeMio;

  String recepcion = "";

  byte header;
  byte mensajeID;
  byte checksum;
  byte checksumCalculado = 0;
  bool checksumCoincide;

  float temperatura;
  long presion;
  float humedad;

  float temperaturaExterior;

  float latitud;
  float longitud;
  long altitud;
  int satelites;
  long hora;

  int horaHora;
  int horaMinuto;
  int horaSegundo;
  String horaTexto;
  String horaCortaTexto;

  unsigned long radiactividad;

  float voltajeBateria;

  byte datos[26];

  float temperaturaBase;
  long presionBase;
  float humedadBase;

  header = LoRa.read();

  if (header == B10101010 && packetSize == 29) {
    mensajeMio = true;
  } else {
    mensajeMio = false;
  }

  if (mensajeMio) {

    mensajeID = LoRa.read();
    checksum = LoRa.read();

    for (int i = 0; i < sizeof(datos) / sizeof(datos[0]); i ++) {
      datos[i] = LoRa.read();
      checksumCalculado += datos[i];
    }

    checksumCoincide = (checksum == checksumCalculado);

    temperatura = (((datos[0] << 8) | datos[1]) - 32768) / 100.0;
    presion = (datos[4] << 16) | (datos[5] << 8) | (datos[6]);
    humedad = ((datos[7] << 8) | (datos[8])) / 100.0;


    temperaturaExterior = (((datos[2] << 8) | datos[3]) - 32768) / 100.0;

    latitud = ((datos[9] << 16) | (datos[10] << 8) | (datos[11])) / 100000.0;
    longitud = ((datos[12] << 24) | (datos[13] << 16) | (datos[14] << 8) | (datos[15])) / 100000.0;

    longitud = (longitud <= 180) ? longitud : longitud - 360.0;

    altitud = (datos[16] << 8) | (datos[17]);
    satelites = (datos[18]);

    hora = (datos[19] << 16) | (datos[20] << 8) | (datos[21]);

    horaHora = hora / 3600;
    horaMinuto = (hora - horaHora * 3600) / 60;
    horaSegundo = hora - horaHora * 3600 - horaMinuto * 60;

    horaTexto = ((horaHora < 10) ? "0" : "") + String(horaHora) + ":" + ((horaMinuto < 10) ? "0" : "") + String(horaMinuto) + ":" + ((horaSegundo < 10) ? "0" : "") + String(horaSegundo);
    horaCortaTexto = ((horaHora < 10) ? "0" : "") + String(horaHora) + ":" + ((horaMinuto < 10) ? "0" : "") + String(horaMinuto);

    radiactividad = (datos[22] << 8) | (datos[23]);
    voltajeBateria = ((datos[24] << 8) | (datos[25])) / 1000.0;

  } else {
    recepcion += (char)header;
    while (LoRa.available()) {
      recepcion += (char)LoRa.read();
    }
  }

  temperaturaBase = useBME ? bme.readTemperature() : 20.0;
  presionBase = useBME ? bme.readPressure() : 101325;
  humedadBase = useBME ? bme.readHumidity() : 50.0;

  //Display
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  display.drawString(0, 0, String(LoRa.packetRssi()) + "dBm " + (checksumCoincide ? "CHK OK" : "CHK NOK") + " " + (ErrorSD ? "SD NOK " : "SD OK"));

  //display.drawStringMaxWidth(0 , 24 , 128, packet);

  DataWriteSD = "";

  if (mensajeMio) {
    display.drawString(0, 10, String(packetSize, DEC) + " B, " + "ID: " + String((unsigned int)mensajeID, DEC) + ", " + horaCortaTexto + " (" + String(temperatura, 0) + "°C)");
    display.drawString(0, 20, String(temperaturaExterior, 2) + "°C " + String(presion, DEC) + "Pa " + String(humedad, 2) + "%");
    display.drawString(0, 30, String(latitud, 5) + "°N, " + String(longitud, 5) + "°E");
    display.drawString(0, 40, String(altitud) + "m " + String(satelites) + "s " + String(radiactividad, DEC) + "CPM " + String(voltajeBateria, 2) + "V");
    display.drawString(0, 50, String(temperaturaBase, 2) + "°C " + String(presionBase) + "Pa");
    display.display();

    DataWriteSD += String (header);
    DataWriteSD += ",";
    DataWriteSD += String (LoRa.packetRssi());
    DataWriteSD += ",";
    DataWriteSD += String (mensajeID);
    DataWriteSD += ",";
    DataWriteSD += String (checksum);
    DataWriteSD += ",";
    DataWriteSD += String (checksumCalculado);
    DataWriteSD += ",";
    DataWriteSD += String (temperatura, 2);
    DataWriteSD += ",";
    DataWriteSD += String (presion);
    DataWriteSD += ",";
    DataWriteSD += String (humedad, 2);
    DataWriteSD += ",";
    DataWriteSD += String (latitud, 5);
    DataWriteSD += ",";
    DataWriteSD += String (longitud, 5);
    DataWriteSD += ",";
    DataWriteSD += String (altitud);
    DataWriteSD += ",";
    DataWriteSD += String (satelites);
    DataWriteSD += ",";
    DataWriteSD += String (horaHora);
    DataWriteSD += ",";
    DataWriteSD += String (horaMinuto);
    DataWriteSD += ",";
    DataWriteSD += String (horaSegundo);
    DataWriteSD += ",";
    DataWriteSD += String (radiactividad);
    DataWriteSD += ",";
    DataWriteSD += String (voltajeBateria, 3);
    DataWriteSD += ",";
    DataWriteSD += String (temperaturaBase);
    DataWriteSD += ",";
    DataWriteSD += String (presionBase);
    DataWriteSD += ",";
    DataWriteSD += String (humedadBase);
    DataWriteSD += ("\r\n");
  } else {
    DataWriteSD += recepcion;
  }


  Serial.print(DataWriteSD);

}

void setup() {
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);
  delay(50);
  digitalWrite(16, HIGH);

  bme.begin(0x76);

  display.init();
  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  Serial.begin(115200);
  delay (2000);
  Serial.println();
  Serial.println("Servet LoRa Receiver");

  SPI.begin(LORASCK, LORAMISO, LORAMOSI, LORACS);

  LoRa.setPins(LORACS, LORARST, DI0);
  if (!LoRa.begin(868E6)) {
    Serial.println("Starting LoRa failed!");
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "WARNING");
    display.drawString(0, 10, "LORA INIT ERROR");

    display.display();

    while (1);

    LoRa.setSpreadingFactor(LORA_SF);
  }

  LoRa.receive();
  Serial.println("LoRa init ok");

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  //display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "LORA INIT OK");
  display.display();

  SPI.end();
  SPI.begin(SDSCK, SDMISO, SDMOSI, SDCS);
  if (!SD.begin(SDCS)) {
    Serial.println("Card Mount Failed");
    display.drawString(0, 10, "ERROR SD CARD");
    display.display();
    ErrorSD = true;
    delay (1000);

  }
  else {
    Serial.println("Card Mount OK");
    display.drawString(0, 15, "SD CARD OK");
    display.display();
    ErrorSD = false;

  }

  SPI.end();
  SPI.begin(LORASCK, LORAMISO, LORAMOSI, LORACS);
  LoRa.setSpreadingFactor(LORA_SF);

  delay(1500);
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    cbk(packetSize);


    SPI.end();
    SPI.begin(SDSCK, SDMISO, SDMOSI, SDCS);  // SD


    char buffer[200];
    DataWriteSD.toCharArray(buffer, 200);
    appendFile(SD, "/Servet.txt", buffer);


    SPI.end();
    SPI.begin(LORASCK, LORAMISO, LORAMOSI, LORACS); // LORA
    LoRa.setSpreadingFactor(LORA_SF);
    //loraData();

  }

  if (Serial.available()) {
    String comando;

    while (Serial.available()) {
      comando += (char)Serial.read();
    }

    comando.replace("\n", "");
    comando.replace("\r", "");
    comando.replace(" ", "_");
    comando.toUpperCase();

    if (comando != "SD_BORRAR" && borrarAntesSD == true) {
      Serial.println("Borrar SD cancelado");
      borrarAntesSD = false;
      return;
    }

    if (comando == "AYUDA") {
      Serial.println("Comando AYUDA: Los comandos existentes son SD_BORRAR, SD_LEER y SD_TAMANO");
    } else if (comando == "SD_BORRAR") {
      if (borrarAntesSD) {
        writeFile(SD, "/Servet.txt", "");
        Serial.println("Comando SD_BORRAR: Archivo borrado de SD");
        borrarAntesSD = false;
      } else {
        Serial.println("Comando SD_BORRAR: Escribe el comando de nuevo para borrar el archivo");
        borrarAntesSD = true;
      }
    } else if (comando == "SD_LEER") {
      Serial.println("Comando SD_LEER: Información guardada enviada por serie");
      Serial.println("[INICIO]");
      readFile(SD, "/Servet.txt");
      Serial.println("\n[FIN]");
    } else if (comando == "SD_TAMANO") {
      unsigned long tamano = fileSize(SD, "/Servet.txt");
      Serial.println("Comando SD_TAMANO: " + String(tamano / 1024.0, 1) + " KB");
    } else {
      Serial.println("No entiendo el comando " + comando + ", escribe AYUDA para obtener la lista de comandos");
    }
  }

  delay(1);
}
