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


#include <Arduino.h>
#include <main.h>

#include <max7219.h>
#include <fonts.h>

#include <Wire.h>
#include <DS3231.h>
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
#include <Ticker.h>

#include <T_ukr.h>
#include <T_cz.h>
#include <T_de.h>
#include <T_en.h>
#include <T_pol.h>
#include <T_rus.h>

//=====================================================================================================================================
RTCDateTime dt;                 // Дата/время для часов
IPAddress apIP(192, 168, 4, 1); // Адресс для точки доступа
WiFiUDP ntpUDP;                 // Клиент для получения NTP
IPAddress timeServerIP;         // ??
Adafruit_BMP280 bmp;            // Датчик bmp
Adafruit_Si7021 sensor = Adafruit_Si7021();     // Датчик SI
HTTPClient client;              // Клиент для погоды
WiFiClient ESPclient;           // Клиент подключения к ВАЙФАЙ
PubSubClient MQTTclient(ESPclient); // Клиент MQTT
Ticker ChangeMode(SwitchShowMode, 2*1000);          // Таймер переключения режимов отображение данных с датчиков

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

    Serial.begin(115200);
    pinMode(buzzerPin, OUTPUT);             // Выход сигнала буззера
    pinMode(buttonPin, INPUT);              // Вход кнопки
    pinMode(lightPin, OUTPUT);              // Выход мигалки

    PRN("");
    PRN("");
    PRN(" ============>START!");

    localMillisAtUpdate = millis();
    localEpoc = (hour * 60 * 60 + minute * 60 + second);

    initMAX7219();                          // Инициализация ЛЕД панели
    sendCmdAll(CMD_SHUTDOWN, 1);            // Сброс панели 
    sendCmdAll(CMD_INTENSITY, 15);          // Установка яркости

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

    if (lang == 0) ukrText();               // Выбор языка для сообщений 
    else if (lang == 1) rusText();
    else if (lang == 2) polText();
    else if (lang == 3) czText();
    else if (lang == 4) deText();
    else if (lang == 5) enText();                 

    wifiConnect();                          // подключение к Вайфай

    if(WiFi.status() == WL_CONNECTED) {
        ntpUDP.begin(localPort);            // Запуск UDP для получения времени
        timeUpdateNTP();                    // Обновление времени
        getWeather();                       // Получение данныз прогноза погоды
    }

    MQTTclient.setServer(MQTTClientas.mqtt_server, MQTTClientas.mqtt_port);
    MQTTclient.setCallback(callback);
    MQTTclient.connect(MQTTClientas.mqtt_name);
    MQTTclient.subscribe(MQTTClientas.mqtt_sub_inform);
    MQTTclient.subscribe(MQTTClientas.mqtt_sub);
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

