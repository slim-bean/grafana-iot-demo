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
#include <esp_task_wdt.h>
#include <M5Unified.h>
#include "M5UnitENV.h"
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
SHT3X sht3x;
QMP6988 qmp;

// Create a write request for 2 series.
WriteRequest req(5, 1024);

TimeSeries temp(1, "temperature_f", "{location=\"281\"}");
TimeSeries humidity(1, "humidity", "{location=\"281\"}");
TimeSeries pressure(1, "pressure", "{location=\"281\"}");
TimeSeries iUSB(1, "current", "{location=\"281\"}");
TimeSeries vBat(1, "vbat", "{location=\"281\"}");

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

    Serial.begin(115200);

    // Start the watchdog timer, sometimes connecting to wifi or trying to set the time can fail in a way that never recovers
    esp_task_wdt_config_t config = {
        .timeout_ms = 300000,
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&config);
    esp_task_wdt_add(NULL);

    // Wait 5s for serial connection or continue without it
    // some boards like the esp32 will run whether or not the
    // serial port is connected, others like the MKR boards will wait
    // for ever if you don't break the loop.
    uint8_t serialTimeout = 0;
    while (!Serial && serialTimeout < 50)
    {
        delay(100);
        serialTimeout++;
    }

    M5.begin();               // Init M5StickCPlus.  
    M5.Lcd.setRotation(3);    // Rotate the screen.  
    M5.Lcd.fillScreen(BLACK); // Fill the screen with black background

    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.printf("==  Grafana Labs ==");

    if (!qmp.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, 32, 33, 400000U)) {
        Serial.println("Couldn't find QMP6988");
        while (1) delay(1);
    }

    if (!sht3x.begin(&Wire, SHT3X_I2C_ADDR, 32, 33, 400000U)) {
        Serial.println("Couldn't find SHT3X");
        while (1) delay(1);
    }

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
    req.addTimeSeries(vBat);
    req.setDebug(Serial);

    esp_task_wdt_reset();
}

// ===================================================
// Loop continiously until forever,
// reading from both sensors, and submitting to your
// Grafana Cloud account for visualisation
// ===================================================
void loop() {
    uint64_t start = millis();
    int64_t time;
    time = transport.getTimeMillis();
    M5.update();

    // Get new updated values from our sensor
    if (sht3x.update()) {
        Serial.println("-----SHT3X-----");
        Serial.print("Temperature: ");
        Serial.print(sht3x.cTemp);
        Serial.println(" degrees C");
        tmp = sht3x.cTemp;
        tmp = tmp * 1.8 + 32;
        Serial.print("Humidity: ");
        Serial.print(sht3x.humidity);
        Serial.println("% rH");
        hum = sht3x.humidity;
        Serial.println("-------------\r\n");
    }
    if (qmp.update()) {
        Serial.println("-----QMP6988-----");
        Serial.print(F("Temperature: "));
        Serial.print(qmp.cTemp);
        Serial.println(" *C");
        Serial.print(F("Pressure: "));
        Serial.print(qmp.pressure);
        Serial.println(" Pa");
        press = qmp.pressure;
        Serial.print(F("Approx altitude: "));
        Serial.print(qmp.altitude);
        Serial.println(" m");
        Serial.println("-------------\r\n");
    }

    // Update the LCD screen
    M5.Lcd.fillRect(00, 40, 100, 60, BLACK);  // Fill the screen with black (to clear the screen).
    M5.Lcd.setCursor(0, 40);
    M5.Lcd.printf("  Temp: %2.1f  \r\n  Humi: %2.0f%%  \r\n  Pressure:%2.0f hPa\r\n", tmp, hum, press / 100);

    Serial.printf("\r\n====================================\r\n");
    Serial.printf("Temp: %2.1f °F \r\nHumi: %2.0f%%  \r\nPressure:%2.0f hPa\r\n", tmp, hum, press / 100);

    // Serial.println("\r\n");
    // int Iusb = M5.Axp.GetIdischargeData() * 0.375;
    // Serial.printf("Iusbin:%da\r\n", Iusb);

    // int disCharge = M5.Axp.GetIdischargeData() / 2;
    // Serial.printf("disCharge:%dma\r\n", disCharge);

    // double Iin = M5.Axp.GetIinData() * 0.625;
    // Serial.printf("Iin:%.3fmA\r\n", Iin);
    double Iin = M5.Power.Axp192.getBatteryDischargeCurrent();

    // int BatTemp = M5.Axp.GetTempData() * 0.1 - 144.7;
    // Serial.printf("Battery temperature:%d\r\n", BatTemp);

    // int Vaps = M5.Axp.GetVapsData();
    // Serial.printf("battery capacity :%dmW\r\n", Vaps);

    // int bat = M5.Axp.GetPowerbatData() * 1.1 * 0.5 / 1000;
    // Serial.printf("battery power:%dmW\r\n", bat);

    // int charge = M5.Axp.GetIchargeData() / 2;
    // Serial.printf("icharge:%dmA\r\n", charge);

    // double vbat = M5.Axp.GetVbatData() * 1.1 / 1000;
    // Serial.printf("vbat:%.3fV\r\n", vbat);
    double vbat = M5.Power.Axp192.getBatteryVoltage();


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
    if (!iUSB.addSample(time, Iin)) {
        Serial.println(iUSB.errmsg);
    }
    if (!vBat.addSample(time, vbat)) {
        Serial.println(vBat.errmsg);
    }

    for (uint8_t i = 0; i <= 5; i++)
    {
        Serial.println("Sending to Grafana Cloud");
        PromClient::SendResult res = client.send(req);
        if (!res == PromClient::SendResult::SUCCESS) {
            Serial.println(client.errmsg);
            delay(250);
        } else {
            esp_task_wdt_reset();
            // Batches are not automatically reset so that additional retry logic could be implemented by the library user.
            // Reset batches after a succesful send.
            temp.resetSamples();
            humidity.resetSamples();
            pressure.resetSamples();
            iUSB.resetSamples();
            vBat.resetSamples();
            uint32_t diff = millis() - start;
            Serial.print("Prom send succesful in ");
            Serial.print(diff);
            Serial.println("ms");
            break;
        }
    }


    uint64_t delayms = 15000 - (millis() - start);
    // If the delay is longer than 5000ms we likely timed out and the send took longer than 5s so just send right away.
    if (delayms > 15000)
    {
        delayms = 0;
    }
    Serial.print("Sleeping ");
    Serial.print(delayms);
    Serial.println("ms");
    delay(delayms);
}