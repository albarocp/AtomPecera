#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
// TEMPERATURA
#include <OneWire.h>
#include <DallasTemperature.h>
// LCD
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// CO2
#include <TroykaMQ.h>

// PINES
#define PIN_MEDICION_TEMPERATURA D7
#define PIN_MEDICION_CO2 A0

#define PIN_RELE_OXIGENO D3
#define PIN_RELE_CIRCULACION D4
#define PIN_RELE_CALENTAR D5


// VARIABLE GLOBALES
const char* ssid = "fishkiller";
const char* password = "fishkiller";
const char* channel_name = "topic_server";
const char* mqtt_server = "192.168.43.124";
const char* http_server = "192.168.43.124";
const char* http_server_port = "8080";
String clientId;

// SENSOR TEMPERATURA INICIALIZACION
OneWire oneWireObjeto(PIN_MEDICION_TEMPERATURA);
DallasTemperature sensorDS18B20(&oneWireObjeto);

// LCD INICIALIZACION
LiquidCrystal_I2C lcd(0x27, 16, 2);

// CO2 INICIALIZACION
MQ135 mq135(PIN_MEDICION_CO2);

// Variables NODE MCU
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
long lastMsgRest = 0;
char msg[50];
int value = 0;

// VARIABLES GLOBALES
float temperaturaMedido;
int co2Medido;
int estadoco2 = 0;