//=== Обновление погоды с сайта каждые 15 минут ==============================================================
    if (((minute % 15 ) == 0) and (second == 15) and (not secFr)) {
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
    if ((minute == 0) and (second == 0) and (secFr == 0) and (hour >= timeSigOn) and (hour <= timeSigOff)) {
        PRN("============> BIP!!!");       
        bip();    
        bip();

        if (WIFI_connected) {
           getNTPtime();               // ***** Получение времени из интернета
        }
    }

//=== Работа с кнопкой и показ данных из датчиков==========================================
    if (((((digitalRead(buttonPin) == HIGH) || (((minute % 15) == 0) && (second == 3))) && ((hour >= timeSigOn) and (hour <= timeSigOff))) || (ShowFlagMQTT == true)) && ShowFlag == false) {
        ChangeMode.start();  
        ShowFlag = true;  
        showSimpleTemp();        
    }

//=== Проверка подключения к вайфай ==========================================
    if ((second > 30 && second < 38) && (WiFi.status() != WL_CONNECTED || !WIFI_connected) && not alarm) {
        WIFI_connected = false;
        Serial.println("============> Check WIFI connect!!!");
        
        WiFi.disconnect();
        if(minute % 5 == 1) {
            wifiConnect();
            if(WiFi.status() == WL_CONNECTED) WIFI_connected = true;
        }
    }

    // ---------- 50 сек. перевірка доступності MQTT та публікація температури ---------
    if (second == 50 && MQTTClientas.mqttOn && !alarm_stat && WIFI_connected) {
      if (WiFi.status() != WL_CONNECTED) {
            WIFI_connected = false;
      }
      if (!MQTTclient.connected() && WIFI_connected) {
            reconnect();
      }
      if (MQTTclient.connected() && WIFI_connected) {
        if (bmp280) MQTTclient.publish(MQTTClientas.mqtt_pub_temp, (String(t1) + "." + String(t2)).c_str());
        if (bmp280) MQTTclient.publish(MQTTClientas.mqtt_pub_press, (String(pressBmp).c_str()));
        if(si7021) MQTTclient.publish(MQTTClientas.mqtt_pub_tempUl, (String(t3) + "." + String(t4)).c_str());
        if(si7021) MQTTclient.publish(MQTTClientas.mqtt_pub_hum, (String(humSi7021)).c_str());
        // if(sensorHumi == 4 && humBme != 0) MQTTclient.publish(MQTTClientas.mqtt_pub_hum, (String(humBme)).c_str());
        // if(sensorHumi == 5 && humiDht22 != 0) MQTTclient.publish(MQTTClientas.mqtt_pub_hum, (String(humiDht22)).c_str());
      } 
        MQTTclient.publish(MQTTClientas.mqtt_pub_forecast, String(MQTTClientas.mqtt_forecast).c_str());
        
        // if(sensorPrAl == 3 && pressBmp != 0) {
        //   MQTTclient.publish(MQTTClientas.mqtt_pub_press, String(pressBmp).c_str());
        //   MQTTclient.publish(MQTTClientas.mqtt_pub_alt, String(altBmp).c_str());
        // }
        // if(sensorPrAl == 4 && pressBme != 0) {
        //   MQTTclient.publish(MQTTClientas.mqtt_pub_press, String(pressBme).c_str());
        //   MQTTclient.publish(MQTTClientas.mqtt_pub_alt, String(altBme).c_str());
        // }
        // if(printCom) {
        if (not secFr) {    
          printTime();
          Serial.print("Publish in topic ");
          if(si7021) Serial.print("Temperature: " + String(t1) + "." + String(t2) + "*C,   ");
          if(bmp280) Serial.print("Na ulice: " + String(t3) + "." + String(t4) + "*C,   ");
          if(humSi7021 != 0) Serial.print("Humidity: " + String(humSi7021) + " %,  ");
        //   if(sensorHumi == 4 && humBme != 0) Serial.print("Humidity: " + String(humBme) + " %,  ");
        //   if(sensorHumi == 5 && humiDht22 != 0) Serial.print("Humidity: " + String(humiDht22) + " %,  ");
          if(pressBmp != 0) Serial.print("  Pressure: " + String(pressBmp) + " mmHg,  Altitude: " + String(altBmp) + " m.");
        //   if(sensorPrAl == 4 && pressBme != 0) Serial.print("  Pressure: " + String(pressBme) + " mmHg,  Altitude: " + String(altBme) + " m.");
          Serial.println("");
        }  
        // }
      }
    // }
  
    // ---------- якщо мережа WiFi доступна то виконуємо наступні функції ----------------------------
    if(WIFI_connected){
        if(MQTTClientas.mqttOn) MQTTclient.loop();           // перевіряємо чи намає вхідних повідомлень, як є, то кoлбек функція
    }




//=== Управление яркостью экрана=========================================
    // int lightSensor = analogRead(PIN_A0);
    // int screenDarkness = 0;

    // if ((lightSensor > 0) and (lightSensor < 240)) 
    // {
    //     screenDarkness = 0;
    // } else if (lightSensor > 270 and lightSensor < 470)
    // {
    //     screenDarkness = 3;
    // } else if (lightSensor > 500 and lightSensor < 670)
    // {
    //     screenDarkness = 6;
    // } else if (lightSensor > 700 and lightSensor < 770)
    // {
    //     screenDarkness = 9;
    // } else if (lightSensor > 800 and lightSensor < 870)
    // {
    //     screenDarkness = 12;
    // } else if (lightSensor > 900 and lightSensor < 1024)
    // {
    //     screenDarkness = 15;
    // }   

    // sendCmdAll(CMD_INTENSITY, screenDarkness);
    //   sendCmdAll(CMD_INTENSITY, map(analogRead(PIN_A0), 0, 15, 0, 15));
   
}

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
        printStringWithShift(weatherString.c_str(), 20);    
        break;
    default:
        break;
    }
}

