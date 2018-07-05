#include <Arduino.h>
#include <painlessMesh.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#define   MESH_PREFIX     "MyMesh"
#define   MESH_PASSWORD   "12345678"
#define   MESH_PORT       5555

#define   STATION_SSID     "BreezeHill_50"
#define   STATION_PASSWORD "0932902190"

#define HOSTNAME "MeshMQTT"

// Prototypes
void meshReceivedCallback( const uint32_t &from, const String &msg );
void mqttCallback(char* topic, byte* payload, unsigned int length);
void changedConnectionCallback();

IPAddress getlocalIP();

IPAddress myIP(0,0,0,0);
IPAddress mqttBroker(192, 168, 0, 103);

painlessMesh  mesh;
bool calc_delay = false;
SimpleList<uint32_t> nodes;

WiFiClient wifiClient;
PubSubClient mqttClient(mqttBroker, 3883, mqttCallback, wifiClient);

int iMeshNodes=0;
void changedConnectionCallback() {
  nodes = mesh.getNodeList();
  if (nodes.size() != iMeshNodes) {
    iMeshNodes = nodes.size();
    Serial.printf("Changed connections %s\n", mesh.subConnectionJson().c_str());
    //MQTT getNodes report--------------------------------------------------------
    mqttClient.publish("MyMesh/from/debug", mesh.subConnectionJson().c_str());
    }
}


void setup() {
  Serial.begin(115200);

  mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION );  // set before init() so that you can see startup messages
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on

  // Channel set to 6. Make sure to use the same channel for your mesh and for you other
  // network (STATION_SSID)
  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 6 );
  mesh.onReceive(&meshReceivedCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(HOSTNAME);
}

bool bGotIP = false;
int lastReconnectAttempt=0;
void loop() {
  mesh.update();

  if (bGotIP) {
    if (!mqttClient.loop()) { //No mqtt connection yet
      long now = millis();
      if (now - lastReconnectAttempt > 3000) {// Attempt to reconnect @5sec
        lastReconnectAttempt = now;
        Serial.println("MQTT reconnecting...");
        if (mqttClient.connect("MyMeshBridge")) {
          mqttClient.publish("MyMesh/from/gateway","------Mesh Bridge Ready------");
          mqttClient.subscribe("MyMesh/to/#");
          lastReconnectAttempt = 0;
          }
        }
      }
    }
  else {
    if(myIP != getlocalIP()){//LocalIP changed
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
    String msg = "My Node is ";
    msg += mesh.getNodeId();
    Serial.println(msg);
    bGotIP = true;
    //if (mqttClient.connect("MyMeshClient")) {
    //  mqttClient.publish("MyMesh/from/gateway","Ready!");
    //  mqttClient.subscribe("MyMesh/to/#");
    //  }
    }
  }
}

void meshReceivedCallback( const uint32_t &from, const String &msg ) {
  Serial.printf("bridge: Received from $%03u msg=%s\n", from%1000, msg.c_str());
  String sFrom = String(from);
  String topic = "MyMesh/from/" + sFrom.substring(sFrom.length()-3);
  mqttClient.publish(topic.c_str(), msg.c_str());
}

void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  char* cleanPayload = (char*)malloc(length+1);
  payload[length] = '\0';
  memcpy(cleanPayload, payload, length+1);
  String msg = String(cleanPayload);
  free(cleanPayload);
  //                                                  01234567890123456
  //String targetStr = String(topic).substring(16); //PainlessMesh/to/
  //                                                01234567890
  String targetStr = String(topic).substring(10); //MyMesh/to/

  Serial.print("MQTT Topic:");
  Serial.println(targetStr);

  if(targetStr == "gateway"){
    if(msg == "getNodes"){
      mqttClient.publish("MyMesh/from/gateway", mesh.subConnectionJson().c_str());
      }
    }
  else if(targetStr == "broadcast")
  {
    mesh.sendBroadcast(msg);
  }
  else
  {
    uint32_t target = strtoul(targetStr.c_str(), NULL, 10);
    Serial.print("MQTT Target:");
    Serial.println(target);
    if(mesh.isConnected(target)) {
      mesh.sendSingle(target, msg);
      Serial.print("MQTT Msg:");
      Serial.println(msg);
      Serial.println("Message sent");
      }
    else {
      mqttClient.publish("MyMesh/from/gateway topic:", targetStr.c_str());
      mqttClient.publish("MyMesh/from/gateway   msg:", msg.c_str());
    }
  }
}

IPAddress getlocalIP() {
  return IPAddress(mesh.getStationIP());
}