// Conexión a la red WiFi
void setup_wifi() {

  delay(10);

  // Fijamos la semilla para la generación de número aleatorios. Nos hará falta
  // más adelante para generar ids de clientes aleatorios
  randomSeed(micros());

  Serial.println();
  Serial.print("Conectando a la red WiFi ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // Mientras que no estemos conectados a la red, seguimos leyendo el estado
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // En este punto el ESP se encontrarÃ¡ registro en la red WiFi indicada, por
  // lo que es posible obtener su direcciÃ³n IP
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Dirección IP registrada: ");
  Serial.println(WiFi.localIP());
}

/******************************** ACTUADORES **********************************************/
void encenderCirculacion(){
 digitalWrite(PIN_RELE_CIRCULACION, HIGH);
 Serial.println("Circulacion Encendida");
}
void apagarCirculacion(){
 digitalWrite(PIN_RELE_CIRCULACION, LOW);
 Serial.println("Circulacion Apagada ");
}

void encenderCalentador(){
 digitalWrite(PIN_RELE_CALENTAR, HIGH);
 Serial.println("Calentador Encendida");
}
void apagarCalentador(){
 digitalWrite(PIN_RELE_CALENTAR, LOW);
 Serial.println("Calentador Apagada ");
}

void encenderOxigeno(){
 digitalWrite(PIN_RELE_OXIGENO, HIGH);
 Serial.println("Oxigeno Encendida");
}
void apagarOxigeno(){
 digitalWrite(PIN_RELE_OXIGENO, LOW);
 Serial.println("Oxigeno Apagada ");
}

// Metodo llamado por el cliente MQTT cuando se recibe un mensaje en un canal
// al que se encuentra suscrito. Los parametros indican el canal (topic),
// el contenido del mensaje (payload) y su tamaño en bytes (length)
void callback(char* topic, byte* payload, unsigned int length) {
    int test=0;
    Serial.print("Mensaje recibido [canal: ");
    Serial.print(topic);
    Serial.print("] ");
    Serial.println("");

    // Leemos la información del cuerpo del mensaje. Para ello no sÃ³lo necesitamos
    // el puntero al mensaje, si no su tamaÃ±o.

    for (int i = 0; i < length; i++) {

      Serial.print((char)payload[i]);
    }
    Serial.println();

    DynamicJsonDocument doc(length);
    deserializeJson(doc, payload, length);

    const char* action2 = doc["disp2"];
    const char* action1= doc["disp1"];
    const float tem=doc["medtem"];
    const int co2m=doc["medco2"];

    if(co2m>430 && (strcmp(action2, "oxigenar") == 0)){
          encenderOxigeno();
          estadoco2 = 1;
          lcd.init();
          //lcd.begin();
          lcd.backlight();
    }else{
          apagarOxigeno();
          estadoco2 = 0;
    }

    // ESTADO MUY FRIO
    if ((strcmp(action1, "circular") == 0) && tem<25) {
         encenderCalentador();
         encenderCirculacion();
         lcd.init();
         //lcd.begin();
         lcd.backlight();
    // TEMPERATURA AMBIENTE
  }else if ((strcmp(action1, "circular") == 0) && (tem>=26 && tem<30)) {
       apagarCalentador();
       if(estadoco2==1){

          encenderCirculacion();
       }else{

          apagarCirculacion();
       }
       lcd.init();
       //lcd.begin();
       lcd.backlight();

    // MUY CALIENTE
  }else if((strcmp(action1, "circular") == 0) && (tem>31)){
      apagarCalentador();
      encenderCirculacion();
      lcd.init();
      //lcd.begin();
      lcd.backlight();
    }

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("T:");
        lcd.print(tem,1);
        lcd.print("\337C");

        lcd.setCursor(0, 1);
        lcd.print("CO2:");
        lcd.print(co2m);
        lcd.print("ppm");


}

// Función para la reconexión con el servidor MQTT y la suscripción al canal
// necesario. Tambien se fija el identificador del cliente
void reconnect() {
  // Esperamos a que el cliente se conecte al servidor
  while (!client.connected()) {
    Serial.print("Conectando al servidor MQTT...");
    // Creamos un identificador de cliente aleatorio. Cuidado, esto debe
    // estar previamente definido en un entorno real, ya que debemos
    // identificar al cliente de manera unÃ­voca en la mayorÃ­a de las ocasiones
    clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Intentamos la conexiÃ³n del cliente
    if (client.connect(clientId.c_str())) {
      String printLine = "   Cliente " + clientId + " conectado al servidor" + mqtt_server;
      Serial.println(printLine);
      // Publicamos un mensaje en el canal indicando que el cliente se ha
      // conectado. Esto avisarÃ­a al resto de clientes que hay un nuevo
      // dispositivo conectado al canal. Puede ser interesante en algunos casos.
      String body = "Dispositivo con ID = ";
      body += clientId;
      body += " conectado al canal ";
      body += channel_name;
      client.publish(channel_name, "");
      // Y, por Ãºltimo, suscribimos el cliente al canal para que pueda
      // recibir los mensajes publicados por otros dispositivos suscritos.
      client.subscribe(channel_name);
    } else {
      Serial.print("Error al conectar al canal, rc=");
      Serial.print(client.state());
      Serial.println(". Intentando de nuevo en 5 segundos.");
      delay(5000);
    }
  }
}


 /******************************** LECTURA DE LOS SENSORES **********************************************/
// Lectura de la temperatura de la pecera
float leerTemperatura(){
  float actualTem;
  Serial.println("Solicitando lectura del sensor");
  sensorDS18B20.requestTemperatures();
  actualTem = ((sensorDS18B20.getTempCByIndex(0)-32)*5)/9;
  Serial.println("Temperatura Actual: ");
  Serial.println(actualTem);
  return actualTem;
}

int leerCO2(){
  int tem = mq135.readCO2();
  delay(100);
   return tem;
 }

/******************************** ALTA EN LA BASE DE DATOS **********************************************/
void altaMedicionBD(float med, String nameTable){
  HTTPClient http;
  // Abrimos la conexiÃ³n con el servidor REST y definimos la URL del recurso
  String url = "http://";
  url += http_server;
  url += ":";
  url += http_server_port;
  url += "/add/";
  url +=nameTable;
  //url += "/price";
  String message = "Enviando petición PUT al servidor REST. ";
  message += url;
  Serial.println(message);
  // Realizamos la peticiÃ³n y obtenemos el cÃ³digo de estado de la respuesta
  http.begin(url);

  const size_t bufferSize = JSON_OBJECT_SIZE(1) + 370;
  DynamicJsonDocument root(bufferSize);
  root["nivel"] = med;
  root["fkiddispositivo"] = 2;
  String json_string;
  serializeJson(root, json_string);

  int httpCode = http.PUT(json_string);

  if (httpCode > 0)
  {
   // Si el cÃ³digo devuelto es > 0, significa que tenemos respuesta, aunque
   // no necesariamente va a ser positivo (podrÃ­a ser un cÃ³digo 400).
   // Obtenemos el cuerpo de la respuesta y lo imprimimos por el puerto serie
   String payload = http.getString();
   Serial.println("payload put: " + payload);
  }

  Serial.printf("\nRespuesta servidor REST PUT %d\n", httpCode);
  // Cerramos la conexiÃ³n con el servidor REST
  http.end();
}

/******************************** PUBLICA MENSAJES **********************************************/
void publicarMensaje(){

  StaticJsonDocument<300> doc;
  doc["clientId"] = clientId;
  doc["disp1"]="circular";
  doc["medtem"] = temperaturaMedido;
  doc["medco2"] = co2Medido;
  doc["disp2"]="oxigenar";

  String output;
  serializeJson(doc, output);
  Serial.print("Publicado temperatura: ");
  Serial.println(output);
  client.publish(channel_name, output.c_str());
}

// Metodo de inicialización de la lógica
void setup() {

    // Fijamos el baudrate del puerto de comunicación serie
    Serial.begin(115200);

    // Habilita los pines como salida
    pinMode(PIN_RELE_CIRCULACION, OUTPUT);
    pinMode(PIN_RELE_CALENTAR, OUTPUT);
    pinMode(PIN_RELE_OXIGENO, OUTPUT);

    // HABILITA EL PIN A2 COMO ENTRADA(MIDE EL CO2);
    pinMode(PIN_MEDICION_CO2,INPUT);

    // Nos conectamos a la red WiFi
    setup_wifi();

    // Sensor inicializa
    sensorDS18B20.begin();

    // LCD INICIALIZACION
    lcd.init();
    //lcd.begin();
    lcd.backlight();

    // CALIBRANDO SENSOR
      mq135.calibrate();

    // servidor MQTT
    client.setServer(mqtt_server, 1883);

    // Fijamos la función de callback que se ejecutará cada vez que se publique
    // un mensaje por parte de otro dispositivo en un canal al que el cliente
    // actual se encuentre suscrito
    client.setCallback(callback);
}

void loop() {

  // Nos conectamos al servidor MQTT en caso de no estar conectado previamente
  if (!client.connected()) {
    reconnect();
  }

  // Esperamos (de manera figurada) a que algÃºn cliente suscrito al canal
  // publique un mensaje que serÃ¡ recibido por el dispositivo actual
  client.loop();

  // Cada 2 segundos publicaremos un mensaje en el canal procedente del cliente
  // actual. Esto se hace sin bloquear el loop ya que de lo contrario afectari­a
  // a la recepción de los mensajes MQTT
  // nada
  long now = millis();

  if ((now - lastMsg) > 2000) {

    lastMsg = now;

    // Construimos un objeto JSON con el contenido del mensaje a publicar
    // en el canal.
      publicarMensaje();

  }

  if (now - lastMsgRest > 2000) {

    lastMsgRest = now;
    temperaturaMedido = leerTemperatura();
    altaMedicionBD(temperaturaMedido, "mediciontemperatura");

    co2Medido =  leerCO2();
    altaMedicionBD(co2Medido, "medicionturbidimetro");
  }

}
