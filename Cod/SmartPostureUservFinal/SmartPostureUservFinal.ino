//192.168.4.1

// Librariile necesare
#include "MPU9250.h"
#include <Wire.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <EEPROM.h>

MPU9250 mpu;

#define enc A0 //potentiometru
#define dir1 12 //pin motor(dir1)
#define pwm 13 //pin motor(pwm)

int addrPrag1 = 0;
int addrPrag2 = 1; //adresele EEPROM
int addrPass = 2;

int stare = 0; //ptr debug

byte prag1 = 0; //valori stocate in EEPROM
byte prag2 = 0;

int sendd = 0; //numara de cate ori ai stat incorect

int cont1 = 0; //"timer home made"
int cont2 = 0;

String password;
byte password1;

int tiltPrag1 = 0; //valorile
int tiltPrag2 = 0;

int16_t poz = 0;
int16_t tilt = 0;


const char* ssid     = "SmartPosture";

WiFiServer server(80);


void restmot(int a) { //functie pentru a aduce motorul la o pozitie stabilita in program
  poz = analogRead(enc);
  poz = map(poz, 0, 1024, 0, 100);
  Serial.println(poz);
  while (poz != a) {
    poz = analogRead(enc);
    poz = map(poz, 0, 1024, 0, 100);
    Serial.println(poz);
    if (poz < a) {
      digitalWrite(dir1, HIGH);     // debobineaza
      analogWrite(pwm, 250);
    }
    if (poz > a) {
      digitalWrite(dir1, LOW);     //bobineaza
      analogWrite(pwm, 250);
    }
    delay(10);
  }
  digitalWrite(dir1, LOW);
  digitalWrite(pwm, LOW);
}


void stopmot() { //opreste motoarele
  digitalWrite(dir1, LOW);
  digitalWrite(pwm, LOW);
}


void motoare(int bob, int vit) { // controleaza motoarele
  digitalWrite(dir1, bob);     //1 relase, 0 pull
  analogWrite(pwm, vit);      //viteza 0-255
}

void limits() {  //limita pentru potentiometru(potentiometrul are o pozitie in care nu citeste)
  if (poz <= 10) {
    poz = 10;
  }
  if (poz >= 90) {
    poz = 90;
  }
}

void setup() {

  Serial.begin(115200);//Pentru debug
  pinMode(enc, INPUT);
  pinMode(dir1, OUTPUT);
  //pinMode(pwm, OUTPUT);

  digitalWrite(dir1, LOW);
  digitalWrite(pwm, LOW);


  EEPROM.begin(512);
  Wire.begin();
  delay(1000);

  if (!mpu.setup(0x68)) {  // schimba pentru adresa ta
    while (1) {
      Serial.println("MPU connection failed.");
      delay(5000);
    }
  }

  mpu.calibrateAccelGyro();

  if (EEPROM.read(addrPass)) { //seteaza noua parola care a fost stocata in EEPROM
    for (int i = 2; i <= 10; i++) {
      Serial.println(EEPROM.read(i));
      password = String(EEPROM.read(i) + (password.toInt()) * 10);
      Serial.println(password);
    }
  }
  else {
    password = "123456789"; //daca nu este parola stocata in EEPROM parola este "123456789"
  }
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  Serial.println(WiFi.localIP());

  // Start server
  server.begin();

  prag1 = EEPROM.read(addrPrag1);
  prag2 = EEPROM.read(addrPrag2);

  Serial.print(password);
}

