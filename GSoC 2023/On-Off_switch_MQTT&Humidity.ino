#include <ArduinoJson.h>// Use version 5.8.0
#include <PubSubClient.h>// Use version 2.6 
#include <ESP8266WiFi.h>
#include <DHT.h>

#define WIFI_AP             "Thomas"
#define WIFI_PASSWORD       "12345678"
#define TOKEN               "h5t7oicry4kll98904hv"

//Pins for On-Off statement (True/False)
#define GPIO0 0
#define GPIO0_PIN 16

//Pins for the DHT sensor
#define DHTPIN 2
#define DHTTYPE DHT11


char thingsboardServer[] = "demo.thingsboard.io";

WiFiClient wifiClient;

PubSubClient client(wifiClient);

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);
unsigned long lastSend; //Don't know if needed!!!!!!!!!!!!

int status = WL_IDLE_STATUS;

// We assume that all GPIOs are LOW
boolean gpioState[] = {false,false};

void setup() {
  Serial.begin(9600);
  // Set output mode for all GPIO pins
  pinMode(GPIO0, OUTPUT);
  pinMode(DHTPIN, INPUT);
  dht.begin();
  delay(10);
  InitWiFi();
  client.setServer( thingsboardServer, 1883 );
  client.setCallback(on_message);
  lastSend = 0;
}

void loop() {
  if ( !client.connected() ) {
    reconnect();
  }
 /* if(
  if ( millis() - lastSend > 100 ) { // Update and send only after 100 msec
    getAndSendTemperatureAndHumidityData();
    lastSend = millis();
  }
*/
  client.loop();
}

void getAndSendTemperatureAndHumidityData()
{
  Serial.println("Collecting temperature data.");

  // Reading temperature or humidity takes about 250 milliseconds!
  float humidity = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float temperature = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  //Print data in serial port
  Serial.println("Sending data to ThingsBoard:");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" *C ");

  //Create the payload in json format
  String payload = "{";
  payload +="\"Humidity\":";
  payload +=humidity;
  payload +=",";
  payload +="\"Temperature\":";
  payload +=temperature;
  payload +="}";

  //Make the payload in to an char type array to bundle things up and publish the payload to thingsboard
  char attributes[1000];
  payload.toCharArray(attributes, 1000);
  client.publish("v1/devices/me/telemetry",attributes);
  client.publish("v1/devices/me/attributes",attributes);
  //Serial.println(attributes); //(only for testing purpose to see if you send the correct format and data as json)
  
}



// The callback for when a PUBLISH message is received from the server.
void on_message(const char* topic, byte* payload, unsigned int length) {

  Serial.println("On message");

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(json);

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);

  if (methodName.equals("getGpioStatus")) {
    // Reply with GPIO status
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
  } else if (methodName.equals("setGpioStatus")) {
    // Update GPIO status and reply
    set_gpio_status(data["params"]["pin"], data["params"]["enabled"]);
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
    client.publish("v1/devices/me/attributes", get_gpio_status().c_str());
  }
}

String get_gpio_status() {
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data[String(GPIO0_PIN)] = gpioState[0] ? true : false;
//  data[String(GPIO2_PIN)] = gpioState[1] ? true : false;
  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  Serial.print("Get gpio status: ");
  Serial.println(strPayload);
  return strPayload;
}

void set_gpio_status(int pin, boolean enabled) {
  if (pin == GPIO0_PIN) {
    // Output GPIOs state
    digitalWrite(GPIO0, enabled ? HIGH : LOW);
    // Update GPIOs state
    gpioState[0] = enabled;
  } //else if (pin == GPIO2_PIN) {
    // Output GPIOs state
    //digitalWrite(GPIO2, enabled ? HIGH : LOW);
    // Update GPIOs state
   // gpioState[1] = enabled;
 // }
}

void InitWiFi() {
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      WiFi.begin(WIFI_AP, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connected to AP");
    }
    Serial.print("Connecting to ThingsBoard node ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect("ESP8266 Device", TOKEN, NULL) ) {
      Serial.println( "[DONE]" );
      // Subscribing to receive RPC requests
      client.subscribe("v1/devices/me/rpc/request/+");
      // Sending current GPIO status
      Serial.println("Sending current GPIO status ...");
      client.publish("v1/devices/me/attributes", get_gpio_status().c_str());
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}
