#include "Adafruit_FONA.h"
#include <SoftwareSerial.h>

#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 4
#define WATERTEMP    avg3(A0)

// Change this for each well
const int well = 1;
const int readingIntervalSec = 3;
const int intervalsPerPost = 3;

String httpPOSTData;
// this is a large buffer for replies
char replyBuffer[255];

// We default to using software serial. If you want to use hardware serial
// (because softserial isnt supported) comment out the following three lines
// and uncomment the HardwareSerial line
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;
// Hardware serial is also possible!
//  HardwareSerial *fonaSerial = &Serial1;

// Use this for FONA 800 and 808s
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
// Use this one for FONA 3G
//Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);
uint8_t type;
int cnt = 0;

int avg3(int in) {
  int avg = analogRead(in);
  avg = avg + analogRead(in);
  avg = avg + analogRead(in);
  return(avg/3);
}

//Get temp as a float
float tempfloat(int t) {
  return(t*110.0/1023.0);
}

//Get temp as a string
String tempStr(int t) {
  long int centigrade;
  centigrade = t*1100L/1023L;
  int degrees = (int) centigrade/10L;
  int tenths = (int) centigrade % 10L;
  String degStr = String(degrees);
  String tenthsStr = String(tenths);
  String per = ".";
  return(degStr + per + tenthsStr);
}

void setup() {
  // For temp sensor
  analogReference(INTERNAL);
  while (!Serial);
  Serial.begin(115200);
  Serial.println(F("FONA test sending temp data"));
  Serial.println(F("Initializing....(May take 3 seconds)"));

  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
    while (1);
  }
  type = fona.type();
  Serial.println(F("FONA is OK"));
  Serial.print(F("Found "));

  // TURN ON GPRS
  if (!fona.enableGPRS(true)) {
    Serial.println(F("Failed to turn on"));
  }

  // ENABLE NTP TIME SYNC
  if (!fona.enableNTPTimeSync(true, F("pool.ntp.org"))) {
    Serial.println(F("Failed to enable"));
  }
  // Optionally configure HTTP gets to follow redirects over SSL.
  // Default is not to follow SSL redirects, however if you uncomment
  // the following line then redirects over SSL will be followed.
  //fona.setHTTPSRedirect(true);
}

// Use this function and comment out other loop to stop operation of Arduino

//void loop() {
//  delay(3600000);
//  Serial.println("Another hour has passed");
//  }

void loop() {
  if (cnt == 0) {
    httpPOSTData = F("{\"well\":1,\"reading_interval_s\":");
    httpPOSTData += readingIntervalSec;
    // GET BATTERY DATA
    // read the ADC
    uint16_t adc;
    if (! fona.getADCVoltage(&adc)) {
      Serial.println(F("Failed to read ADC"));
    } else {
      Serial.print(F("ADC = ")); Serial.print(adc); Serial.println(F(" mV"));
      httpPOSTData += F(",\"adc_voltage_mv\":");
      httpPOSTData += adc;
      Serial.println(httpPOSTData);
    }

    // read the battery voltage and percentage
    uint16_t vbat;
    if (! fona.getBattVoltage(&vbat)) {
      Serial.println(F("Failed to read Batt"));
    } else {
      Serial.print(F("VBat = ")); Serial.print(vbat); Serial.println(F(" mV"));
      httpPOSTData += F(",\"batt_voltage_mv\":");
      httpPOSTData += vbat;
      Serial.println(httpPOSTData);
    }


    if (! fona.getBattPercent(&vbat)) {
      Serial.println(F("Failed to read Batt"));
    } else {
      Serial.print(F("VPct = ")); Serial.print(vbat); Serial.println(F("%"));
      httpPOSTData += F(",\"batt_percent_charged\":");
      httpPOSTData += vbat;
      Serial.println(httpPOSTData);
    }

  // READ THE TIME
    char startBuffer[23];
    fona.getTime(startBuffer, 23);  // make sure replyBuffer is at least 23 bytes!
    String startTime = startBuffer;
    startTime = startTime.substring(1, startTime.length() - 4);
    startTime.replace("/","-");
    startTime.replace(",","T");
    startTime = "20" + startTime;
    Serial.print(F("Start Time = ")); Serial.println(startTime);
    httpPOSTData += F(",\"startTime\":\"");
    httpPOSTData += startTime;
    httpPOSTData += F("\"");
    httpPOSTData += F(",\"temp_readings_c\":\"");
  }

  delay(1000 * readingIntervalSec);
  cnt++;
  Serial.println(cnt);
  // Check temp, add it to object
  httpPOSTData += tempStr(WATERTEMP);

  if(cnt < intervalsPerPost) {
    httpPOSTData += F("|");
  }
  Serial.println(httpPOSTData);
  // If intervalsPerPost time intervals have passed, post the data and reset the counter
  if(cnt >= intervalsPerPost) {
    //RECORD THE END TIME
    char endBuffer[23];
    fona.getTime(endBuffer, 23);  // make sure replyBuffer is at least 23 bytes!
    String endTime = endBuffer;
    endTime = endTime.substring(1, endTime.length() - 4);
    endTime.replace("/","-");
    endTime.replace(",","T");
    endTime = "20" + endTime;
    Serial.print(F("End Time = ")); Serial.println(endTime);
    httpPOSTData += F("\",\"endTime\":\"");
    httpPOSTData += endTime;
    // httpPOSTData += F("2016-09-19T17:57:49");
    httpPOSTData += F("\"}");
    delay(1000);
    Serial.println("DATA TO POST");
    Serial.println(httpPOSTData);
    uint16_t statuscode;
    int16_t length;
    // char url[80] = "wellcom-staging.herokuapp.com/api/device_data/";
    // char url[80] = "wellcom-staging.herokuapp.com/api/test/";
    char url[80] = "wellcom-staging.herokuapp.com/api/device_output/";
    char charBuf[500];
    httpPOSTData.toCharArray(charBuf, 500);

    Serial.println(F("****"));
    if (!fona.HTTP_POST_start(url, F("application/json"), (char *) charBuf, strlen(charBuf), &statuscode, (uint16_t *)&length)) {
      Serial.println("Failed!");
//              break;
    }
    while (length > 0) {
      while (fona.available()) {
        char c = fona.read();
        #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
          loop_until_bit_is_set(UCSR0A, UDRE0); /* Wait until data register empty. */
          UDR0 = c;
        #else
          Serial.write(c);
        #endif

          length--;
          if (! length) break;
      }
  }
  Serial.println(F("\n****"));
  fona.HTTP_POST_end();
  cnt = 0;
  }
}
