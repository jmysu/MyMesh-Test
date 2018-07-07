//************************************************************
// this is a simple example that uses the easyMesh library
//
// 1. blinks led once for every node on the mesh
// 2. blink cycle repeats every BLINK_PERIOD
// 3. sends a silly message to every node on the mesh at a random time between 1 and 5 seconds
// 4. prints anything it receives to Serial.print
//
//
//************************************************************
#include <stdio.h>
#include <painlessMesh.h>


// some gpio pin that is connected to an LED...
// on my rig, this is 5, change to the right number of your LED.
#define   LED             2       // GPIO number of connected LED, ON ESP-12 IS GPIO2

#define   BLINK_PERIOD    3000 // milliseconds until cycle repeat
#define   BLINK_DURATION  100  // milliseconds LED is on for

#define   MESH_SSID       "MyMesh"
#define   MESH_PASSWORD   "12345678"
#define   MESH_PORT       5555

// Prototypes
void sendMessage();
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback();
void nodeTimeAdjustedCallback(int32_t offset);
void delayReceivedCallback(uint32_t from, int32_t delay);

String getFormattedMillis(){
    //String message = "";
    char buf[16];
    long milliseconds = millis();
    int seconds = (int) (milliseconds / 1000) % 60;
    int minutes = (int) ((milliseconds / (1000 * 60)) % 60);
    int hours = (int) ((milliseconds / (1000 * 60 * 60)) % 24);
    int days = (int) (milliseconds / (1000 * 60 * 60 * 24)); //Millis() will rollover every 50days
    sprintf(buf, "%03d:%02d:%02d:%02d", days, hours, minutes, seconds);
    return String(buf);
    }

/*
// spliting a string and return the part nr index
// split by separator
*/
String getStringPartByNr(String data, char separator, int index)
{
    int stringData = 0;        //variable to count data part nr
    String dataPart = "";      //variable to hole the return text
      for(int i = 0; i<(int)data.length(); i++) {    //Walk through the text one letter at a time
        if(data[i]==separator) { //Count the number of times separator character appears in the text
          stringData++;
        }else if(stringData==index) {//get the text when separator is the rignt one
          dataPart.concat(data[i]);
        }else if(stringData>index) {//return text and stop if the next separator appears - to save CPU-time
          return dataPart;
          break;
        }
      }
    return dataPart;//return text if this is the last part
}
int getStringPartNr(String data, char separator)
{
    int stringData = 0;        //variable to count data part nr
    for(int i = 0; i<(int)data.length(); i++) {    //Walk through the text one letter at a time
        if(data[i]==separator) { //Count the number of times separator character appears in the text
          stringData++;
        }
      }
    return stringData;
}

Scheduler     userScheduler; // to control your personal task
painlessMesh  mesh;

bool calc_delay = false;
SimpleList<uint32_t> nodes;

void sendMessage() ; // Prototype
Task taskSendMessage( TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;



void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);

  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  //mesh.setDebugMsgTypes(ERROR | DEBUG | CONNECTION | COMMUNICATION);  // set before init() so that you can see startup messages
  mesh.setDebugMsgTypes(ERROR | DEBUG );  // set before init() so that you can see startup messages

  //mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, 6 );
  String sNodeId = "\nMy NodeId:"+String(mesh.getNodeId());
  Serial.println(sNodeId);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);

  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();

  blinkNoNodes.set(BLINK_PERIOD, (mesh.getNodeList().size() + 1) * 2, []() {
      // If on, switch off, else switch on
      if (onFlag)
        onFlag = false;
      else
        onFlag = true;
      blinkNoNodes.delay(BLINK_DURATION);

      if (blinkNoNodes.isLastIteration()) {
        // Finished blinking. Reset task for next run
        // blink number of nodes (including this node) times
        blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
        // Calculate delay based on current mesh time and BLINK_PERIOD
        // This results in blinks between nodes being synced
        blinkNoNodes.enableDelayed(BLINK_PERIOD -
            (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
      }
  });
  userScheduler.addTask(blinkNoNodes);
  blinkNoNodes.enable();

  randomSeed(analogRead(A0));
}

void loop() {
  userScheduler.execute(); // it will run mesh scheduler as well
  mesh.update();
  digitalWrite(LED, !onFlag);
}

void sendMessage() {
  String msg = "<";
  String sId = String(mesh.getNodeId() );
  msg += sId + ">";
  //msg += ")  FreeMemory: " + String(ESP.getFreeHeap());
  msg += "[" +getFormattedMillis() +"]";
  mesh.sendBroadcast(msg);

  if (calc_delay) {
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
      mesh.startDelayMeas(*node);
      node++;
    }
    calc_delay = false;
  }

  Serial.printf("Sending message: %s\n", msg.c_str());

  taskSendMessage.setInterval( random(TASK_SECOND * 10, TASK_SECOND * 15));  // between 10 and 15 seconds
}


void receivedCallback(uint32_t from, String & msg) {
  Serial.print(millis()/1000);
  String sMeshTime = "("+String(mesh.getNodeTime())+")";
  Serial.printf("%s: Received from %u msg=%s\n", sMeshTime.c_str(), from, msg.c_str());

  if (msg.indexOf('=')>0) {
    String sCmd  = getStringPartByNr(msg, '=',0);
    String sVal  = getStringPartByNr(msg, '=',1);
    Serial.println(sCmd+" "+sVal);
    Serial.println(getStringPartNr(sCmd, '/'));
    String sCmds="";
    int iNr=getStringPartNr(sCmd, '/');
    if (iNr>0) {
      for (int i=0; i<=iNr; i++) {
        sCmds += ">"+getStringPartByNr(sCmd, '/',i);
        }
      mesh.sendBroadcast("cmd:"+sCmds+" val:"+sVal); //no '=' to prevent recursive messages
      }
    else
      mesh.sendBroadcast("cmd:"+sCmd+" val:"+sVal); //no '=' to prevent recursive messages
    }

}

void newConnectionCallback(uint32_t nodeId) {
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
  Serial.print(millis()/1000);
  Serial.printf(">--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.print(millis()/1000);
  Serial.printf(">Changed connections %s\n", mesh.subConnectionJson().c_str());
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);

  nodes = mesh.getNodeList();

  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) {
    Serial.printf(" %u", *node);
    node++;
  }
  Serial.println();
  calc_delay = true;
}

void nodeTimeAdjustedCallback(int32_t offset) {
  //Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void delayReceivedCallback(uint32_t from, int32_t delay) {
  //Serial.printf("Delay to node %u is %d us\n", from, delay);
}
