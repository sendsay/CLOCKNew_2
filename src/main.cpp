/*_______By IvanUA ed. by Sendsay______________
 Піни LED------NodeMCU  1.0                    |
       DataIn__________________D7/GPIO 13      |
       LOAD/CS_________________D8/GPIO 15      |
       CLK_____________________D5/GPIO 14      |
 Кнопка________________________D0/GPIO 16      |
 Бaзер_________________________D4/GPIO 12      |
 DS18B20_______________________D3/GPIO 0       |
 Si7021/BMP/BME280/DS3231 DA___D2/GPIO 4       |
 Si7021/BMP/BME280/DS3231 CL___D1/GPIO 5       |
 GND - FotoRezistor ___________A0              |
 +3,3 - rezistor 2-100k _______A0              |
 DHT22_________________________D4/GPIO 2       |
______________________________________________*/



// TODO: Use for transmit data aTalkArduino
// TODO: Add get weather on tomorrow == НЕ ВЫПОНИМО ПОКА
// TODO: Add pomodoro technics == Пока нет нужды


#include <Arduino.h>
#include <main.h>

#include <max7219.h>
#include <fonts.h>

#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <Adafruit_Si7021.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <math.h>
#include <FS.h>

#include <T_ukr.h>
#include <T_en.h>
#include <T_rus.h>

//=====================================================================================================================================
IPAddress apIP(192, 168, 4, 1); // Адресс для точки доступа
WiFiUDP ntpUDP;                 // Клиент для получения NTP
IPAddress timeServerIP;         // ??
Adafruit_BMP280 bmp;            // Датчик bmp
Adafruit_Si7021 sensor = Adafruit_Si7021();     // Датчик SI
HTTPClient client;              // Клиент для погоды
WiFiClient ESPclient;           // Клиент подключения к ВАЙФАЙ
PubSubClient MQTTclient(ESPclient); // Клиент MQTT
Ticker ChangeMode(SwitchShowMode, 2*1000);          // Таймер переключения режимов отображение данных с датчиков

ESP8266WebServer server(80);    // Веб сервер

/*
..######..########.########.##.....##.########.
.##....##.##..........##....##.....##.##.....##
.##.......##..........##....##.....##.##.....##
..######..######......##....##.....##.########.
.......##.##..........##....##.....##.##.......
.##....##.##..........##....##.....##.##.......
..######..########....##.....#######..##.......
*/
void setup() {

    Serial.begin(57600);

    SPIFFS.begin();

    loadConfig("/config.json", config);     // загрузка конфига

    pinMode(buzzerPin, OUTPUT);             // Выход сигнала буззера
    pinMode(lightPin, OUTPUT);              // Выход мигалки

    PRN("");
    PRN("");
    PRN("============>START!");

    initMAX7219();                          // Инициализация ЛЕД панели
    sendCmdAll(CMD_SHUTDOWN, 1);            // Сброс панели
    sendCmdAll(CMD_INTENSITY, config.manualBright);          // Установка яркости

    if(bmp.begin(0x76)) {                   // Инициализация датчика bmp
        PRN("============> YES!!! find BMP280 sensor!");
        bmp280 = true;
        sensorsBmp();
    } else {
        PRN("============> Did not find BMP280 sensor!");
    }

    if (sensor.begin()) {                   // Инициализация датчика Si7021
        PRN("============> YES!!! find Si7021 sensor!");
        si7021 = true;
        sensorsSi7021();
    } else {
        PRN("============> Did not find Si7021 sensor!");
    }

    wifiConnect();                          // подключение к Вайфай

    localMillisAtUpdate = millis();
    localEpoc = (hour * 60 * 60 + minute * 60 + second);

    if(WiFi.status() == WL_CONNECTED) {
        ntpUDP.begin(localPort);            // Запуск UDP для получения времени
        timeUpdateNTP();                    // Обновление времени
        if (config.weatherOn == 1) {
            getWeather();                   // Получение данныз прогноза погоды
        }
    }

    if (config.mqttOn == 1) {
        MQTTclient.setServer(config.mqttserver, config.mqttport);
        MQTTclient.setCallback(callback);
        MQTTclient.connect(config.mqttname);
        MQTTclient.subscribe(config.mqttsubinform);
        MQTTclient.subscribe(config.mqttsub);
    }

    server.begin();
    server.on("/", fileindex);
    server.on("/index.html", fileindex);
    server.on("/bootstrap.min.css", bootstrap);
    server.on("bootstrap.min.css", bootstrap);
    server.on("popper.min.js", popper);
    server.on("/popper.min.js", popper);
    server.on("/bootstrap.min.js", bootstrapmin);
    server.on("bootstrap.min.js", bootstrapmin);
    server.on("jquery.min.js", jquery);
    server.on("/jquery.min.js", jquery);
    server.on("script.js", jscript);
    server.on("/script.js", jscript);
    server.on("style.css", style);
    server.on("/style.css", style);
    server.on("/getData", sendData);
    server.on("/logo.png", logo);
    server.on("logo.png", logo);
    server.on("/saveContent", saveContent);
    server.on("/restart", restart);
    printFile("/config.json");
}

