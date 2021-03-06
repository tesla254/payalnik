//=========================           Пины на МК:            ===========================//
#define pinDHT  7   //  Датчик DHT11
#define pinNP   3   //  ШИМ нагрева паяльника
#define pinNF   5   //  ШИМ нагрева фена
#define pinVF   6   //  ШИМ скорости фена
#define pinTP   14  //  Термопара паяльника после усиления
#define pinTF   15  //  Термопара фена после усиления
#define pinSVF  16  //  Аналоговое значение скорости фена
#define REG_L   10  //  Защелка сдвиг.регистра
#define pinOn   4   //  Кнопка включения станции

//======================================================================================//

//=========================         Пины на 74hc165:         ===========================//
#define enc1_A  0   //A  Вывод А 1 энкодера
#define enc1_B  1   //B  Вывод B 1 энкодера
#define enc2_A  2   //C  Вывод А 2 энкодера
#define enc2_B  3   //D  Вывод B 2 энкодера
#define enc1_D  4   //E  Кнопка 1 энкодера
#define enc2_D  5   //F  Кнопка 2 энкодера
#define key1    6   //G  Кнопка Вкл/Выкл 1
#define key2    7   //H  Кнопка Вкл/Выкл 2
//======================================================================================//



//=========================        Подключаем библиотеки       =========================//
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
DHT dht(pinDHT, DHT11);
#include <iarduino_RTC.h>
iarduino_RTC time(RTC_DS3231);
LiquidCrystal_I2C lcd(0x27, 16, 2);
//======================================================================================//

byte degree[8] =      //  Кодируем символ градуса
{
  B00111,
  B00101,
  B00111,
  B00000,
  B00000,
  B00000,
  B00000,
};

//=========================       Основные параметры:         ==========================//
byte NP   = 0;    //  ШИМ нагрева паяльника
byte NF   = 0;    //  ШИМ нагрева фена
byte VF   = 0;    //  ШИМ скорости фена
int  TP   = 0;    //  Текущая т паяльника, в С*
int  TF   = 0;    //  Текущая т фена,в С*
int  STP  = 200;  //  Заданная т паяльника, в С*
int  STF  = 200;  //  Заданная т фена, в С*
int  SVF  = 0;    //  Заданная v фена
int  DM   = 0;    //  Режим дисплея: HIGH-рабочий режим, LOW-режим часов
int  dht_T = 0;   //  Температура с датчика DHT
int  dht_H = 0;   //  Влажность с датчика DHT
int  oldDM = 0;   
//======================================================================================//

//=============================     Таймеры     ================================//


unsigned long timer_1   = 0;
unsigned int  interval_1  = 1;

unsigned long timer_2   = 0;
unsigned int  interval_2  = 1000;

unsigned long timer_3   = 0;
unsigned int  interval_3  = 250;
//======================================================================================//



unsigned long Timer(unsigned long prev_ms, unsigned long interval) {      //  Таймер
  if (millis() < prev_ms) {
    prev_ms = 0;                                                          //  Сбрасываем счетчик, если произошло переполнение millis
  }
  if (millis() >= (prev_ms + interval)) {
    prev_ms = millis();                                                   //  Обновляем prev_ms при достижения необходимого интервала
  }
  return (prev_ms);
}

int Encoder_stat(byte new_s, byte old_s, byte enc1, byte enc2) {         //  Определяем в какую сторону вращение энкодера
  int rez;
  if ( (bitRead(old_s, enc1) == 1) && (bitRead(old_s, enc2) == 1) ) {     //  Если старое состояние было 11
    if ((bitRead(new_s, enc1) == 1) && (bitRead(new_s, enc2) == 0) ){     //  ...а стало 10, то на увеличение
      rez = 2;
    }else if ((bitRead(new_s, enc1) == 0) && (bitRead(new_s, enc2) == 1) ){//  ...если стало 01, то на уменьшение
      rez = 0;
    } else {                                                              //  ...иначе его не трогали
      rez = 1;
    }
  } else {rez = 1;}
  return rez;
}

int Butttons(byte new_s, byte pin) {                                     //  Определяем нажатие кнопки
  int rez = bitRead(new_s, pin);
  return rez;
}