void loop() {

  mpu.update();
  poz = analogRead(enc);
  poz = map(poz, 0, 1024, 0, 100);
  tilt = abs(mpu.getRoll());
  prag1 = EEPROM.read(addrPrag1);
  prag2 = EEPROM.read(addrPrag2);
  Serial.print("cont1 = ");
  Serial.print(cont1);
  Serial.print(" ");
  Serial.print("cont2 = ");
  Serial.print(cont2);
  Serial.print("tilt = ");
  Serial.print(tilt);
  Serial.println();

  if (tilt > prag2 && tilt < prag1 && poz >= 49 && poz <= 51) {
    cont1++; //porneste timer-ul cand stai incorect
    mpu.update();
  }
  else {
    cont1 = 0;
    mpu.update(); //reseteaza daca stai drept
  }

  if (cont1 == 50) { //daca timer-ul a ajus la 50 aparatul porneste corectia
    while (poz >= 20) {
      poz = analogRead(enc);
      poz = map(poz, 0, 1024, 0, 100);
      limits();
      motoare(0, 255);
      delay(1);
      mpu.update();
    }
    sendd++; //numara de cate ori ai stat incorect
    stopmot(); // dupa corectie motorul se opreste si timer-ul se reseteaza
    cont1 = 0;
  }

  if (tilt > prag1 && poz >= 19 && poz <= 21) {
    cont2++; //porneste timer-ul daca stai drept
    cont1 = 0;
    mpu.update();
  }
  else {
    cont2 = 0; //reseteaza daca stai incorect
    mpu.update();
  }
  if (cont2 == 50) { //daca timer-ul a ajus la 50 aparatul porneste detensionarea
    while (poz <= 50) {
      poz = analogRead(enc);
      poz = map(poz, 0, 1024, 0, 100);
      limits();
      motoare(1, 200);
      delay(10);
    }
    stopmot();
    cont2 = 0;
  }

  delay(100);

  WiFiClient client = server.available(); //verifica potententialii clienti
  if (!client) {
    return;
  }

  String req = client.readStringUntil('\r'); //transforma in string adresa primita din aplicatie
  Serial.println(req);
  client.flush();

  if (req.indexOf("/Sendd/") != -1) { //daca butonul "Statistica" este apasat in aplicatie trimite de cate ori ai stat incorect
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println(""); //  Comillas importantes.
    client.println(sendd); //  Return value.
  }

  if (req.indexOf("/Relax/") != -1) { //daca butonul "Detensionare" este apasat in aplicatie aparatul se detensioneaza
    restmot(80);
  }

  if (req.indexOf("/Start/") != -1) { //daca butonul "Start" este apasat in aplicatie aparatul se pretensioneaza
    restmot(50);
  }

  if (req.indexOf("/Prag1/%") != -1) {//daca butonul "Salveaza primul prag" este apasat in aplicatie este scrisa in memoria EEPROM valoarea slider-ului care seteaza primul prag folosit la corectarea pozitiei
    String pragAux1 = req.substring(req.indexOf("/Prag1/%") + 8);
    int prag1 = pragAux1.toInt();
    EEPROM.write(addrPrag1, prag1);
    EEPROM.commit();
    delay(1000);
    Serial.println(prag1);
  }

  if (req.indexOf("/Prag2/%") != -1) {//daca butonul "Salveaza al doilea prag" este apasat in aplicatie este scrisa in memoria EEPROM valoarea slider-ului care seteaza al doilea prag folosit la corectarea pozitiei
    String pragAux2 = req.substring(req.indexOf("/Prag2/%") + 8);
    int prag2 = pragAux2.toInt();
    EEPROM.write(addrPrag2, prag2);
    EEPROM.commit();
    delay(1000);
    Serial.println(prag2);
  }

  if (req.indexOf("/Pass/") != -1) {//cand butonul "Salveaza parola" este apasat in aplicatie parola de 9 cifre scrisa in textbox-ul special este scrisa in memoria EEPROM
    String PassWAux = req.substring(req.indexOf("/Pass/") + 6);
    int PassW = PassWAux.toInt();
    int NrPass = PassW;
    int p = 1;
    while (p * 10 <= NrPass) {
      p = p * 10;
    }
    while (p != 0) {//salveaza fiecare cifra intr-o adresa diferita a memoriei
      int cif = NrPass / p;
      EEPROM.write(addrPass, cif);
      EEPROM.commit();
      delay(1000);
      addrPass++;
      NrPass = NrPass % p;
      p = p / 10;
    }
    Serial.println(PassW);
  }


  client.flush();
  delay(100);
  stare++;

}