/*
.##........#######...#######..########.
.##.......##.....##.##.....##.##.....##
.##.......##.....##.##.....##.##.....##
.##.......##.....##.##.....##.########.
.##.......##.....##.##.....##.##.......
.##.......##.....##.##.....##.##.......
.########..#######...#######..##.......
*/
void loop() {
    ChangeMode.update();                    // Обновление таймера отображения данных из датчиков
    server.handleClient();                  // Работа Веб страницы

//=== Мигалка =====================================================
    digitalWrite(lightPin, (((second % 2) == 0) ? HIGH : LOW));

//=== Работа с временем, поднимем флаг каждую секунду ===================================
    if(second != lastSecond) {     // счетчик секунд и флаг для процессов                                            // на початку нової секунди скидаємо secFr в "0"
        lastSecond = second;
        secFr = 0;                          // флаг для процессов
    } else {
        secFr++;
    }

//=== Обновление датчиков каждую минуту =============================================================
    if ((second == 0) and (not secFr)) {
        updateSensors();
    }

//=== Обновление погоды с сайта каждые 29 минут ==============================================================
    if (((minute == 29 ) and (second == 0) and (not secFr)) && config.weatherOn == 1) {
        if (WIFI_connected) {
            getWeather();
        } else {
            PRN("============> Can't get Weather, check WiFi!!!");
        }
    }

//=== Обновление переменных времени, ход часов ==========================================
    updateTime();

//===Основной цикл отображения ==========================================
    if (!ShowFlag) {
        showAnimClock();                        // Вывод времени на часы
    }

//=== Сигнал каждый час и обновление времени ==========================================
    if ((minute == 0) and (second == 0) and (secFr == 0) and (hour >= config.timeSigOn) and (hour <= config.timeSigOff)) {
        PRN("============> BIP!!!");
        bip();
        bip();

        if (WIFI_connected) {
           getNTPtime();               // ***** Получение времени из интернета
        }
    }

/*
.########..##.....##.########.########..#######..##....##
.##.....##.##.....##....##.......##....##.....##.###...##
.##.....##.##.....##....##.......##....##.....##.####..##
.########..##.....##....##.......##....##.....##.##.##.##
.##.....##.##.....##....##.......##....##.....##.##..####
.##.....##.##.....##....##.......##....##.....##.##...###
.########...#######.....##.......##.....#######..##....##
*/
    if (((digitalRead(buttonPin) == HIGH) ||
        ((((minute % 15) == 0) && (second == 3)) && ((hour >= config.timeSigOn) and (hour <= config.timeSigOff))) ||
        (ShowFlagMQTT == true)) && ShowFlag == false) {
        ChangeMode.start();
        ShowFlag = true;
        showSimpleTemp();
    }

/*
..######..##.....##.########..######..##....##....##......##.####.########.####
.##....##.##.....##.##.......##....##.##...##.....##..##..##..##..##........##.
.##.......##.....##.##.......##.......##..##......##..##..##..##..##........##.
.##.......#########.######...##.......#####.......##..##..##..##..######....##.
.##.......##.....##.##.......##.......##..##......##..##..##..##..##........##.
.##....##.##.....##.##.......##....##.##...##.....##..##..##..##..##........##.
..######..##.....##.########..######..##....##.....###..###..####.##.......####
*/
    if ((minute % 5 == 0) && (second == 0) && (secFr == 0) && (WiFi.status() != WL_CONNECTED || !WIFI_connected)) {
        WIFI_connected = false;

        if (secFr == 0) DEBUG("============> Check WIFI connect!!!");
        
        wifiConnect();
    }
/*
.##.....##..#######..########.########
.###...###.##.....##....##.......##...
.####.####.##.....##....##.......##...
.##.###.##.##.....##....##.......##...
.##.....##.##..##.##....##.......##...
.##.....##.##....##.....##.......##...
.##.....##..#####.##....##.......##...
*/
// ---------- 50 сек. перевірка доступності MQTT та публікація температури ---------
    if (second == 50 && config.mqttOn && !alarm_stat && WIFI_connected && secFr == 0) {
        if (WiFi.status() != WL_CONNECTED) {
                WIFI_connected = false;
        }
        if (!MQTTclient.connected() && WIFI_connected) {
                reconnect();
        }
        if (MQTTclient.connected() && WIFI_connected) {
            if (bmp280) MQTTclient.publish(config.mqttpubtemp, (String(t1) + "." + String(t2)).c_str());
            if (bmp280) MQTTclient.publish(config.mqttpubpress, (String(pressBmp).c_str()));
            if(si7021) MQTTclient.publish(config.mqttpubtempUl, (String(t3) + "." + String(t4)).c_str());
            if(si7021) MQTTclient.publish(config.mqttpubhum, (String(humSi7021)).c_str());
        }
        MQTTclient.publish(config.mqttpubforecast, String(config.mqttforecast).c_str());

        if (not secFr) {
            printTime();
            Serial.print("Publish in topic ");
            if(si7021) Serial.print("Temperature: " + String(t1) + "." + String(t2) + "*C,   ");
            if(bmp280) Serial.print("Na ulice: " + String(t3) + "." + String(t4) + "*C,   ");
            if(humSi7021 != 0) Serial.print("Humidity: " + String(humSi7021) + " %,  ");
            if(pressBmp != 0) Serial.print("  Pressure: " + String(pressBmp) + " mmHg,  Altitude: " + String(altBmp) + " m.");
            Serial.println("");
        }
    }

// ---------- якщо мережа WiFi доступна то виконуємо наступні функції ----------------------------
    if(WIFI_connected){
        if(config.mqttOn) MQTTclient.loop();           // перевіряємо чи намає вхідних повідомлень, як є, то кoлбек функція
    }

// ---------- керування яскравосттю экрану ----------------------------
    if (config.autoBright == 1 && secFr == 0) {
        sendCmdAll(CMD_INTENSITY, map(analogRead(PIN_A0), 0, 1023, 0, 15));
    }
}

/*
.########.##....##.########.....##........#######...#######..########.
.##.......###...##.##.....##....##.......##.....##.##.....##.##.....##
.##.......####..##.##.....##....##.......##.....##.##.....##.##.....##
.######...##.##.##.##.....##....##.......##.....##.##.....##.########.
.##.......##..####.##.....##....##.......##.....##.##.....##.##.......
.##.......##...###.##.....##....##.......##.....##.##.....##.##.......
.########.##....##.########.....########..#######...#######..##.......
*/


/*
.##......##.########.########..####.##....##.########.########.########..########....###.....######..########
.##..##..##.##.......##.....##..##..###...##....##....##.......##.....##.##.........##.##...##....##.##......
.##..##..##.##.......##.....##..##..####..##....##....##.......##.....##.##........##...##..##.......##......
.##..##..##.######...########...##..##.##.##....##....######...########..######...##.....##.##.......######..
.##..##..##.##.......##.....##..##..##..####....##....##.......##...##...##.......#########.##.......##......
.##..##..##.##.......##.....##..##..##...###....##....##.......##....##..##.......##.....##.##....##.##......
..###..###..########.########..####.##....##....##....########.##.....##.##.......##.....##..######..########
*/
    void fileindex() {
        File file = SPIFFS.open("/index.html.gz", "r");
        size_t sent = server.streamFile(file, "text/html");
    }
    void bootstrap() {
        File file = SPIFFS.open("/bootstrap.min.css.gz", "r");
        size_t sent = server.streamFile(file, "text/css");
    }
    void popper() {
        File file = SPIFFS.open("/popper.min.js.gz", "r");
        size_t sent = server.streamFile(file, "applicat ion/javascript");
    }
    void bootstrapmin() {
        File file = SPIFFS.open("/bootstrap.min.js.gz", "r");
        size_t sent = server.streamFile(file, "application/javascript");}

    void jquery() {
        File file = SPIFFS.open("/jquery.min.js.gz", "r");
        size_t sent = server.streamFile(file, "application/javascript");
    }
    void jscript() {
        File file = SPIFFS.open("/script.js.gz", "r");
        size_t sent = server.streamFile(file, "application/javascript");
    }

    void style() {
        File file = SPIFFS.open("/style.css.gz", "r");
        size_t sent = server.streamFile(file, "text/css");
    }

    void logo() {
        File file = SPIFFS.open("/logo.png.gz", "r");
        size_t sent = server.streamFile(file, "image/png");
    }

    void sendData() {
        PRN("sending Data in WEB!");

        String json = "{";
        //wifi
        json += "\"ssid\":\"";
        json += config.ssid;
        json += "\",\"password\":\"";
        json += config.password;
        json += "\",\"ssidAP\":\"";
        json += config.ssidAP;
        json += "\",\"passwordAP\":\"";
        json += config.passwordAP;
        //Time
        json += "\",\"timezone\":\"";
        json += config.timeZone;
        json += "\",\"summertime\":\"";
        json += config.summertime;
        json += "\",\"sigOn\":\"";
        json += config.timeSigOn;
        json += "\",\"sigOff\":\"";
        json += config.timeSigOff;
        json += "\",\"ntpServerName\":\"";
        json += config.ntpServerName;
        //weather
        json += "\",\"apiKey\":\"";
        json += config.apiKey;
        json += "\",\"cityId\":\"";
        json += config.cityId;
        json += "\",\"weatherServer\":\"";
        json += config.weatherServer;
        json += "\",\"langWeather\":\"";
        json += config.langWeather;
        json += "\",\"mqttserver\":\"";
        json += config.mqttserver;
        json += "\",\"mqttport\":\"";
        json += config.mqttport;
        json += "\",\"mqttUserName\":\"";
        json += config.mqttUserName;
        json += "\",\"mqttpass\":\"";
        json += config.mqttpass;
        json += "\",\"mqttname\":\"";
        json += config.mqttname;
        json += "\",\"mqttsubinform\":\"";
        json += config.mqttsubinform;
        json += "\",\"mqttsub\":\"";
        json += config.mqttsub;
        json += "\",\"mqttpubtemp\":\"";
        json += config.mqttpubtemp;
        json += "\",\"mqttpubtempUl\":\"";
        json += config.mqttpubtempUl;
        json += "\",\"mqttpubhum\":\"";
        json += config.mqttpubhum;
        json += "\",\"mqttpubpress\":\"";
        json += config.mqttpubpress;
        // json += "\",\"mqttpubalt\":\"";
        // json += config.mqttpubalt;
        json += "\",\"mqttpubforecast\":\"";
        json += config.mqttpubforecast;
        json += "\",\"mqttbutt\":\"";
        json += config.mqttbutt;
        json += "\",\"mqttOn\":\"";
        json += config.mqttOn;
        json += "\",\"weatherOn\":\"";
        json += config.weatherOn;
        json += "\",\"autoBright\":\"";
        json += config.autoBright;
        json += "\",\"scrollDelay\":\"";
        json += config.scrollDelay;
        json += "\",\"manualBright\":\"";
        json += config.manualBright;

        json += "\"}";

        server.send (200, "text/json", json);
        Serial.println(json);
    }

    void saveContent() {
        PRN("save Content!!!");

        //Wifi
        server.arg("ssid").toCharArray(config.ssid, 50) ;
        server.arg("password").toCharArray(config.password, 50) ;
        server.arg("ssidAP").toCharArray(config.ssidAP, 50) ;
        server.arg("passwordAP").toCharArray(config.passwordAP, 50) ;

        //Time
        config.timeZone = server.arg("timezone").toFloat();
        config.summertime = server.arg("summertime").toInt();
        config.timeSigOn = server.arg("sigOn").toInt();
        config.timeSigOff = server.arg("sigOff").toInt();
        server.arg("ntpServerName").toCharArray(config.ntpServerName, 50) ;
        //weather
        server.arg("apiKey").toCharArray(config.apiKey, 70) ;
        config.cityId = server.arg("cityId").toInt();
        server.arg("weatherServer").toCharArray(config.weatherServer, 50) ;
        server.arg("langWeather").toCharArray(config.langWeather, 4) ;
        //mqtt
        server.arg("mqttserver").toCharArray(config.mqttserver, 50);
        config.mqttport = server.arg("mqttport").toInt();
        server.arg("mqttUserName").toCharArray(config.mqttUserName, 50);
        server.arg("mqttpass").toCharArray(config.mqttpass, 50) ;
        server.arg("mqttname").toCharArray(config.mqttname, 50) ;
        server.arg("mqttsubinform").toCharArray(config.mqttsubinform, 50) ;
        server.arg("mqttsub").toCharArray(config.mqttsub, 50) ;
        server.arg("mqttpubtemp").toCharArray(config.mqttpubtemp, 50) ;
        server.arg("mqttpubtempUl").toCharArray(config.mqttpubtempUl, 50) ;
        server.arg("mqttpubhum").toCharArray(config.mqttpubhum, 50) ;
        server.arg("mqttpubpress").toCharArray(config.mqttpubpress, 50) ;
        server.arg("mqttpubforecast").toCharArray(config.mqttpubforecast, 50) ;
        server.arg("mqttbutt").toCharArray(config.mqttbutt, 50) ;
        config.mqttOn = server.arg("mqttOn").toInt();
        config.weatherOn = server.arg("weatherOn").toInt();
        config.autoBright = server.arg("autoBright").toInt();
        config.scrollDelay = server.arg("scrollDelay").toInt();
        config.manualBright = server.arg("manualBright").toInt();

        saveConfig("/config.json", config);
    }

    void restart() {
        server.send(200, "text/json", "");
        delay(1000);
        ESP.restart();
    }