void input() {
  static uint8_t oldstates = 0;                                           //  Переменная хранящее последнее значение
  digitalWrite(REG_L, LOW);                                               //  Дергаем пин защелки
  digitalWrite(REG_L, HIGH);                                              //  для снятия показаний
  uint8_t states = SPI.transfer(0);                                       //  Снимаем значения с регистра
  if (states != oldstates) {                                              //  Если что то изменилось, то:

    if (Butttons(states, key1) == 0) {                                    //  Изменяем заданную температуру паяльника
      STP = 0;
    } else {
      int enc_1 = Encoder_stat(states, oldstates, enc1_A, enc1_B) - 1;
      if (Butttons(states, enc1_D) == 1) {
        STP = STP + enc_1 * 10;
      } else {
        STP = STP + enc_1;
      }
//    STP = constrain(STP, 20, 450);
    }
    
    if (Butttons(states, key2) == 0) {                                    //  Изменяем заданную температуру фена
      STF = 0;
    } else {
      int enc_2 = Encoder_stat(states, oldstates, enc2_A, enc2_B) - 1;
      if (Butttons(states, enc2_D) == 1) {
        STF = STF + enc_2 * 10;
      } else {
        STF = STF + enc_2;
      }
    STF = constrain(STF, 20, 450);
    }
  oldstates = states;
  }
  
}

byte SetShim(int tset, int tact) {
  byte shim = 0;
  if (tact < tset ) {                                                 //  Если температура паяльника ниже установленной температуры то:
    if ((tset - tact) < 16 & (tset - tact) > 6 )                      //  Проверяем разницу между установленной температурой и текущей паяльника,если разница меньше 16 градусов, то
    {
      shim = 99;                                                      //  Понижаем мощность нагрева (шим 0-255, мы делаем 99) - таким образом мы убираем инерцию перегрева
    }
    else  if ((tset - tact) < 7 & (tset - tact) > 3)
    {
      shim = 80;
    }
    else if ((tset - tact) < 4 & (tset - tact) > 0)
    {
      shim = 45;
    }
    else {
      shim = 230;                                                      // Иначе поднимаем мощность нагрева (шим 0-255, мы делаем 230) на максимум для быстрого нагрева до нужной температуры
    }
  }
  else {                                                               // Иначе (если температура паяльника равняется или выше установленной)
    shim = 0;                                                          // Выключаем мощность нагрева (шим 0-255  мы делаем 0)  - таким образом мы отключаем паяльник
  }
  return shim;
}

void Print_on_LCD() {
  /* Макет дисплея:

    TP/STP  time
    TF/STF  SVF
    ----------------
    | 123/234°C 12:12| Рабочий режим
    | 123/234°C  30% |
    ================
    | 12:12:12  25°C | Режим часов
    | 02.09.16  80%  |
    ----------------
  */
  
  if (DM == HIGH) {                                                   //Если включен рабочий режим:

    lcd.setCursor(1, 0);
    if (TP < 100) {
      lcd.print(0);
    }
    if (TP < 10) {
      lcd.print(0);
    }
    lcd.print(TP);
    lcd.print("/");
    if (STP < 100) {
      lcd.print(0);
    }
    if (STP < 10) {
      lcd.print(0);
    }
    lcd.print(STP);
    lcd.print("\1C");
    lcd.print(" ");
    lcd.print(time.gettime("H:i"));

    lcd.setCursor(1, 1);
    if (TF < 100) {
      lcd.print(0);
    }
    if (TF < 10) {
      lcd.print(0);
    }
    lcd.print(TF);
    lcd.print("/");
    if (STF < 100) {
      lcd.print(0);
    }
    if (STF < 10) {
      lcd.print(0);
    }
    lcd.print(STF);
    lcd.print("\1C");
    lcd.print(" ");
    if (SVF < 100) {
      lcd.print(" ");
    }
    if (SVF < 10) {
      lcd.print("0");
    }
    lcd.print(SVF);
    lcd.print("%");
  } else {                                                             //Если включен режим часов

    lcd.setCursor(1, 0);
    lcd.print(time.gettime("H:i:s"));
    lcd.print("  ");
    lcd.print(dht_T);
    lcd.print("\1C");
    lcd.setCursor(1, 1);
    lcd.print(time.gettime("d.m.y"));
    lcd.print("  ");
    lcd.print(dht_H);
    lcd.print("%");
  }
}

