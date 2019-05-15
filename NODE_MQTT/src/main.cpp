#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include <RF24Mesh.h>
#include <RF24Ethernet.h>
#include <PubSubClient.h>
#define NODE_IP 20

IPAddress ip(10,10,3,NODE_IP);
IPAddress gateway(10,10,3,1);

RF24 radio(7,8);
RF24Network network(radio);
RF24Mesh mesh(radio,network);
RF24EthernetClass RF24Ethernet(radio,network,mesh);
EthernetClient ethClient;
PubSubClient clientMqtt(ethClient);
uint32_t mesh_timer = 0;
uint32_t mqtt_timer = 0;
void reconnect() {
  if (!clientMqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (clientMqtt.connect(String(String("node")+String(ip[3])).c_str())) {
      Serial.println("connected");
    } 
  }
}

void setup(){
  Serial.begin(115200);
  clientMqtt.setServer(gateway, 1883);
  Ethernet.begin(ip);
  Ethernet.set_gateway(gateway);
  mesh.setNodeID(NODE_IP);
  mesh.setChild(true);
  mesh.begin();
  
}

void loop()
{
  mesh.update();
  if (!clientMqtt.connected()) {
    reconnect();
  }else{
    if(millis()-mqtt_timer > 1000){ //Every 1 second send heartbeat via MQTT
      mqtt_timer = millis();
      clientMqtt.publish("heartbeat",String("{\"name\":\""+String(String("node")+String(ip[3]))+"\",\"heartbeat\":1,\"mesh_address\":"+String(mesh.mesh_address)+",\"_nodeID\":"+String(mesh._nodeID)+",\"addrListTop\":"+String(mesh.addrListTop)+",\"ip\":\""+String(ip[0])+"."+String(ip[1])+"."+String(ip[2])+"."+String(ip[3])+"\"}").c_str());
      for(uint8_t i=0; i< mesh.addrListTop; i++){
                  Serial.println( mesh.addrList[i].nodeID);
              
          }
    }
  }
  if(network.available()){
        RF24NetworkHeader header;
        uint32_t mills;
        network.read(header,&mills,sizeof(mills));
        Serial.print("Rcv "); Serial.print(mills);
        Serial.print(" from nodeID ");
        int _ID = mesh.getNodeID(header.from_node);
        if( _ID > 0){
           Serial.println(_ID);
        }else{
           Serial.println("Mesh ID Lookup Failed"); 
        }
  }
  if(millis()-mesh_timer > 30000){ //Every 30 seconds, test mesh connectivity
    mesh_timer = millis();
    if( ! mesh.checkConnection() ){
        mesh.renewAddress(); 
     }
  }  
  clientMqtt.loop();
}