/*
..######...#######..##....##.########.####..######..
.##....##.##.....##.###...##.##........##..##....##.
.##.......##.....##.####..##.##........##..##.......
.##.......##.....##.##.##.##.######....##..##...####
.##.......##.....##.##..####.##........##..##....##.
.##....##.##.....##.##...###.##........##..##....##.
..######...#######..##....##.##.......####..######..
*/
    void loadConfig(const char *filename, Config &config) {

        File file = SPIFFS.open("/config.json", "r");

        const size_t capacity = JSON_OBJECT_SIZE(26) + 720;
        DynamicJsonDocument doc(capacity);

        DeserializationError error = deserializeJson(doc, file);
        if (error) {
            PRN(F("Failed to read file, using default configuration"));
            PRN("Error is :");
            PRN(error.c_str());
        }
        //Wifi
        strlcpy(config.ssid, doc["ssid"] | "PUTIN UTELE", sizeof(config.ssid));
        strlcpy(config.password, doc["password"] | "0674788273", sizeof(config.password));
        strlcpy(config.ssidAP, doc["ssidAP"] | "CLOCKat", sizeof(config.ssidAP));
        strlcpy(config.passwordAP, doc["passwordAP"] | "", sizeof(config.passwordAP));
        //Time
        config.timeZone = doc["timezone"] | 2;
        config.summertime = doc["summertime"] | 0;
        strlcpy(config.ntpServerName, doc["ntpServerName"] | "ntp3.time.in.ua", sizeof(config.ntpServerName));
        config.timeSigOn = doc["timeSigOn"] | 7;
        config.timeSigOff = doc["timeSigOff"] | 21;
        //Weather
        strlcpy(config.apiKey, doc["apiKey"] | "df9c74ff1a47dcb48aab814fa5500429", sizeof(config.apiKey));
        config.cityId = doc["cityId"] | 598098;
        strlcpy(config.weatherServer, doc["weatherServer"] | "api.openweathermap.org", sizeof(config.weatherServer));
        strlcpy(config.langWeather, doc["langWeather"] | "ua", sizeof(config.langWeather));
        if (doc["langWeather"] == "ua") {ukrText(); lang = 0;}               // Выбор языка для сообщений
        else if (doc["langWeather"] == "ru") {rusText(); lang = 1;}
        else if (doc["langWeather"] == "en") {enText(); lang =2;}
        //Mqtt
        strlcpy(config.mqttserver, doc["mqtt_server"] | "m24.cloudmqtt.com", sizeof(config.mqttserver));
        config.mqttport = doc["mqtt_port"] | 17049;
        strlcpy(config.mqttUserName, doc["mqtt_user"] | "zqyslqbd", sizeof(config.mqttUserName));
        strlcpy(config.mqttpass, doc["mqtt_pass"] | "ghCaEZLP2i0V", sizeof(config.mqttpass));
        strlcpy(config.mqttname, doc["mqtt_name"] | "Informer", sizeof(config.mqttname));
        strlcpy(config.mqttsubinform, doc["mqtt_sub_inform"] | "Inform/mess", sizeof(config.mqttsubinform));
        strlcpy(config.mqttsub, doc["mqtt_sub"] | "Ulica/temp", sizeof(config.mqttsub));
        strlcpy(config.mqttpubtemp, doc["mqtt_pub_temp"] | "Informer/temp", sizeof(config.mqttpubtemp));
        strlcpy(config.mqttpubtempUl, doc["mqtt_pub_tempUl"] | "Informer/tempUl", sizeof(config.mqttpubtempUl));
        strlcpy(config.mqttpubhum, doc["mqtt_pub_hum"] | "Informer/hum", sizeof(config.mqttpubhum));
        strlcpy(config.mqttpubpress, doc["mqtt_pub_press"] | "Informer/press", sizeof(config.mqttpubpress));
        strlcpy(config.mqttpubforecast, doc["mqtt_pub_forecast"] | "Informer/forecast", sizeof(config.mqttpubforecast));
        strlcpy(config.mqttbutt, doc["mqtt_butt"] | "Informer/button", sizeof(config.mqttbutt));
        config.mqttOn = doc["mqttOn"] | 0;
        config.weatherOn = doc["weatherOn"] | 0;
        config.autoBright = doc["autoBright"] | 0;
        config.scrollDelay = doc["scrollDelay"] | 20;
        config.manualBright = doc["manualBright"] | 15;

        file.close();
    }

    void saveConfig(const char *filename, Config &config) {
        SPIFFS.remove("/config.json");
        File file = SPIFFS.open("/config.json", "w");
        if (!file) {
            Serial.println(F("Failed to create file"));
            return;
        }

        const size_t capacity = JSON_OBJECT_SIZE(26) + 720;
        DynamicJsonDocument doc(capacity);

        doc["ssid"] = config.ssid;
        doc["password"] = config.password;
        doc["ssidAP"] = config.ssidAP;
        doc["passwordAP"] = config.passwordAP;
        doc["timezone"] = config.timeZone;
        doc["summertime"] = config.summertime;
        doc["ntpServerName"] = config.ntpServerName;
        doc["timeSigOn"] = config.timeSigOn;
        doc["timeSigOff"] = config.timeSigOff;
        doc["apiKey"] = config.apiKey;
        doc["cityId"] = config.cityId;
        doc["weatherServer"] = config.weatherServer;
        doc["langWeather"] = config.langWeather;
        doc["mqtt_server"] = config.mqttserver;
        doc["mqtt_port"] = config.mqttport;
        doc["mqtt_user"] = config.mqttUserName;
        doc["mqtt_pass"] = config.mqttpass;
        doc["mqtt_name"] = config.mqttname;
        doc["mqtt_sub_inform"] = config.mqttsubinform;
        doc["mqtt_sub"] = config.mqttsub;
        doc["mqtt_pub_temp"] = config.mqttpubtemp;
        doc["mqtt_pub_tempUl"] = config.mqttpubtempUl;
        doc["mqtt_pub_hum"] = config.mqttpubhum;
        doc["mqtt_pub_press"] = config.mqttpubpress;
        doc["mqtt_pub_forecast"] = config.mqttpubforecast;
        doc["mqtt_butt"] = config.mqttbutt;
        doc["mqttOn"] = config.mqttOn;
        doc["weatherOn"] = config.weatherOn;
        doc["autoBright"] = config.autoBright;
        doc["scrollDelay"] = config.scrollDelay;
        doc["manualBright"] = config.manualBright;

        if (serializeJson(doc, file) == 0) {
            Serial.println(F("Failed to write to file"));
        }

        file.close();
    }

    void printFile(const char *filename) {
        File file = SPIFFS.open(filename, "r");
        if (!file) {
            Serial.println(F("Failed to read file"));
            return;
        }

        while (file.available()) {
            Serial.print((char)file.read());
        }
        Serial.println();
        file.close();
    }

