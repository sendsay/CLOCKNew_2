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

#define DIN_PIN   13                                                                    //GPIO 13 / D7
#define CS_PIN    15                                                                    //GPIO 15 / D8
#define CLK_PIN   14   
#define MAX_DIGITS 16   
#define NUM_MAX 4

#define PRN(s){Serial.println(s);}  // Печать в ПОРТ
#define DEBUG                       // Отладка

#define buzzerPin D6                // Пин сигнала
#define buttonPin  D0                // Пин кнопки  

//=====================================================================================================================================
int rotate = 90;        //поворот матрицы
int dx = 0;             //координаты
int dy = 0;             //координаты
//int hour=22, minute=40, second=42, month=4, day=6, dayOfWeek=6, year=2018;  // переменные времени

struct time_struct {
  byte hour = 22;
  byte minute = 40;
  byte second = 42;
  byte month = 4;
  byte day = 6;
  byte dayOfWeek = 6;
  int year = 2018;

};
time_struct timeDate;



String ssid = "PUTIN UTELE";            // Назва локального WiFi
String password = "0674788273";         // Пароль локального WiFi
String ssidAP      = "WiFi-Clock";      // Назва точки доступу
String passwordAP  = "" ;               // Пароль точки доступу
bool firstStart = false;                // Первый старт
boolean WIFI_connected = false;         // Флаг подкючекния к ВАЙФАЙ
String tMes, tNow, tPress, tSpeed, tMin, tTom, tKurs, tSale, tYour, tPoint; // Строки для сообщений
byte amountNotStarts = 0;               // Счет НЕ подключения
byte del = 0;                           // ???
byte dig[MAX_DIGITS] = {0};             // ???
byte digold[MAX_DIGITS] = {0};          // ???
byte digtrans[MAX_DIGITS] = {0};        // ???
bool alarm_stat = false;                // Статус будильника
bool alarm_hold = false;                // Отложить будильник???


bool statusUpdateNtpTime = 0;           // Если не 0, то обновление было удачным
int timeZone = 1.0;                     // Временная зона для часов
long timeUpdate = 60000;                // Период обновления времени
// const char* timeServer = "0.ua.pool.ntp.org";  // Сервер обновления времени
uint8_t hourTest[3], minuteTest[3];     // ??
int g_hour, g_minute, g_second, g_month=1, g_day, g_dayOfWeek, g_year;  // ??
long localEpoc = 0;                     // ??
long localMillisAtUpdate = 0;           // ??

String ntpServerName = "ntp3.time.in.ua";   // ??
const int NTP_PACKET_SIZE = 48;         // ??
byte packetBuffer[NTP_PACKET_SIZE];     // ??
static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};        // Кількість днів у місяцях 
#define LEAP_YEAR(Y) (((1970+Y)>0) && !((1970+Y)%4) && (((1970+Y)%100)||!((1970+Y)%400)))   // Високосні літа
bool isDayLightSaving = true;
unsigned int localPort = 2390;
String tJanuary, tFebruary, tMarch, tApril, tMay, tJune, tJuly, tAugust, tSeptember, tOctober, tNovember, tDecember;
String tMonday, tTuesday, tWednesday, tThursday, tFriday, tSaturday, tSunday;
String dw, _month;
//bool RTCnotReady = false;   // Часы не готовы

bool bmp280 = false;                    // Наличие датчика темп. и влажности
float tempBmp = 0;                      // Переменная , хранит темпер. Bmp 
int pressBmp = 0;                       // Давление bmp280
float altBmp = 0;                       // Высота bmp280
float t5 = 0;                           // ??
int t4 = 0;                             // ??
int t3 = 85;                            // ??
int t2 = 0;                             // ??
int t1 = 85;                            // ??

byte mode = 0;                          // Текущий режим отбражения инфs

float hum = 0;  
float humSi7021 = 0;
float celsiusSi7021 = 0;
bool si7021 = false;
byte h1 = 0;
byte h2 = 0;
byte h3 = 0;

bool firstRun = true;                                 // Флаг первого запуска
int showTimeInterval = 300;                            // Интервал показа часов
int showAllInterval = 2;                              // Интервал показа всего остального  

int secFr, lastSecond, lastMinute;                    // Работа с временем

String apiKey= "df9c74ff1a47dcb48aab814fa5500429";    // Ключ для погоды
int cityId = 598098;                                  // локация погоды
char weatherServer[] = "api.openweathermap.org";      // Сервер погоды
String langWeather = "ua";                            // Язык погоды
String weatherDescription = "";                       // Описание погоды
String tClearSky, tSkyIsClear, tFewClouds, tScatteredClouds, tBrokenClouds, tOvercastClouds, tLightRain, tModerateRain, tLightIntensityShowerRain, tShowerRain, tHeavyIntensityRain, tVeryHeavyRain, tThunderstorm, tHaze, tFog, tMist, tShowerSleet, tLightSnow, tLightShowerSnow, tSnow, tWeatrNot, tWeatrTN;   // Описание погоды
byte lang = 0;                                        // Язык текста часов
String cityName;
byte humidity;                                        // влажность для прогноза
float pressure;                                       // давление для прогноза
float temp;                                           // Темпераьтура для прогноза
int clouds;                                           // Облачность для прогноза
int windDeg;                                          // Направление ветра для прогноза
float windSpeed;                                      // Сила ветра для прогноза
String weatherString;                                 // Строка для сборки прогноза для показа
String httpData;

struct weather_structure {
  unsigned int id;
  const char* main;
  const char* icon;
  const char* descript;
  float temp;
  float pressure;
  byte  humidity;
  float speed;
  float deg;
  const char* cityName;
  unsigned int cityId;
  int clouds;
};
weather_structure weather;
String url;

byte timeSigOn = 5;                           // Время начала сигнала
byte timeSigOff = 22;                         // Время конца сигнала

bool alarm = false;                           // Флаг сработки будильника


//=====================================================================================================================================
void showDigit(char ch, int col, const uint8_t *data);// показ цифры на позиции
void setCol(int col, byte v);                         // показ символа в колонке
int showChar(char ch, const uint8_t *data);           // просто показ символа
void wifiConnect();                                   // подключение к ВайФай
void printTime() ;                                    // вывод времени в порт
void printStringWithShift(const char* s, int shiftDelay);   // Вывод бегущей строки
void printCharWithShift(unsigned char c, int shiftDelay);   // Вывод бегущего символа
unsigned char convert_UA_RU_PL_DE(unsigned char _c);  // Конвертация символов
void showAnimWifi(byte probaWifi);                    // Показ подключения к ВАЙФАЙ
void updateTime();                                    // Получить время с часов DS3231
void showAnimClock();                                 // Показ анимированных часов
void timeUpdateNTP();                                 // Получение времени с сервера часов

void getNTPtime();                                    // получение пакета с сервреа               
void convertDw();
void convertMonth();

void showSimpleTempU();                               // Вывести темп на улице на экран
void showSimpleTemp();                                // Вывести темп в доме на экран
void showSimpleHum();                                 // Вывести влажность в доме 
void showSimpleHum();                                 // Вывести влажность
void showSimplePre();                                 // Вывести давление

void changeMode();                                    // Переключение режимов

void updateSensors();                                 // Обновление данных с датчиков
void sensorsSi7021();                                 // Опросить датчик si7021
void sensorsBmp();   //1                              // Опросить датчик bmp280 (дом)

void bip();                                           // Звуковой сигнал

void getWeather();                                    // Получение погоды из интернета
                                  
void convertWeatherDes();                             // Конвертация описания погоды
bool httpRequest();
bool parseData();

//=================================================
// END.