void PrintTerm() {
  Serial.print(time.gettime("H:i:s"));
  Serial.println("");
  
  Serial.print("Payalnik: ");
  Serial.print(TP);
  Serial.print("/");
  Serial.print(STP);
  Serial.print("\t");

  Serial.print("Fen: ");
  Serial.print(TF);
  Serial.print("/");
  Serial.print(STF);
  Serial.print("\t");

  Serial.print(SVF);
  Serial.print("%");

  Serial.println("");
  
  Serial.print("PWM: ");
  Serial.print(NP); 
  Serial.print(" | ");
  Serial.print(NF);
  Serial.print(" | ");
  Serial.print(VF);
  Serial.println("");
}

void setup() {

  lcd.init();
  lcd.backlight();                                                   //  Включаем подсветку дисплея
  lcd.createChar(1, degree);                                         //  Создаем символ под номером 1
  Serial.begin(9600);
  time.begin();
  dht.begin();
  SPI.begin();
  digitalWrite(REG_L, HIGH);

  pinMode(pinNP, OUTPUT);
  pinMode(pinNF, OUTPUT);
  pinMode(pinVF, OUTPUT);
  pinMode(pinTP, INPUT);
  pinMode(pinTF, INPUT);
  pinMode(pinSVF, INPUT);
  pinMode(pinOn, INPUT);

  oldDM = digitalRead(pinOn);

}

void loop() {

  if (timer_1 != Timer(timer_1, interval_1)) {
  timer_1 = millis();
    DM = digitalRead(pinOn);
    if (DM != oldDM){
      lcd.clear();
      if (DM  ==  HIGH){
        STP = 200;
        STF = 200;
      }
      oldDM = DM;
    }
    
    if (DM == HIGH) {                             // Проверяем нажата ли кнопка включения станции.

      //Считывание ввода
      input();

      //Считывание сигналов:
      
      
      //==============================================================//      
      //      !!!          ИЗМЕНИТЬ ПОСЛЕ ОТЛАДКИ       !!!           //
      //==============================================================//
      // TP = analogRead(pinTP);
      // TF = analogRead(pinTF);
      TP = 90;
      TF = 50;
      //SVF = analogRead(pinSVF);                   //  Значение потенциометра для фена
      SVF = 50;
      //==============================================================//
      
      
      
      
      SVF = map(SVF, 0, 1024, 0, 100);            //  Перевод значения в %

    } else {                                      //  Если не нажата, то выключаем все нагреватели и фен.
      STP = 0;
      STF = 0;
      if (TF > 60) {                              //  Если фен горячий
        STF = 30;                                 //  то продуваем на мощности в 30%
      } else {
        SVF = 0;                                  //  иначе просто выключаем
      }
    }

    //Выдача ШИМ сигналов:
    NP = SetShim(STP, TP);                        //  Расчет текущего сигнала ШИМ для нарева паяльника
    analogWrite(pinNP, NP);                       //  Выставление ШИМ

    NF = SetShim(STF, TF);                        //  Расчет текущего сигнала ШИМ для нарева фена
    analogWrite(pinNF, NF);                       //  Выставление ШИМ


    VF = map(SVF, 0, 100, 0, 250);                //  Расчет текущего сигнала ШИМ для скорости фена
    analogWrite(pinVF, VF);                       //  Выставление ШИМ

    

  }

  if (timer_2 != Timer(timer_2, interval_2)) {
  
  timer_2 = millis();
  dht_T=dht.readTemperature();
  dht_H=dht.readHumidity();
  dht_T = abs(dht_T);
  dht_H = abs(dht_H);
                                       //Вывод информации в терминал
  } 
  
  if (timer_3 != Timer(timer_3, interval_3)) {

    timer_3 = millis();
    Print_on_LCD();                               //Вывод информации дисплей
    PrintTerm();

  }
  //====================================================================================================//
}