/*
..######..##......##.####.########..######..##.....##....##.....##..#######..########..########
.##....##.##..##..##..##.....##....##....##.##.....##....###...###.##.....##.##.....##.##......
.##.......##..##..##..##.....##....##.......##.....##....####.####.##.....##.##.....##.##......
..######..##..##..##..##.....##....##.......#########....##.###.##.##.....##.##.....##.######..
.......##.##..##..##..##.....##....##.......##.....##....##.....##.##.....##.##.....##.##......
.##....##.##..##..##..##.....##....##....##.##.....##....##.....##.##.....##.##.....##.##......
..######...###..###..####....##.....######..##.....##....##.....##..#######..########..########
*/
//=== Переключение режимов ==================================================================================
    void SwitchShowMode() {
        Mode++;

        switch (Mode)
        {
        case 1:
            showSimpleHum();
            break;
        case 2:
            Mode = 0;
            ChangeMode.stop();
            ShowFlag = false;
            ShowFlagMQTT = false;
            printStringWithShift(weatherString.c_str(), config.scrollDelay);
            break;
        default:
            break;
        }
    }

//=== Вывод только цифр ==================================================================================
    void showDigit(char ch, int col, const uint8_t *data) {
        if ((dy<-8) | (dy> 8))
            return;
        int len = pgm_read_byte(data);
        int w = pgm_read_byte(data + 1 + ch * len);
        col += dx;
        for (int i = 0; i < w; i++)
        {
            if (col + i >= 0 && col + i < 8 * NUM_MAX)
            {
                byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
                if (!dy)
                    scr[col + i] = v;
                else
                    scr[col + i] |= dy > 0 ? v >> dy : v << -dy;
            }
        }
    }
//=== Показ символа в колонке ===================================================================================
    void setCol(int col, byte v) {
        if ((dy<-8) | (dy> 8))
            return;
        col += dx;
        if (col >= 0 && col < 8 * NUM_MAX)
        {
            if (!dy)
                scr[col] = v;
            else
                scr[col] |= dy > 0 ? v >> dy : v << -dy;
        }
    }
//=== Показ символа ===================================================================================
    int showChar(char ch, const uint8_t *data) {
        int len = pgm_read_byte(data);
        int i, w = pgm_read_byte(data + 1 + ch * len);
        for (i = 0; i < w; i++)
            scr[NUM_MAX * 8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
        scr[NUM_MAX * 8 + i] = 0;
        return w;
    }

//=== Подключение к ВАЙФАЙ ==================================================================================================================================
    void wifiConnect() {
        printTime();
        PRN("Connecting WiFi (ssid=" + String(config.ssid) + "  pass=" + String(config.password) + ") ");

        if (!firstStart)
            printStringWithShift("WiFi", 15);
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.ssid, config.password);
        for (int i = 1; i < 21; i++)
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                WIFI_connected = true;
                PRN(" IP adress : ");
                PRN(WiFi.localIP());
                if (!firstStart)
                {
                    String msg = WiFi.localIP().toString() + "                ";
                    clr();
                    printStringWithShift((tYour + " IP: ").c_str(), 12);
                    printStringWithShift(msg.c_str(), 12);
                }
                firstStart = 1;
                amountNotStarts = 0;
                return;
            }
            PRN(".");
            if (!firstStart)
            {
                int j = 0;
                while (j < 500)
                {
                    if (j % 10 == 0)
                        showAnimWifi(i);
                    j++;
                    delay(2);

                }
            }
        }
        WiFi.disconnect();
        PRN(" Not connected!!!");
        amountNotStarts++;
        PRN("Amount of the unsuccessful connecting = ");
        PRN(amountNotStarts);

        if (amountNotStarts > 6)
            ESP.reset();

        WiFi.disconnect();

        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(config.ssidAP, config.passwordAP);
        printTime();    
        DEBUG("Start AP mode!!!");
        DEBUG("Wifi AP IP : ");
        DEBUG(WiFi.softAPIP());

        firstStart = 1;
    }

//===Вывод времени в ПОРТ ==================================================================================================================================
    void printTime() {
        PRN((hour < 10 ? "0" : "") + String(hour) + ":" + (minute < 10 ? "0" : "") + String(minute) + ":" + (second < 10 ? "0" : "") + String(second) + "  ");
    }

//=== Печать бегущей строки *s - текст, shiftDelay - скорость ==========================================
    void printStringWithShift(const char *s, int shiftDelay) {
        while (*s)
        { // коли працює ця функція, основний цикл зупиняється
            printCharWithShift(*s, shiftDelay);
            s++;
            server.handleClient();                     // зберігаемо можливість відповіді на HTML запити під час бігучої стоки
            if(config.mqttOn) MQTTclient.loop();   // зберігаемо можливість слухати MQTT топік під час бігучої стоки

        }
    }

//=== Вывод бегущего символа с - символ, shiftDelay - скорость =====================================
    void printCharWithShift(unsigned char c, int shiftDelay) {
        c = convert_UA_RU_PL_DE(c);
        if (c < ' ')
            return;
        c -= 32;
        int w = showChar(c, fontUA_RU_PL_DE);
        for (int i = 0; i < w + 1; i++)
        {

            delay(shiftDelay);
            scrollLeft();
            refreshAll();
        }
    }