//=== Вывод только цифр ==================================================================================
void showDigit(char ch, int col, const uint8_t *data)
{
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
void setCol(int col, byte v)
{
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
int showChar(char ch, const uint8_t *data)
{
    int len = pgm_read_byte(data);
    int i, w = pgm_read_byte(data + 1 + ch * len);
    for (i = 0; i < w; i++)
        scr[NUM_MAX * 8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
    scr[NUM_MAX * 8 + i] = 0;
    return w;
}

//=== Подключение к ВАЙФАЙ ==================================================================================================================================
void wifiConnect()
{
    printTime();
    PRN("Connecting WiFi (ssid=" + String(ssid.c_str()) + "  pass=" + String(password.c_str()) + ") ");

    if (!firstStart)
        printStringWithShift("WiFi", 15);
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
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
                printStringWithShift((tYour + " IP: ").c_str(), 15);
                printStringWithShift(msg.c_str(), 25);
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
       // delay(800);
    }
    WiFi.disconnect();
    PRN(" Not connected!!!");
    amountNotStarts++;
    PRN("Amount of the unsuccessful connecting = ");
    PRN(amountNotStarts);
    if (amountNotStarts > 21)
        ESP.reset();
    if (!firstStart)
    {
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(ssidAP.c_str(), passwordAP.c_str());

        printTime();
        PRN("Start AP mode!!!");
        PRN("          Wifi AP IP : ");
        PRN(WiFi.softAPIP());

        updateTime();
        printStringWithShift(tPoint.c_str(), 35);
        firstStart = 1;
    }
}

//===Вывод времени в ПОРТ ==================================================================================================================================
void printTime()
{
    PRN((hour < 10 ? "0" : "") + String(hour) + ":" + (minute < 10 ? "0" : "") + String(minute) + ":" + (second < 10 ? "0" : "") + String(second) + "  ");
}

//=== Печать бегущей строки *s - текст, shiftDelay - скорость ==========================================
void printStringWithShift(const char *s, int shiftDelay)
{
    while (*s)
    { // коли працює ця функція, основний цикл зупиняється
        printCharWithShift(*s, shiftDelay);
        s++;
        // server.handleClient();                     // зберігаемо можливість відповіді на HTML запити під час бігучої стоки
        if(MQTTClientas.mqttOn) MQTTclient.loop();   // зберігаемо можливість слухати MQTT топік під час бігучої стоки

    }
}

//=== Вывод бегущего символа с - символ, shiftDelay - скорость =====================================
void printCharWithShift(unsigned char c, int shiftDelay)
{
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
unsigned char convert_UA_RU_PL_DE(unsigned char _c)
{
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
void showAnimWifi(byte probaWifi)
{
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
}

//=== Обновление времени ==============================================
void updateTime()
{
  long curEpoch = localEpoc + ((millis() - localMillisAtUpdate) / 1000);
  long epoch = round(double((curEpoch + 86400L) % 86400L));
  hour = ((epoch % 86400L) / 3600) % 24;
  minute = (epoch % 3600) / 60;
  second = epoch % 60;
 
}

//=== Показ анимир часов ==============================================
void showAnimClock() {
    if ((millis() % 25) == 0) {
        byte digPos[6] = {1, 8, 18, 25, 15, 16};

        int digHt = 16;
        int num = 4;
        int i;        if(del == 0) {
            del = digHt;
            for(i = 0; i < num; i++) digold[i] = dig[i];
            dig[0] = hour / 10; // ? timeDate.hour / 10 : 10;
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
        
        int flash = millis() % 2000;
        
        if(!alarm_stat){
            if(statusUpdateNtpTime) {                                                 // якщо останнє оновленя часу було вдалим, то двокрапки в годиннику будуть анімовані
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

//=== Обновление времени из сервера==============================================
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

//=== Получение пакета времени из сервера ==============================================
void getNTPtime() {
  WiFi.hostByName(ntpServerName.c_str(), timeServerIP); 
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
    epoch += (int)(timeZone*3600 + (3600*(isDayLightSaving && summerTime)));      
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
void convertDw(){
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

//=== Переключение режимов =======================================
void changeMode() {                                      // Переключение режимов
    mode++;  
    if (mode == 7) {
        mode = 0;
    }          
}

//=== Обновление данных для датчиков =======================================
void updateSensors() {                                  // Обновление данных с датчиков
    sensorsBmp();                                       // Обновление датчика улицы Bmp
    sensorsSi7021();                                    // Обновление датчика дома Si7021
}

// ****************************************
// *        ВЫХОДЫ                        *
// ****************************************


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

// ****************************************
// *        ВХОДЫ                         *
// ****************************************

//=== Чтение данных с датчика  ==============================================
void sensorsBmp() {  //1
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
void sensorsSi7021() {  //2 
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

//=== Звуковой сигнал ========================================
void bip(){
    digitalWrite(buzzerPin, HIGH);
    delay(80);
    digitalWrite(buzzerPin, LOW);
    delay(120);  
}

//=== Получение погоды на сегодня ========================================
void getWeather() { 

    if(!WIFI_connected) return;

    String payload = "";            // ОТвет от сервера

    Serial.println("\nStarting connection to server..."); 

    String url = "http://api.openweathermap.org/data/2.5/weather";
            
            url = url + "?id=" + cityId;
            url += "&appid=" + apiKey;
            url += "&cnt=1";
            url += "&units=metric";
            url += "&lang=" + langWeather;

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
    if(lang!=5) convertWeatherDes();

    cityName = weather.cityName;
    cityId = weather.cityId; 
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

    weatherString += "     \213 " + String(humidity) + "%";

    // weatherString += "     \215 " + String(pressure, 0) + tPress;    
    weatherString += "     \215 " + String(pressBmp) + tPress;
    
    weatherString += "     \214 " + windDegString + String(windSpeed, 1) + tSpeed;
    weatherString += "     \216 " + String(clouds) + "%     " + weatherDescription + "                ";

  
  //заполняем строку для mqtt
    MQTTClientas.mqtt_forecast = "T.:" + String(temp, 0) + " H:" + String(humidity) + "%" + 
                  " W. deg:"+ windDeg + " W. spd:" + String(windSpeed, 1) + tSpeed + 
                  " Desc:" + weatherDescription;

  
  
  
    PRN("          Getting weather forecast - is OK.");
} 

//=== Конвертация описания погоды ========================================
void convertWeatherDes(){
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

  if(!MQTTClientas.mqttOn) return;

  if(String(topic) == MQTTClientas.mqtt_sub_inform) {
    String Text = "        ";
    for(unsigned i = 0; i < length; i++) {
      Text += ((char)payload[i]);
    }
    Text += "             ";
    for(int i = 0; i < 4; i++) {
      bip();
    }
    printStringWithShift(Text.c_str(), 20);
  }

     if(String(topic) == MQTTClientas.mqtt_butt) {       //Кнопка
        bip();
        ShowFlagMQTT = true;                             // Показ данных с датчика  
    }
  
  if(String(topic) == MQTTClientas.mqtt_sub) {
    MQTTClientas.tMqtt3 = 0;
    MQTTClientas.tMqtt4 = 0;
    if((payload[0] >= 48 && payload[0] < 58) || payload[0] == 45) { // в payload[0] - хранится первый полученный символ. 48, 58 и 45 - это коды знаков можете их посмотреть в fontUA_RU_PL_DE[]
      if(payload[0] == 45) {                                        // если первый символ = "-" (равен минусу) то tMqtt5 = -1 
        MQTTClientas.tMqtt5 = -1;
        if(payload[1] >= 48 && payload[1] < 58) {                   //  здесь проверяем уже второй символ что он является числом...
          MQTTClientas.tMqtt3 = payload[1] - 48;                                 // если от кода числа отнять 48 то получим число.... К примеру код "0" = 48 если от 48-48 то получим 0
          if(payload[2] >= 48 && payload[2] < 58) {
            MQTTClientas.tMqtt3 = MQTTClientas.tMqtt3 * 10 + (payload[2] - 48);               // если третий знак тоже число, то второй знак был десятками, умножаем его на 10 (получаем ествественно десятки
          }
        }
        if(payload[3] == 46) {                                      // если третий знак не число, то проверяем его на то что но является точкой...
          if(payload[4] >= 48 && payload[4] < 58) MQTTClientas.tMqtt4 = payload[4] - 48; // если третий знак точка и четвертый знак является числом, то это десятые значения
        }
        if(payload[2] == 46) {                                      // тоже самое со втрорым знаком...
          if(payload[3] >= 48 && payload[3] < 58) MQTTClientas.tMqtt4 = payload[3] - 48;
        }
      } else {                                                      // здесь таже самая процедура но уже с положительными числами))))) tMqtt5 = 1 - это признак того что число положителньное или отрицательное....
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
    if(MQTTclient.connect(MQTTClientas.mqtt_name, MQTTClientas.mqtt_user, MQTTClientas.mqtt_pass)) {
      Serial.println("connected");
      MQTTclient.subscribe(MQTTClientas.mqtt_sub_inform);
      MQTTclient.subscribe(MQTTClientas.mqtt_butt);
      MQTTclient.subscribe(MQTTClientas.mqtt_sub);
    } else {
        Serial.print("failed, rc = ");
        Serial.println(MQTTclient.state());
    }
  }
}


//=================================================
// END.