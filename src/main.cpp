// Written for Grafana Labs to demonstrate how to use the M5Stick CPlus with Grafana Cloud
// 2023/01/03
// Willie Engelbrecht - willie.engelbrecht@grafana.com
// Introduction to time series: https://grafana.com/docs/grafana/latest/fundamentals/timeseries/
// Grafana and InfluxDB proxy: https://grafana.com/docs/grafana-cloud/data-configuration/metrics/metrics-influxdb/push-from-telegraf/
// All the battery API documentation: https://docs.m5stack.com/en/api/stickc/axp192_m5stickc


// ===================================================
// Includes - no need to change anything here
// ===================================================
#include <Arduino.h>
#include <M5StickCPlus.h>
#include "M5_ENV.h"
#include <PromLokiTransport.h>
#include <PrometheusArduino.h>

// ===================================================
// All the things that needs to be changed 
// Your local WiFi details
// Your Grafana Cloud details
// ===================================================
#include "config.h"
#include "certificates.h"


// ===================================================
// Global Variables
// ===================================================
PromLokiTransport transport;
PromClient client(transport);
SHT3X sht30;
QMP6988 qmp6988;

// Create a write request for 2 series.
WriteRequest req(5, 1024);

TimeSeries temp(1, "temperature", "{location=\"office\"}");
TimeSeries humidity(1, "temperature", "{location=\"office\"}");
TimeSeries pressure(1, "temperature", "{location=\"office\"}");
TimeSeries iUSB(1, "current", "{location=\"office\",type=\"usb\"}");
TimeSeries iIn(1, "current", "{location=\"office\",type=\"in\"}");

//TODO add more values (make sure to increase WriteRequest above and add the series in the end of setup() below )


float tmp = 0.0;
float hum = 0.0;
float press = 0.0;
/*
POSTtext = "m5stick Iusb=" + String(Iusb)
            + ",disCharge=" + String(disCharge)
            + ",Iin=" + String(Iin)
            + ",BatTemp=" + String(BatTemp)
            + ",Vaps=" + String(Vaps)
            + ",bat=" + String(bat)
            + ",charge=" + String(charge)
            + ",vbat=" + String(vbat);
*/

// ===================================================
// Set up procedure that runs once when the controller starts
// ===================================================
void setup() {
    M5.begin();               // Init M5StickCPlus.  
    M5.Lcd.setRotation(3);    // Rotate the screen.  
    M5.lcd.fillScreen(BLACK); // Fill the screen with black background

    M5.lcd.setTextSize(2);
    M5.lcd.setCursor(10, 10);
    M5.lcd.printf("==  Grafana Labs ==");

    Wire.begin(32, 33);       // Wire init, adding the I2C bus.  
    qmp6988.init();           // Initiallize the pressure sensor

    // Configure and start the transport layer
    transport.setUseTls(true);
    transport.setCerts(grafanaCert, strlen(grafanaCert));
    transport.setWifiSsid(WIFI_SSID);
    transport.setWifiPass(WIFI_PASSWORD);
    transport.setDebug(Serial);  // Remove this line to disable debug logging of the client.
    if (!transport.begin()) {
        Serial.println(transport.errmsg);
        while (true) {};
    }

    // Configure the client
    client.setUrl(GC_URL);
    client.setPath((char*)GC_PATH);
    client.setPort(GC_PORT);
    client.setUser(GC_USER);
    client.setPass(GC_PASS);
    client.setDebug(Serial);  // Remove this line to disable debug logging of the client.
    if (!client.begin()) {
        Serial.println(client.errmsg);
        while (true) {};
    }

    // Add our TimeSeries to the WriteRequest
    req.addTimeSeries(temp);
    req.addTimeSeries(humidity);
    req.addTimeSeries(pressure);
    req.addTimeSeries(iUSB);
    req.addTimeSeries(iIn);
    req.setDebug(Serial);


}

// ===================================================
// Loop continiously until forever,
// reading from both sensors, and submitting to your
// Grafana Cloud account for visualisation
// ===================================================
void loop() {

    int64_t time;
    time = transport.getTimeMillis();

    // Get new updated values from our sensor
    press = qmp6988.calcPressure();
    if (sht30.get() == 0) {     // Obtain the data of sht30.  
        tmp = sht30.cTemp;      // Store the temperature obtained from sht30.                             
        hum = sht30.humidity;   // Store the humidity obtained from the sht30.
    } else {
        tmp = 0, hum = 0;
    }

    // Update the LCD screen
    M5.lcd.fillRect(00, 40, 100, 60, BLACK);  // Fill the screen with black (to clear the screen).
    M5.lcd.setCursor(0, 40);
    M5.Lcd.printf("  Temp: %2.1f  \r\n  Humi: %2.0f%%  \r\n  Pressure:%2.0f hPa\r\n", tmp, hum, press / 100);

    Serial.printf("\r\n====================================\r\n");
    Serial.printf("Temp: %2.1f °C \r\nHumi: %2.0f%%  \r\nPressure:%2.0f hPa\r\n", tmp, hum, press / 100);

    Serial.println("\r\n");
    int Iusb = M5.Axp.GetIdischargeData() * 0.375;
    Serial.printf("Iusbin:%da\r\n", Iusb);

    int disCharge = M5.Axp.GetIdischargeData() / 2;
    Serial.printf("disCharge:%dma\r\n", disCharge);

    double Iin = M5.Axp.GetIinData() * 0.625;
    Serial.printf("Iin:%.3fmA\r\n", Iin);

    int BatTemp = M5.Axp.GetTempData() * 0.1 - 144.7;
    Serial.printf("Battery temperature:%d\r\n", BatTemp);

    int Vaps = M5.Axp.GetVapsData();
    Serial.printf("battery capacity :%dmW\r\n", Vaps);

    int bat = M5.Axp.GetPowerbatData() * 1.1 * 0.5 / 1000;
    Serial.printf("battery power:%dmW\r\n", bat);

    int charge = M5.Axp.GetIchargeData() / 2;
    Serial.printf("icharge:%dmA\r\n", charge);

    double vbat = M5.Axp.GetVbatData() * 1.1 / 1000;
    Serial.printf("vbat:%.3fV\r\n", vbat);


    // Load the new values
    if (!pressure.addSample(time, press)) {
        Serial.println(pressure.errmsg);
    }
    if (!temp.addSample(time, tmp)) {
        Serial.println(temp.errmsg);
    }
    if (!humidity.addSample(time, hum)) {
        Serial.println(humidity.errmsg);
    }
    if (!iUSB.addSample(time, Iusb)) {
        Serial.println(iUSB.errmsg);
    }
    if (!iIn.addSample(time,Iin)) {
        Serial.println(iIn.errmsg);
    }


    Serial.println("Sending to Grafana Cloud");
    PromClient::SendResult res = client.send(req);
    if (!res == PromClient::SendResult::SUCCESS) {
        Serial.println(client.errmsg);
        // Note: additional retries or error handling could be implemented here.
        // the result could also be:
        // PromClient::SendResult::FAILED_DONT_RETRY
        // PromClient::SendResult::FAILED_RETRYABLE
    }
    // Batches are not automatically reset so that additional retry logic could be implemented by the library user.
    // Reset batches after a succesful send.
    temp.resetSamples();
    humidity.resetSamples();
    pressure.resetSamples();
    iUSB.resetSamples();
    iIn.resetSamples();

    // Sleep for 5 seconds
    delay(5000);
}