//=== Конвертация символов если исползуется украинский язык ==================
    byte dualChar = 0;
    unsigned char convert_UA_RU_PL_DE(unsigned char _c) {
        unsigned char c = _c;
        // конвертирование латиницы
        if (c == 208) {
            dualChar = 1;
            return 0;
        }
        else if (c == 209 || c == 210) {
            dualChar = 2;
            return 0;
        }
        if (c == 32 && dualChar != 3) {
            dualChar = 3;
            return c;
        }
        if (dualChar == 1) {
            if (c >= 144 && c < 192) {
                c += 48;
            }
            dualChar = 0;
            return c;
        }
        if (dualChar == 2) {
            if (c >= 128 && c < 144) {
                c += 112;
            }
            switch (_c) {
            case 144:
                c = 133;
                break;
            case 145:
                c = 149;
                break;
            }
            dualChar = 0;
            return c;
        }
        // конвертирование польского и немецкого
        if (c == 195) {
            dualChar = 4;
            return 0;
        }
        if (c == 196) {
            dualChar = 5;
            return 0;
        }
        if (c == 197) {
            dualChar = 6;
            return 0;
        }
        if (dualChar == 4) {
            switch (_c) {
            case 132:
                c = 177;
                break;
            case 147:
                c = 166;
                break;
            case 150:
                c = 179;
                break;
            case 156:
                c = 181;
                break;
            case 159:
                c = 183;
                break;
            case 164:
                c = 178;
                break;
            case 179:
                c = 167;
                break;
            case 182:
                c = 180;
                break;
            case 188:
                c = 182;
                break;
            }
            dualChar = 0;
            return c;
        }
        if (dualChar == 5) {
            if (c >= 132 && c < 136) {
                c += 26;
            }
            switch (_c)
            {
            case 152:
                c = 168;
                break;
            case 153:
                c = 169;
                break;
            }
            dualChar = 0;
            return c;
        }
        if (dualChar == 6) {
            if (c >= 129 && c < 133) {
                c += 33;
            }
            if (c >= 154 && c < 156) {
                c += 16;
            }
            if (c >= 185 && c < 189) {
                c -= 13;
            }
            dualChar = 0;
            return c;
        }
    }

//=== Показ подключения ==============================================
    void showAnimWifi(byte probaWifi) {
        byte digPos[2] = {18, 25};
        int digHt = 16;
        int num = 2;
        int ii;
        if (del == 0)
        {
            del = digHt;
            for (ii = 0; ii < num; ii++)
                digold[ii] = dig[ii];
            dig[0] = probaWifi / 10;
            dig[1] = probaWifi % 10;
            for (ii = 0; ii < num; ii++)
                digtrans[ii] = (dig[ii] == digold[ii]) ? 0 : digHt;
        }
        else
            del--;
        clr();
        for (ii = 0; ii < num; ii++)
        {
            if (digtrans[ii] == 0)
            {
                dy = 0;
                showDigit(dig[ii], digPos[ii], dig6x8);
            }
            else
            {
                dy = digHt - digtrans[ii];
                showDigit(digold[ii], digPos[ii], dig6x8);
                dy = -digtrans[ii];
                showDigit(dig[ii], digPos[ii], dig6x8);
                digtrans[ii]--;
            }
        }
        dy = 0;
        refreshAll();
        return;
    }

/*
.##.....##.########..########.....###....########.########....########.####.##.....##.########
.##.....##.##.....##.##.....##...##.##......##....##.............##.....##..###...###.##......
.##.....##.##.....##.##.....##..##...##.....##....##.............##.....##..####.####.##......
.##.....##.########..##.....##.##.....##....##....######.........##.....##..##.###.##.######..
.##.....##.##........##.....##.#########....##....##.............##.....##..##.....##.##......
.##.....##.##........##.....##.##.....##....##....##.............##.....##..##.....##.##......
..#######..##........########..##.....##....##....########.......##....####.##.....##.########
*/
    void updateTime() {
    long curEpoch = localEpoc + ((millis() - localMillisAtUpdate) / 1000);
    long epoch = round(double((curEpoch + 86400L) % 86400L));
    hour = ((epoch % 86400L) / 3600) % 24;

    minute = (epoch % 3600) / 60;
    second = epoch % 60;
    }

/*
..######..##.....##..#######..##......##.########.####.##.....##.########
.##....##.##.....##.##.....##.##..##..##....##.....##..###...###.##......
.##.......##.....##.##.....##.##..##..##....##.....##..####.####.##......
..######..#########.##.....##.##..##..##....##.....##..##.###.##.######..
.......##.##.....##.##.....##.##..##..##....##.....##..##.....##.##......
.##....##.##.....##.##.....##.##..##..##....##.....##..##.....##.##......
..######..##.....##..#######...###..###.....##....####.##.....##.########
*/
    void showAnimClock() {
        if ((millis() % 20) == 0) {
            byte digPos[6] = {1, 8, 18, 25, 15, 16};

            int digHt = 16;
            int num = 4;
            int i;        if(del == 0) {
                del = digHt;
                for(i = 0; i < num; i++) digold[i] = dig[i];


                dig[0] = hour / 10;
                dig[1] = hour % 10;
                dig[2] = minute / 10;
                dig[3] = minute % 10;
                for(i = 0; i < num; i++)  digtrans[i] = (dig[i] == digold[i]) ? 0 : digHt;
            } else del--;
            clr();
                for(i = 0; i < num; i++) {
                    if(digtrans[i] == 0) {
                    dy = 0;
                    showDigit(dig[i], digPos[i], dig6x8);
                    } else {
                    dy = digHt - digtrans[i];
                    showDigit(digold[i], digPos[i], dig6x8);
                    dy =- digtrans[i];
                    showDigit(dig[i], digPos[i], dig6x8);
                    digtrans[i]--;
                    }
                }
                dy = 0;

            // unsigned long flash = millis() % 2000;
            int flash = millis() % 2000;

            if(!alarm_stat){
                if(statusUpdateNtpTime) {                                                 // якщо останнє оновленя часу було вдалим, то двокрапки в годиннику будуть анімовані
                    setCol(digPos[4], flash < 1000 ? 0x24 : 0x42);
                    setCol(digPos[5], flash > 1000 ? 0x42 : 0x24);
                } else {
                    setCol(digPos[4], flash < 1000 ? 0x66 : 0x00);
                    setCol(digPos[5], flash < 1000 ? 0x66 : 0x00);
                }
            } else {
                setCol(digPos[4], 0x66);
                setCol(digPos[5], 0x66);
            }
            refreshAll();
        }
    }

