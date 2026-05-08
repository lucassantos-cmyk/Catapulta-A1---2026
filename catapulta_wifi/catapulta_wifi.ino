#include "BluetoothSerial.h"
#include <Stepper.h>          //Biblioteca com alterações conforme pull request em https://github.com/arduino-libraries/Stepper/pull/11
#include <ESP32Servo.h>
#define STEPS_MOTOR 2048

//Bibliotecas para permitir atualizações OTA (Over-the-Air) no esp32
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include "senha.h"  //arquivo com ssid e senha da rede wi-fi que o esp tentará se conectar
//const char *ssid = "----------";
//const char *password = "---------";

uint32_t last_ota_time = 0;

//Configuração inicial do bluetooth
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
//Variáveis que vão regir a comunicação bluetooth entre o aplicativo e o esp32
String comando = "";            
String parametro = "";

//Variáveis para controlar o motor de passo
float pos_nova = 0;
float pos_ant = 0;
Stepper motor = Stepper(STEPS_MOTOR, 26, 33, 25, 32);
//Variáveis para controlar o servo que vai liberar a catapulta
Servo trava;     
int servo = 13;
bool travado = 0;
int led_verde = 17;   //led que indica se a catapulta está pronta
int botao = 14;       //botão para ligar a catapulta


void setup() {
  // put your setup code here, to run once:
  /*
  pinMode(botao, INPUT);
  while(digitalRead(botao)==LOW){}  //Espera ligar o botão pra catapulta começar a funcionar
  */
  Serial.begin(115200);
  //Conectar com o wi-fi - IMPORTANTE: A CATAPULTA NÃO VAI LIGAR SE NÃO CONECTAR NO WI-FI
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  //configurações para OTA
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      if (millis() - last_ota_time > 500) {
        Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
        last_ota_time = millis();
      }
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();

  //wi-fi configurado
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //Iniciar comunicação bluetooth
  SerialBT.begin("ESP32_A1"); //Bluetooth device name
  Serial.println("Bluetooth ligado");

  motor.setSpeed(8);  //velocidade do motor
  
  //Inicializar o servo
  trava.attach(servo); 
  trava.write(0);
  
  pinMode(led_verde, OUTPUT);
  digitalWrite(led_verde, LOW);
  
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);  //liga o led azul imbutido na placa para indicar que ela está funcionando

}

//Função para ler os comandos enviados pela comunicação serial bluetooth com o aplicativo
void lerSerial(){
  comando = "";
  parametro = "";
  boolean isCommand=true;//determina aonde salvar o caracter
  char carac; //armazena cada caracter
  while(SerialBT.available()>0) { // enquando houver caracteres
    carac=SerialBT.read(); //grava-lo na variavel carac
    if(carac =='|') {//se o caracter for | 
      isCommand=false;//gravar o restante na var parametro
    }
    else if((carac !='\n') && (isCommand==true)) {//se o caracter NAO for quebra de linha e for um comando
      comando.concat(carac);//concatena o valor na var comando
    }
    else if((carac !='\n') && (isCommand==false)) {//se o caracter NAO for quebra de linha e for um parametro
      parametro.concat(carac);//concatena o valor na var parametro
    }
    delay(50); //delay para nova leitura
  }
}

//Função para girar o motor uma certa quantidade de voltas e depois desliga o motor
void girarMotor(float voltas){
  motor.step(int(voltas * STEPS_MOTOR));
  motor.idle();
}

void loop() {
  // put your main code here, to run repeatedly:
  ArduinoOTA.handle();        //verifica se há a tentativa de enviar outro código para atualizar o esp

  if(SerialBT.available()){
    lerSerial();
  }
  else{
    comando="-1";             //se não houver comando novo, definir "comando" para algo que não faz nada
  }
  
  //Sinal de bluetooth ligado
  if(comando=="0"){
    digitalWrite(led_verde, HIGH);
  }

  //Sinal para controlar o motor
  if(comando=="1"){
    pos_ant = pos_nova;
    pos_nova = parametro.toFloat();
    girarMotor(pos_nova - pos_ant);
  }
    
  //Sinal para controlar o servo
  if(comando=="2"){
    if(!travado){
      trava.write(90);
      travado=1;
    }
    else{
      trava.write(0);
      travado=0;
    }
  }

  //Retorna o comando realizado de volta para a comunicação bluetooth (bom para troubleshooting)
  if(comando!="-1"){
    SerialBT.println(comando + "|" + parametro);
  }
  
}
