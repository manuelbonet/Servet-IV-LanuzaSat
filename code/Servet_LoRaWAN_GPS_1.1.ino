#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include <CayenneLPP.h>

CayenneLPP lpp(51);

TinyGPSPlus gps;

float latitude = 42;
float longitude = 0;
float altitude;

static const PROGMEM u1_t NWKSKEY[16] = {}; //ASSIGN IT TO YOUR OWN VALUE
static const u1_t PROGMEM APPSKEY[16] = {}; //ASSIGN IT TO YOUR OWN VALUE


static const u4_t DEVADDR = 0; //ASSIGN IT TO YOUR OWN VALUE

void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }


static osjob_t sendjob;

const unsigned TX_INTERVAL = 307;

const lmic_pinmap lmic_pins = {
  .nss = 18,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 23,
  .dio = {26, 33, 32},
};

void onEvent (ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch (ev) {
    case EV_SCAN_TIMEOUT:
      Serial.println(F("EV_SCAN_TIMEOUT"));
      break;
    case EV_BEACON_FOUND:
      Serial.println(F("EV_BEACON_FOUND"));
      break;
    case EV_BEACON_MISSED:
      Serial.println(F("EV_BEACON_MISSED"));
      break;
    case EV_BEACON_TRACKED:
      Serial.println(F("EV_BEACON_TRACKED"));
      break;
    case EV_JOINING:
      Serial.println(F("EV_JOINING"));
      break;
    case EV_JOINED:
      Serial.println(F("EV_JOINED"));
      break;
    case EV_RFU1:
      Serial.println(F("EV_RFU1"));
      break;
    case EV_JOIN_FAILED:
      Serial.println(F("EV_JOIN_FAILED"));
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("EV_REJOIN_FAILED"));
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
      if (LMIC.txrxFlags & TXRX_ACK)
        Serial.println(F("Received ack"));
      if (LMIC.dataLen) {
        Serial.println(F("Received "));
        Serial.println(LMIC.dataLen);
        Serial.println(F(" bytes of payload"));
      }
      
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
      break;
    case EV_LOST_TSYNC:
      Serial.println(F("EV_LOST_TSYNC"));
      break;
    case EV_RESET:
      Serial.println(F("EV_RESET"));
      break;
    case EV_RXCOMPLETE:
      Serial.println(F("EV_RXCOMPLETE"));
      break;
    case EV_LINK_DEAD:
      Serial.println(F("EV_LINK_DEAD"));
      break;
    case EV_LINK_ALIVE:
      Serial.println(F("EV_LINK_ALIVE"));
      break;
    default:
      Serial.println(F("Unknown event"));
      break;
  }
}

void do_send(osjob_t* j) {
  if (LMIC.opmode & OP_TXRXPEND) {
    Serial.println(F("OP_TXRXPEND, not sending"));
  } else {
    printData();
    LMIC_setTxData2(1, lpp.getBuffer(), lpp.getSize(), 0);

    Serial.println(F("Packet queued"));
  }
}

void setup() {
  Serial.begin(115200);

  Serial1.begin(9600, SERIAL_8N1, 12, 15);

  Serial.println(F("Starting"));

#ifdef VCC_ENABLE
  pinMode(VCC_ENABLE, OUTPUT);
  digitalWrite(VCC_ENABLE, HIGH);
  delay(1000);
#endif

  os_init();

  LMIC_reset();

#ifdef PROGMEM
  uint8_t appskey[sizeof(APPSKEY)];
  uint8_t nwkskey[sizeof(NWKSKEY)];
  memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
  memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
  LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
#else
  LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
#endif

#if defined(CFG_eu868)
  LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
  LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);
  LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
  LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
  LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
  LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
  LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
  LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
  LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);
#elif defined(CFG_us915)
  LMIC_selectSubBand(1);
#endif

  LMIC_setLinkCheckMode(0);

  LMIC.dn2Dr = DR_SF9;

  LMIC_setDrTxpow(DR_SF11, 14);

  do_send(&sendjob);
}

void loop() {

  while (Serial1.available()) {
    gps.encode(Serial1.read());
  }

  os_runloop_once();
}


void printData()
{

  latitude = gps.location.lat();
  longitude = gps.location.lng();
  altitude = gps.altitude.meters();


  lpp.reset();

  if (latitude != 0 || longitude != 0) {
    lpp.addGPS(1, latitude, longitude, altitude);
    lpp.addAnalogInput(2, gps.altitude.meters());


  }

  else {
    lpp.addAnalogInput(2, gps.altitude.meters());

  }


  Serial.print(F("Latitude: "));
  Serial.println(gps.location.lat());

  Serial.print(F("Longitude: "));
  Serial.println(gps.location.lng());

  Serial.print(F("Altitude: "));
  Serial.println(gps.altitude.meters());

  Serial.println();

}