/*
..######..########.########..##.....##.########.########.....########.####.##.....##.########
.##....##.##.......##.....##.##.....##.##.......##.....##.......##.....##..###...###.##......
.##.......##.......##.....##.##.....##.##.......##.....##.......##.....##..####.####.##......
..######..######...########..##.....##.######...########........##.....##..##.###.##.######..
.......##.##.......##...##....##...##..##.......##...##.........##.....##..##.....##.##......
.##....##.##.......##....##....##.##...##.......##....##........##.....##..##.....##.##......
..######..########.##.....##....###....########.##.....##.......##....####.##.....##.########
*/
    void timeUpdateNTP() {
        if(!WIFI_connected) return;
        printTime();
        statusUpdateNtpTime = 1;
        for(int timeTest = 0; timeTest < 3; timeTest++) {
            getNTPtime();
            PRN("          ");
            PRN("Proba #"+String(timeTest+1)+"   "+String(g_hour)+":"+((g_minute<10)?"0":"")+String(g_minute)+":"+((g_second<10)?"0":"")+String(g_second)+":"+String(g_dayOfWeek)+":"+String(g_day)+":"+String(g_month)+":"+String(g_year));

            hourTest[timeTest] = g_hour;
            minuteTest[timeTest] = g_minute;
            if(statusUpdateNtpTime == 0) {
                printTime();
                PRN("ERROR TIME!!!\r\n");
                    return;
            }
            if(timeTest > 0) {
                if((hourTest[timeTest] != hourTest[timeTest - 1]||minuteTest[timeTest] != minuteTest[timeTest - 1])) {
                    statusUpdateNtpTime = 0;
                    printTime();
                    PRN("ERROR TIME!!!\r\n");
                    return;
                }
            }
        }

        hour=g_hour;

        // if (!config.summertime) {
        //     hour = hour - 1;
        // }

        minute=g_minute;
        second=g_second;
        day=g_day;
        dayOfWeek=g_dayOfWeek;
        month=g_month;
        year=g_year;

        localMillisAtUpdate = millis();
        localEpoc = (hour * 60 * 60 + minute * 60 + second);
        convertDw();
        convertMonth();
        printTime();
        Serial.println((day < 10 ? "0" : "") + String(day) + "." + (month < 10 ? "0" : "") + String(month) + "." + String(year) + " DW = " + String(dayOfWeek));
        Serial.println("          Time update OK.");
    }

/*
.##....##.########.########.....########.####.##.....##.########
.###...##....##....##.....##.......##.....##..###...###.##......
.####..##....##....##.....##.......##.....##..####.####.##......
.##.##.##....##....########........##.....##..##.###.##.######..
.##..####....##....##..............##.....##..##.....##.##......
.##...###....##....##..............##.....##..##.....##.##......
.##....##....##....##..............##....####.##.....##.########
*/
    void getNTPtime() {
        WiFi.hostByName(config.ntpServerName, timeServerIP);
        int cb;
        for(int i = 0; i < 3; i++){
            memset(packetBuffer, 0, NTP_PACKET_SIZE);
            packetBuffer[0] = 0b11100011;
            packetBuffer[1] = 0;
            packetBuffer[2] = 6;
            packetBuffer[3] = 0xEC;
            packetBuffer[12] = 49;
            packetBuffer[13] = 0x4E;
            packetBuffer[14] = 49;
            packetBuffer[15] = 52;
            ntpUDP.beginPacket(timeServerIP, 123);                       //NTP порт 123
            ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
            ntpUDP.endPacket();
            delay(800);                                                  // чекаємо пів секуни
            cb = ntpUDP.parsePacket();
            if(!cb) Serial.println("          no packet yet..." + String (i + 1));
            if(!cb && i == 2) {                                          // якщо час не отримано
            statusUpdateNtpTime = 0;
            return;                                                    // вихіз з getNTPtime()
            }
            if(cb) i = 3;
        }

        if(cb) {                                                      // якщо отримали пакет з серверу
            ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);
            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            unsigned long secsSince1900 = highWord << 16 | lowWord;
            const unsigned long seventyYears = 2208988800UL;        // Unix час станом на 1 січня 1970. в секундах, то 2208988800:
            unsigned long epoch = secsSince1900 - seventyYears;
            boolean summerTime;

            if(month < 3 || month > 10) summerTime = false;             // не переходимо на літній час в січні, лютому, листопаді і грудню
            if(month > 3 && month < 10) summerTime = true;              // Sommerzeit лічимо в квіні, травні, червені, липні, серпені, вересені
            if(((month == 3) && (hour + 24 * day)) >= (3 + 24 * (31 - (5 * year / 4 + 4) % 7)) || ((month == 10) && (hour + 24 * day)) < (3 + 24 * (31 - (5 * year / 4 + 1) % 7))) summerTime = true;
            epoch += (int)(config.timeZone*3600 + (3600*(config.summertime   /*isDayLightSaving*/ && summerTime)));

            g_year = 0;
            int days = 0;
            uint32_t time;
            time = epoch/86400;
            g_hour = (epoch % 86400L) / 3600;
            g_minute = (epoch % 3600) / 60;
            g_second = epoch % 60;
            g_dayOfWeek = (((time) + 4) % 7) + 1;
            while((unsigned)(days += (LEAP_YEAR(g_year) ? 366 : 365)) <= time) {    // Счет года
            g_year++;
            }
            days -= LEAP_YEAR(g_year) ? 366 : 365;
            time -= days;
            days = 0;
            g_month = 0;
            uint8_t monthLength = 0;
            for(g_month = 0; g_month < 12; g_month++){                      // Счет месяца
            if(g_month == 1){
                if(LEAP_YEAR(g_year)) monthLength = 29;
                else monthLength = 28;
            }
            else monthLength = monthDays[g_month];
            if(time >= monthLength) time -= monthLength;
            else break;
            }
            g_month++;
            g_day = time + 1;
            g_year += 1970;
            return;
        }
        Serial.println("Nie ma czasu(((");
    }

// ===========================КОНВЕРТАЦІЯ НАЗВ ДНІВ ТИЖНЯ НА УКРАЇНСЬКУ МОВУ============================================
    void convertDw() {
        switch(dayOfWeek){
            case 1 : dw = tSunday;     break;
            case 2 : dw = tMonday;    break;
            case 3 : dw = tTuesday;   break;
            case 4 : dw = tWednesday; break;
            case 5 : dw = tThursday;  break;
            case 6 : dw = tFriday;    break;
            case 7 : dw = tSaturday;  break;
        }
    }
// ===========================КОНВЕРТАЦІЯ НАЗВ МІСЯЦІВ НА УКРАЇНСЬКУ МОВУ============================================
    void convertMonth(){
        switch(month){
            case 1 : _month = tJanuary;      break;
            case 2 : _month = tFebruary;     break;
            case 3 : _month = tMarch;        break;
            case 4 : _month = tApril;        break;
            case 5 : _month = tMay;          break;
            case 6 : _month = tJune;         break;
            case 7 : _month = tJuly;         break;
            case 8 : _month = tAugust;       break;
            case 9 : _month = tSeptember;    break;
            case 10 : _month = tOctober;     break;
            case 11 : _month = tNovember;    break;
            case 12 : _month = tDecember;    break;
        }
    }

/*
..######..##.....##....###....##....##..######...########....##.....##..#######..########..########
.##....##.##.....##...##.##...###...##.##....##..##..........###...###.##.....##.##.....##.##......
.##.......##.....##..##...##..####..##.##........##..........####.####.##.....##.##.....##.##......
.##.......#########.##.....##.##.##.##.##...####.######......##.###.##.##.....##.##.....##.######..
.##.......##.....##.#########.##..####.##....##..##..........##.....##.##.....##.##.....##.##......
.##....##.##.....##.##.....##.##...###.##....##..##..........##.....##.##.....##.##.....##.##......
..######..##.....##.##.....##.##....##..######...########....##.....##..#######..########..########
*/
    void changeMode() {                                      // Переключение режимов
        mode++;
        if (mode == 7) {
            mode = 0;
        }
    }

/*
..######..########.##....##..######...#######..########...######.....########.....###....########....###...
.##....##.##.......###...##.##....##.##.....##.##.....##.##....##....##.....##...##.##......##......##.##..
.##.......##.......####..##.##.......##.....##.##.....##.##..........##.....##..##...##.....##.....##...##.
..######..######...##.##.##..######..##.....##.########...######.....##.....##.##.....##....##....##.....##
.......##.##.......##..####.......##.##.....##.##...##.........##....##.....##.#########....##....#########
.##....##.##.......##...###.##....##.##.....##.##....##..##....##....##.....##.##.....##....##....##.....##
..######..########.##....##..######...#######..##.....##..######.....########..##.....##....##....##.....##
*/
    void updateSensors() {                                  // Обновление данных с датчиков
        sensorsBmp();                                       // Обновление датчика улицы Bmp
        sensorsSi7021();                                    // Обновление датчика дома Si7021
    }

