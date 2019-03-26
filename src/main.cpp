/*_______By IvanUA ed. by Sendsay______________
 Піни LED------NodeMCU  1.0                    | 
       DataIn__________________D7/GPIO 13      |
       LOAD/CS_________________D8/GPIO 15      |
       CLK_____________________D5/GPIO 14      |
 Кнопка________________________D0/GPIO 16      |
 Бaзер_________________________D6/GPIO 12      |
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

#include <T_ukr.h>
#include <T_cz.h>
#include <T_de.h>
#include <T_en.h>
#include <T_pol.h>
#include <T_rus.h>


//=====================================================================================================================================
DS3231 RTClock;                 // Часы
RTCDateTime dt;                 // Дата/время для часов
IPAddress apIP(192, 168, 4, 1); // Адресс для точки доступа
WiFiUDP ntpUDP;                 // Клиент для получения NTP
IPAddress timeServerIP;         // ??
Adafruit_BMP280 bmp;            // Датчик bmp
Adafruit_Si7021 sensor = Adafruit_Si7021(); 
Ticker modeChangeTimer(changeMode, 3*1000);  // Таймер переключения режимов
HTTPClient client;              // Клиент для погоды

//======================================================================================
void setup()
{
    Serial.begin(115200);
    pinMode(buzzerPin, OUTPUT);             // Выход сигнала буззера
    pinMode(buttonPin, INPUT);               // Вход кнопки

    PRN("");
    PRN("");
    PRN(" ============>START!");
    bip();                                  // Сигнал при старте

    RTClock.begin();                        // Инициализация часов
    PRN("============> Itialize clock!");
    updateTime();                           // Обновление времени

    initMAX7219();                          // Инициализация ЛЕД панели
    sendCmdAll(CMD_SHUTDOWN, 1);            // Сброс панели
    sendCmdAll(CMD_INTENSITY, 1);           // Установка яркости

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

    modeChangeTimer.interval(7*1000);       // Настройка таймера переключения режимов
    modeChangeTimer.start();                // Таймер переключения режимов

    // RTClock.setAlarm1(0, 22,34, 00, DS3231_MATCH_H_M_S);
}

void loop()
{
//=== Обновление таймеров =====================================================
    modeChangeTimer.update();               // Смена режимов отображения

//=== Работа с временем, поднимем флаг каждую секунду ===================================
    if(timeDate.second != lastSecond) {     // счетчик секунд и флаг для процессов                                            // на початку нової секунди скидаємо secFr в "0"
        lastSecond = timeDate.second;
        secFr = 0;                          // флаг для процессов                                   
    } else {
        secFr++;
    }

//=== Обновление датчиков каждую минуту =============================================================
    if ((timeDate.second == 0) and (not secFr)) {
        updateSensors();     
    }

//=== Обновление погоды с сайта каждые 15 минут ==============================================================
    if (((timeDate.minute % 15 ) == 0) and (timeDate.second == 0) and (not secFr)) {
        if (WIFI_connected) {
            getWeather();
        } else {
            PRN("============> Can't get Weather, check WiFi!!!");
        }
    }   

       

//=== Работа с будильником==============================================================
    // if (RTClock.isAlarm1() or RTClock.isAlarm2()) {
    //     alarm = true;
    // }    

    if (alarm) {
        if(millis() % 25 == 0) showAnimClock();
        if(secFr==0 && timeDate.second>1 && timeDate.second<=59){
            clr();
        //    sendCmdAll(CMD_INTENSITY, 15);
            refreshAll();
            bip();
            bip();
        } else {
        refreshAll();     
        }
    }

//=== Обновление переменных времени, ход часов ==========================================
    updateTime();

//===Основной цикл отображения ==========================================
    if(not alarm){   
        switch (mode)  {
            case 0 : {
                if (millis() % 25 == 0) {
                    showAnimClock();                     // Вывод времени на часы 
                }
                modeChangeTimer.interval(showTimeInterval * 1000);       
            }
                break;
            case 1 : {
                modeChangeTimer.interval(showAllInterval * 1000);            

                if (si7021) {                            // Вывести темп в доме на экран
                    showSimpleTemp();
                } else { mode++; }
            }
            break;
            case 2 : {
      //         if (bmp280) showSimpleTempU();          // Вывести темп на улице на экран           
            }
            break;
            case 3 : {
                if (si7021) {                            // Вывести влажность в доме                     
                    showSimpleHum();
                } else { mode++; }
            }
            break;
            case 4 : {
         //      if (bmp280) showSimplePre();            // Вывести давление на экран
            break;
            } 
            case 5 : {
                printStringWithShift(weatherString.c_str(), 20);    
            }   
            default:
                break;
        }
    }

//=== Сигнал каждый час и обновление времени ==========================================
    if ((timeDate.minute == 0) and (timeDate.second == 0) and (secFr == 0) and (timeDate.hour >= timeSigOn) and (timeDate.hour <= timeSigOff)) {
        PRN("============> BIP!!!");
        bip();
        bip();

        if (WIFI_connected) {
           getNTPtime();               // ***** Получение времени из интернета
        }
    }

//=== Работа с кнопкой ==========================================
    if (digitalRead(buttonPin) == HIGH) {
        
        if (not alarm) {
            mode = 1;
        } else {
            alarm = false;
        }
//delay(500);
    //   while((millis() - timing > 500) == 0) {PRN("BUTTON")};            // Пауза 
    //   PRN("UNBUTTON");
    }

//=== Синронизация таймеров каждые десят минут и 5 секунд ==========================================
    if (((timeDate.minute % 10) == 0) and (timeDate.second == 0) and (not secFr))  {      // Синхронизация таймеров 
        printTime();
        PRN("============> Synchro timers");

        modeChangeTimer.start();                    // Смена режимов отображения
        firstRun = false; 
    }

//=== Проверка подключения к вайфай ==========================================
    if ((timeDate.second > 30 && timeDate.second < 38) && (WiFi.status() != WL_CONNECTED || !WIFI_connected) && not alarm) {
        WIFI_connected = false;
        Serial.println("============> Check WIFI connect!!!");
        
        WiFi.disconnect();
        if(timeDate.minute % 5 == 1) {
            wifiConnect();
            if(WiFi.status() == WL_CONNECTED) WIFI_connected = true;
        }
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


///****************
  //  delay(40);
///****************    
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
    PRN((timeDate.hour < 10 ? "0" : "") + String(timeDate.hour) + ":" + (timeDate.minute < 10 ? "0" : "") + String(timeDate.minute) + ":" + (timeDate.second < 10 ? "0" : "") + String(timeDate.second) + "  ");
}

//=== Печать бегущей строки *s - текст, shiftDelay - скорость ==========================================
void printStringWithShift(const char *s, int shiftDelay)
{
    while (*s)
    { // коли працює ця функція, основний цикл зупиняється
        printCharWithShift(*s, shiftDelay);
        s++;
        // server.handleClient();                                      // зберігаемо можливість відповіді на HTML запити під час бігучої стоки
        // if(mqttOn) MQTTclient.loop();                                          // зберігаемо можливість слухати MQTT топік під час бігучої стоки
     //   ArduinoOTA.handle();                    // Обновление 
     //   timeUpdNTP.update();                    // Оновление таймера обновления времени
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
    dt = RTClock.getDateTime();
    timeDate.hour = dt.hour;
    timeDate.minute = dt.minute;
    timeDate.second = dt.second; 

    //  long curEpoch = localEpoc + ((millis() - localMillisAtUpdate) / 1000);
    //  long epoch = (long)round(curEpoch + 86400L) % 86400L;
    //  hour = ((epoch % 86400L) / 3600) % 24;
    //  minute = (epoch % 3600) / 60;
    //  second = epoch % 60;
    

}


//=== Показ анимир часов ==============================================

void showAnimClock() {
  byte digPos[6] = {1, 8, 18, 25, 15, 16};
  
  if(timeDate.hour < 10) {
    digPos[1] = 5;
    digPos[2] = 15;
    digPos[3] = 22;
    digPos[4] = 12;
    digPos[5] = 13;
  }
  int digHt = 16;
  int num = 4;
  int i;
  if(del == 0) {
    del = digHt;
    for(i = 0; i < num; i++) digold[i] = dig[i];
    dig[0] = timeDate.hour / 10 ? timeDate.hour / 10 : 10;
    dig[1] = timeDate.hour % 10;
    dig[2] = timeDate.minute / 10;
    dig[3] = timeDate.minute % 10;
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
    
  //  setCol(digPos[4], 0x66);
  //  setCol(digPos[5], 0x66);

  //  setCol(digPos[4], flash < 500 ? 0x66 : 0x00);
  //  setCol(digPos[5], flash < 500 ? 0x66 : 0x00);    
    
 //   if((flash >= 180 && flash < 360) || flash >= 540) {                       // мерегтіння двокрапок в годиннику підвязуємо до личильника циклів
 //     setCol(digPos[4], WIFI_connected ? 0x66 : 0x60);
 //     setCol(digPos[5], WIFI_connected ? 0x66 : 0x60);
    //}
    if(statusUpdateNtpTime) {                                                 // якщо останнє оновленя часу було вдалим, то двокрапки в годиннику будуть анімовані
    //  if(flash >= 0 && flash < 180) {
    //    setCol(digPos[4], WIFI_connected ? 0x24 : 0x20);
    //    setCol(digPos[5], WIFI_connected ? 0x42 : 0x40);
    //  }
    //  if(flash >= 360 && flash < 540) {
     //   setCol(digPos[4], WIFI_connected ? 0x42 : 0x40);
     //   setCol(digPos[5], WIFI_connected ? 0x24 : 0x20);
     // }
     
    setCol(digPos[4], flash < 1000 ? 0x66 : 0x00);
    setCol(digPos[5], flash < 1000 ? 0x66 : 0x00);      


     
    }

  //  if(updateForecast && WIFI_connected) setCol(00, flash < 500 ? 0x80 : 0x00);
 //   if(updateForecasttomorrow && WIFI_connected) setCol(31, flash < 500 ? 0x80 : 0x00);
  } else {
    setCol(digPos[4], 0x66);
    setCol(digPos[5], 0x66);
  }
  refreshAll();
}


//=== Обновление времени из сервера==============================================
void timeUpdateNTP() {
    if(!WIFI_connected) return;
    printTime();
    statusUpdateNtpTime = 1;
    for(int timeTest = 0; timeTest < 3; timeTest++) {
        getNTPtime();
        PRN("          ");
        PRN("Proba #"+String(timeTest+1)+"   "+String(g_hour)+":"+((g_minute<10)?"0":"")+String(g_minute)+":"+((g_second<10)?"0":"")+String(g_second));
        
        
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

    RTClock.setDateTime(g_year, g_month, g_day, g_hour, g_minute, g_second);  // Устанвока времени

    timeDate.hour=g_hour;
    timeDate.minute=g_minute;
    timeDate.second=g_second;
    timeDate.day=g_day;
    timeDate.dayOfWeek=g_dayOfWeek;
    timeDate.month=g_month;
    timeDate.year=g_year; 
    localMillisAtUpdate = millis();
    localEpoc = (timeDate.hour * 60 * 60 + timeDate.minute * 60 + timeDate.second);
    convertDw();
    convertMonth();
    printTime();
    Serial.println((timeDate.day < 10 ? "0" : "") + String(timeDate.day) + "." + (timeDate.month < 10 ? "0" : "") + String(timeDate.month) + "." + String(timeDate.year) + " DW = " + String(timeDate.dayOfWeek));
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
    ntpUDP.beginPacket(timeServerIP, 123);                     //NTP порт 123
    ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
    ntpUDP.endPacket();
    delay(800); 
    
                                                // чекаємо пів секуни
    cb = ntpUDP.parsePacket();
    if(!cb) Serial.println("          no packet yet..." + String (i + 1)); 
    if(!cb && i == 2) {                                              // якщо час не отримано
      statusUpdateNtpTime = 0;
      return;                                             // вихіз з getNTPtime()
    }
    if(cb) i = 3;
  }
  if(cb) {                                                   // якщо отримали пакет з серверу
    ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    const unsigned long seventyYears = 2208988800UL;        // Unix час станом на 1 січня 1970. в секундах, то 2208988800:
    unsigned long epoch = secsSince1900 - seventyYears;
    boolean summerTime;
    if(timeDate.month < 3 || timeDate.month > 10) summerTime = false;             // не переходимо на літній час в січні, лютому, листопаді і грудню
    if(timeDate.month > 3 && timeDate.month < 10) summerTime = true;              // Sommerzeit лічимо в квіні, травні, червені, липні, серпені, вересені
    if(((timeDate.month == 3) && (timeDate.hour + 24 * timeDate.day)) >= (3 + 24 * (31 - (5 * timeDate.year / 4 + 4) % 7)) || ((timeDate.month == 10) && (timeDate.hour + 24 * timeDate.day)) < (3 + 24 * (31 - (5 * timeDate.year / 4 + 1) % 7))) summerTime = true; 
    epoch += (int)(timeZone*3600 + (3600*(isDayLightSaving && summerTime)));      
    g_year = 0;
    int days = 0;
    uint32_t time;
    time = epoch/86400;
    g_hour = (epoch % 86400L) / 3600;
    g_minute = (epoch % 3600) / 60;
    g_second = epoch % 60;
    g_dayOfWeek = (((time) + 4) % 7) + 1;
    while((unsigned)(days += (LEAP_YEAR(g_year) ? 366 : 365)) <= time) {
      g_year++;
    }
    days -= LEAP_YEAR(g_year) ? 366 : 365;
    time -= days;
    days = 0;
    g_month = 0;
    uint8_t monthLength = 0;
    for(g_month = 0; g_month < 12; g_month++){
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
  switch(timeDate.dayOfWeek){
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
  switch(timeDate.month){
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
    if (mode == 6) {
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


    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);

    if (!root.success()) {
        Serial.println("Json parsing failed!");
        return;
    }

    weather.id       = root["weather"][0]["id"];
    weather.main     = root["weather"][0]["main"];
    weather.descript = root["weather"][0]["description"];
    weather.icon     = root["weather"][0]["icon"];
    weather.temp     = root["main"]["temp"];
    weather.humidity = root["main"]["humidity"];
    weather.pressure = root["main"]["pressure"];
    weather.speed    = root["wind"]["speed"];
    weather.deg      = root["wind"]["deg"];
    weather.cityName = root["name"];
    weather.cityId = root["id"];
    weather.clouds = root["clouds"]["all"];

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
    clouds = root["clouds"]["all"];
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

//=================================================
// END.