//=== Вывод на экран температуры на улице =======================================
    void showSimpleTempU() {
        dx = dy = 0;
        clr();
        showDigit((t5 < 0 ? 16 : 15), 0, dig5x8rn); //друкуємо U+ альбо U-

        if(t3 <= -10.0 || t3 >= 10) showDigit((t3 < 0 ? (t3 * -1) / 10 : t3 / 10), 4, dig5x8rn);
        showDigit((t3 < 0 ? (t3 * -1) % 10 : t3 % 10), 10, dig5x8rn);
        showDigit(12, 16, dig5x8rn);
        showDigit(t4, 18, dig5x8rn);
        showDigit(10, 24, dig5x8rn);
        showDigit(11, 27, dig5x8rn);
        refreshAll();
    }


//=== Вывод на экран температуры в доме =======================================
    void showSimpleTemp() {
        dx = dy = 0;
        clr();
        showDigit((t1 < 0 ? 14 : 13), 0, dig5x8rn); // друкуємо D+ альбо D-
        if(t1 <= -10.0 || t1 >= 10) showDigit((t1 < 0 ? (t1 * -1) / 10 : t1 / 10), 4, dig5x8rn);

        showDigit((t1 < 0 ? (t1 * -1) % 10 : t1 % 10), 10, dig5x8rn);
        showDigit(12, 16, dig5x8rn);
        showDigit(t2, 18, dig5x8rn);
        showDigit(10, 24, dig5x8rn);
        showDigit(11, 27, dig5x8rn);
        refreshAll();
    }

//=== Вывод на экран влажности в доме ========================================
    void showSimpleHum() {
        dx = dy = 0;
        clr();
        showDigit(17, 0, dig5x8rn);     // друкуємо знак вологості
        if(h1 >= 0) showDigit(h1, 6, dig5x8rn);
        showDigit(h2, 12, dig5x8rn);
        showDigit(12, 18, dig5x8rn);
        showDigit(h3, 20, dig5x8rn);
        showDigit(18, 26, dig5x8rn);
        refreshAll();
    }

//=== Чтение данных с датчика  ==============================================
    void sensorsBmp() {
        if(bmp280 == false) return;
        tempBmp = bmp.readTemperature();
        pressBmp = bmp.readPressure() / 133.3;
        pressBmp = (int) pressBmp;
        altBmp = bmp.readAltitude(1013.25);
        t3 = tempBmp;
        t4 = (int)(tempBmp*(tempBmp<0?-10:10)) % 10;
        t5 = tempBmp;
        printTime();
        PRN("Temperature BMP280: " + String(tempBmp) + " *C,  Pressure: " + String(pressBmp) + " mmHg,  Approx altitude: " + String(altBmp) + " m");
    }

//=== Чтение данных с датчика si7021 ==============================================
    void sensorsSi7021() {
        if(si7021 == false) return;

        humSi7021 = sensor.readHumidity();
        celsiusSi7021 = sensor.readTemperature();

        t1 = (int)celsiusSi7021;
        t2 = (int)(celsiusSi7021 * (celsiusSi7021<0?-10:10)) % 10;

        h1 = humSi7021 / 10;
        h2 = int(humSi7021) % 10;
        h3 = int(humSi7021 * 10) % 10;

        Serial.println("Temperature Si7021: " + String(celsiusSi7021) + " *C,  Humidity: " + String(humSi7021) + " %");
    }

//=== Вывод на экран давления атмосферы ========================================
    void showSimplePre() {
        dx = dy = 0;
        clr();
        showDigit(19, 0, dig5x8rn);     // друкуємо знак тиску
        showDigit(int(pressBmp) / 100, 6, dig5x8rn);
        showDigit((int(pressBmp) % 100) / 10, 12, dig5x8rn);
        showDigit(int(pressBmp) % 10, 18, dig5x8rn);
        showDigit(20, 24, dig5x8rn);
        showDigit(21, 29, dig5x8rn);
        refreshAll();
    }

/*
.########..####.########.....########..####.########.
.##.....##..##..##.....##....##.....##..##..##.....##
.##.....##..##..##.....##....##.....##..##..##.....##
.########...##..########.....########...##..########.
.##.....##..##..##...........##.....##..##..##.......
.##.....##..##..##...........##.....##..##..##.......
.########..####.##...........########..####.##.......
*/
    void bip(){
        // digitalWrite(buzzerPin, HIGH);
        // delay(80);
        // digitalWrite(buzzerPin, LOW);
        // delay(120);
        tone(buzzerPin, 2000, 80);
        delay(80);
        noTone(buzzerPin);
        delay(120);

    }

/*
.##......##.########....###....########.##.....##.########.########.
.##..##..##.##.........##.##......##....##.....##.##.......##.....##
.##..##..##.##........##...##.....##....##.....##.##.......##.....##
.##..##..##.######...##.....##....##....#########.######...########.
.##..##..##.##.......#########....##....##.....##.##.......##...##..
.##..##..##.##.......##.....##....##....##.....##.##.......##....##.
..###..###..########.##.....##....##....##.....##.########.##.....##
*/
    void getWeather() {
        if(!WIFI_connected) return;

        String payload = "";            // Ответ от сервера
        Serial.println("\nStarting connection to server...");
        String url = "http://";
        url += String(config.weatherServer);
        url += "/data/2.5/weather";
        url += "?id=" + String(config.cityId);
        url += "&appid=" + String(config.apiKey) ;
        url += "&cnt=1";
        url += "&units=metric";
        url += "&lang=" + String(config.langWeather);

        Serial.println(url);

        client.begin(url);
        int httpCode = client.GET();

        payload = client.getString();
        PRN(payload);

        if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK) {
                payload = client.getString();
            } else {
                Serial.println("Can't load weather data! #" + t_http_codes(httpCode));
                return;
            }
        }

        client.end();

        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
        return;
        }

        weather.id       = doc["weather"][0]["id"];
        weather.main     = doc["weather"][0]["main"];
        weather.descript = doc["weather"][0]["description"];
        weather.icon     = doc["weather"][0]["icon"];
        weather.temp     = doc["main"]["temp"];
        weather.feels    = doc["main"]["feels_like"];
        weather.humidity = doc["main"]["humidity"];
        weather.pressure = doc["main"]["pressure"];
        weather.speed    = doc["wind"]["speed"];
        weather.deg      = doc["wind"]["deg"];
        weather.cityName = doc["name"];
        weather.cityId = doc["id"];
        weather.clouds = doc["clouds"]["all"];

        httpData = "";

        weatherDescription = weather.descript;
        weatherDescription.toLowerCase();
        if(lang!=2) convertWeatherDes();

        cityName = weather.cityName;
        config.cityId = weather.cityId;
    //  convertCity();
        temp = weather.temp;
        humidity = weather.humidity;
        pressure = weather.pressure;
        pressure = (pressure/1.3332239) - 0;
        windDeg = weather.deg;
        windSpeed = weather.speed;
        clouds = doc["clouds"]["all"];
        String windDegString;
        if(windDeg >= 345 || windDeg <= 22)  windDegString = "\211";    //"Північний";
        if(windDeg >= 23  && windDeg <= 68)  windDegString = "\234";    //"Північно-східний";
        if(windDeg >= 69  && windDeg <= 114) windDegString = "\230";    //"Східний";
        if(windDeg >= 115 && windDeg <= 160) windDegString = "\235";    //"Південно-східний";
        if(windDeg >= 161 && windDeg <= 206) windDegString = "\210";    //"Південний";
        if(windDeg >= 207 && windDeg <= 252) windDegString = "\232";    //"Південно-західний";
        if(windDeg >= 253 && windDeg <= 298) windDegString = "\231";    //"Західний";
        if(windDeg >= 299 && windDeg <= 344) windDegString = "\233";    //"Північно-західний";

        // weatherString = "         " + tNow + ":    \212 " + String(temp, 0) + ("\202") + "C";
        weatherString = "         " + tNow + ":    \212 " + String(tempBmp, 0) + ("\202") + "C";
        weatherString += "     " + tFeels + ":  " +String(weather.feels) + ("\202") + "C";
        weatherString += "     \213 " + String(humidity) + "%";
        // weatherString += "     \215 " + String(pressure, 0) + tPress;
        weatherString += "     \215 " + String(pressBmp) + tPress;
        weatherString += "     \214 " + windDegString + String(windSpeed, 1) + tSpeed;
        weatherString += "     \216 " + String(clouds) + "%     " + weatherDescription + "                ";

    //заполняем строку для mqtt
        config.mqttforecast = "T.:" + String(temp, 0) + " H:" + String(humidity) + "%" +
                    " W. deg:"+ windDeg + " W. spd:" + String(windSpeed, 1) + tSpeed +
                    " Desc:" + weatherDescription;

        PRN("          Getting weather forecast - is OK.");
    }

//=== Конвертация описания погоды ========================================
    void convertWeatherDes() {
        if(weatherDescription == "clear sky") weatherDescription = tClearSky;
        else if(weatherDescription == "sky is clear") weatherDescription = tSkyIsClear;
        else if(weatherDescription == "few clouds") weatherDescription = tFewClouds;
        else if(weatherDescription == "scattered clouds") weatherDescription = tScatteredClouds;
        else if(weatherDescription == "broken clouds") weatherDescription = tBrokenClouds;
        else if(weatherDescription == "overcast clouds") weatherDescription = tOvercastClouds;
        else if(weatherDescription == "light rain") weatherDescription = tLightRain;
        else if(weatherDescription == "moderate rain") weatherDescription = tModerateRain;
        else if(weatherDescription == "light intensity shower rain") weatherDescription = tLightIntensityShowerRain;
        else if(weatherDescription == "shower rain") weatherDescription = tShowerRain;
        else if(weatherDescription == "heavy intensity rain") weatherDescription = tHeavyIntensityRain;
        else if(weatherDescription == "very heavy rain") weatherDescription = tVeryHeavyRain;
        else if(weatherDescription == "thunderstorm") weatherDescription = tThunderstorm;
        else if(weatherDescription == "haze") weatherDescription = tHaze;
        else if(weatherDescription == "fog") weatherDescription = tFog;
        else if(weatherDescription == "mist") weatherDescription = tMist;
        else if(weatherDescription == "shower sleet") weatherDescription = tShowerSleet;
        else if(weatherDescription == "light snow") weatherDescription = tLightSnow;
        else if(weatherDescription == "light shower snow") weatherDescription = tLightShowerSnow;
        else if(weatherDescription == "snow") weatherDescription = tSnow;
    }

//=== Коллбек функция MQTT ========================================
    void callback(char* topic, byte* payload, unsigned int length) {

        if(!config.mqttOn) return;

        if(String(topic) == config.mqttsubinform) {
        String Text = "";
        char CountRepeat = 0;

            for(unsigned i = 0; i < length; i++) {
            Text += ((char)payload[i]);
            }

            PRN("==========> MQTT >" + Text);

            if (Text.startsWith("##"))  {
                CountRepeat = Text.charAt(2);
                Text = "        " + Text.substring(3, Text.length()) + "            ";
                int CntRepeat = CountRepeat - '0';
                for (int ii = 0; ii < CntRepeat; ii++) {
                    for(int i = 0; i < 4; i++) {
                        bip();
                    }
                    printStringWithShift(Text.c_str(), config.scrollDelay);
                }
            } else {
                Text = "        " + Text + "            ";
                for(int i = 0; i < 4; i++) {
                    bip();
                }
                printStringWithShift(Text.c_str(), config.scrollDelay);
            }


        }

        if(String(topic) == config.mqttbutt) {               //Кнопка
            bip();
            ShowFlagMQTT = true;                             // Показ данных с датчика
        }

        if(String(topic) == config.mqttsub) {
            MQTTClientas.tMqtt3 = 0;
            MQTTClientas.tMqtt4 = 0;
            if((payload[0] >= 48 && payload[0] < 58) || payload[0] == 45) {     // в payload[0] - хранится первый полученный символ. 48, 58 и 45 - это коды знаков можете их посмотреть в fontUA_RU_PL_DE[]
            if(payload[0] == 45) {                                              // если первый символ = "-" (равен минусу) то tMqtt5 = -1
                MQTTClientas.tMqtt5 = -1;
                if(payload[1] >= 48 && payload[1] < 58) {                       //  здесь проверяем уже второй символ что он является числом...
                MQTTClientas.tMqtt3 = payload[1] - 48;                          // если от кода числа отнять 48 то получим число.... К примеру код "0" = 48 если от 48-48 то получим 0
                if(payload[2] >= 48 && payload[2] < 58) {
                    MQTTClientas.tMqtt3 = MQTTClientas.tMqtt3 * 10 + (payload[2] - 48);   // если третий знак тоже число, то второй знак был десятками, умножаем его на 10 (получаем ествественно десятки
                }
                }
                if(payload[3] == 46) {                                          // если третий знак не число, то проверяем его на то что но является точкой...
                if(payload[4] >= 48 && payload[4] < 58) MQTTClientas.tMqtt4 = payload[4] - 48; // если третий знак точка и четвертый знак является числом, то это десятые значения
                }
                if(payload[2] == 46) {                                          // тоже самое со втрорым знаком...
                if(payload[3] >= 48 && payload[3] < 58) MQTTClientas.tMqtt4 = payload[3] - 48;
                }
            } else {                                                            // здесь таже самая процедура но уже с положительными числами))))) tMqtt5 = 1 - это признак того что число положителньное или отрицательное....
                MQTTClientas.tMqtt5 = 1;
                MQTTClientas.tMqtt3 = payload[0] - 48;
                if(payload[1] >= 48 && payload[1] < 58) {
                MQTTClientas.tMqtt3 = MQTTClientas.tMqtt3 * 10 + (payload[1] - 48);
                if(payload[2] == 46) {
                    if(payload[3] >= 48 && payload[3] < 58) MQTTClientas.tMqtt4 = payload[3] - 48;
                }
                }
                if(payload[1] == 46) {
                if(payload[2] >= 48 && payload[2] < 58) MQTTClientas.tMqtt4 = payload[2] - 48;
                }
            }
            }
        }
    }

//=== Ренеконнект для MQTT =============================================================================
    void reconnect() {
        if(!ESPclient.connected() && WiFi.status() == WL_CONNECTED) {
            // if(printCom) {
            printTime();
            Serial.print("MQTT reconnection...");
            // }
            if(MQTTclient.connect(config.mqttname, config.mqttUserName, config.mqttpass)) {
            Serial.println("connected");
            MQTTclient.subscribe(config.mqttsubinform);
            MQTTclient.subscribe(config.mqttbutt);
            MQTTclient.subscribe(config.mqttsub);
            } else {
                Serial.print("failed, rc = ");
                Serial.println(MQTTclient.state());
            }
        }
    }
/*
.########.##....##.########.....
.##.......###...##.##.....##....
.##.......####..##.##.....##....
.######...##.##.##.##.....##....
.##.......##..####.##.....##....
.##.......##...###.##.....##.###
.########.##....##.########..###
*/
