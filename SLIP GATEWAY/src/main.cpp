#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include <RF24Mesh.h>
#define SLIP_END     0300
#define SLIP_ESC     0333
#define SLIP_ESC_END 0334
#define SLIP_ESC_ESC 0335
#define UIP_BUFFER_SIZE MAX_PAYLOAD_SIZE
static uint16_t len, tmplen;
static uint8_t lastc;
HardwareSerial *slip_device;
RF24 radio(7, 8);
RF24Network network(radio);
RF24Mesh mesh(radio, network);
uint8_t slip_buf[UIP_BUFFER_SIZE]; // MSS + TCP Header Length
/*-----------------------------------------------------------------------------------*/

// Put a character on the serial device.
void slipdev_char_put(uint8_t c)
{
	slip_device->write((char)c);
}

/*-----------------------------------------------------------------------------------*/

// Poll the serial device for a character.
uint8_t slipdev_char_poll(uint8_t *c)
{
	if (slip_device->available()) {
		*c = slip_device->read();
		return 1;
	}
	return 0;
}

/*-----------------------------------------------------------------------------------*/
/**
 * Send the packet in the uip_buf and uip_appdata buffers using the
 * SLIP protocol.
 *
 * The first 40 bytes of the packet (the IP and TCP headers) are read
 * from the uip_buf buffer, and the following bytes (the application
 * data) are read from the uip_appdata buffer.
 *
 * TMRh20: This function now takes in a pointer to an array, and the length of the data
 *
 */
/*-----------------------------------------------------------------------------------*/
void slipdev_send(uint8_t *ptr, size_t len)
{
  uint16_t i;
  uint8_t c;

  slipdev_char_put(SLIP_END);
  
  #if defined (LED_TXRX)
  digitalWrite(DEBUG_LED_PIN,HIGH);
  #endif
  
  for(i = 0; i < len; ++i) {
    c = *ptr++;
    switch(c) {
    case SLIP_END:
      slipdev_char_put(SLIP_ESC);
      slipdev_char_put(SLIP_ESC_END);
      break;
    case SLIP_ESC:
      slipdev_char_put(SLIP_ESC);
      slipdev_char_put(SLIP_ESC_ESC);
      break;
    default:
      slipdev_char_put(c);
      break;
    }
  }
  slipdev_char_put(SLIP_END);
  #if defined (LED_TXRX)
  digitalWrite(DEBUG_LED_PIN,LOW);
  #endif
}
/*-----------------------------------------------------------------------------------*/
/** 
 * Poll the SLIP device for an available packet.
 *
 * This function will poll the SLIP device to see if a packet is
 * available. It uses a buffer in which all avaliable bytes from the
 * RS232 interface are read into. When a full packet has been read
 * into the buffer, the packet is copied into the uip_buf buffer and
 * the length of the packet is returned.
 *
 * \return The length of the packet placed in the uip_buf buffer, or
 * zero if no packet is available.
 */
/*-----------------------------------------------------------------------------------*/
uint16_t slipdev_poll(void)
{
  uint8_t c;


 // Create a new RF24Network header if there is data available, and possibly ready to send
 if(slip_device->available()){  
  
  while((uint8_t)slipdev_char_poll(&c)) {
    switch(c) {
    case SLIP_ESC:
      lastc = c;
      break;

    case SLIP_END:
      lastc = c;
      /* End marker found, we copy our input buffer to the uip_buf
	 buffer and return the size of the packet we copied. */

      // Ensure the data is no longer than the configured UIP buffer size
      len = min(len,UIP_BUFFER_SIZE);
      
      tmplen = len;
      len = 0;
      return tmplen;

    default:
      if(lastc == SLIP_ESC) {
	lastc = c;
	/* Previous read byte was an escape byte, so this byte will be
	   interpreted differently from others. */
	switch(c) {
	case SLIP_ESC_END:
	  c = SLIP_END;
	  break;
	case SLIP_ESC_ESC:
	  c = SLIP_ESC;
	  break;
	}
      } else {
	lastc = c;
      }

      slip_buf[len] = c;
      ++len;

      if(len > UIP_BUFFER_SIZE) {
	len = 0;
      }

      break;
    }
  }
 }
  return 0;
}
/*-----------------------------------------------------------------------------------*/
/**
 * Initialize the SLIP module.
 *
 * This function does not initialize the underlying RS232 device, but
 * only the SLIP part.
 */ 
/*-----------------------------------------------------------------------------------*/
void slipdev_init(HardwareSerial &dev){
  lastc = len = 0;
  slip_device = &dev;
}


// NOTE: IMPORTANT this should be set to the same value as the UIP_BUFSIZE and
// the MAX_PAYLOAD_SIZE in RF24Network. The default is 120 bytes




//Function to send incoming network data to the SLIP interface
void networkToSLIP();

void setup() {

  Serial.begin(115200);

  // Set this to the master node (nodeID 0)
  mesh.setNodeID(0);
  mesh.begin();

  // Use the serial port as the SLIP device
  slipdev_init(Serial);

}


uint32_t active_timer =0;

void loop() {

  // Provide RF24Network addresses to connecting & reconnecting nodes
  if(millis() > 10000){
    mesh.DHCP();
  }

  //Ensure any incoming user payloads are read from the buffer
  while(network.available()){
    RF24NetworkHeader header;
    network.read(header,0,0);    
  }
  
  // Handle external (TCP) data
  // Note: If not utilizing RF24Network payloads directly, users can edit the RF24Network_config.h file
  // and uncomment #define DISABLE_USER_PAYLOADS. This can save a few hundred bytes of RAM.

  if(mesh.update() == EXTERNAL_DATA_TYPE) {
    networkToSLIP();
  }

  // Poll the SLIP device for incoming data
  //uint16_t len = slipdev_poll();
  uint16_t len;
  if( (len = slipdev_poll()) > 0 ){
    if(len > MAX_PAYLOAD_SIZE){ return; }
    RF24NetworkHeader header(01, EXTERNAL_DATA_TYPE);    
    uint8_t meshAddr;
    // Get the last octet of the destination IP address
    uint8_t lastOctet = slip_buf[19];
    //Convert the IP into an RF24Network Mac address
    if ( (meshAddr = mesh.getAddress(lastOctet)) > 0) {
      // Set the RF24Network address in the header
      header.to_node = meshAddr;
      network.write(header, &slip_buf, len);
    }
  }

}


void networkToSLIP(){
    RF24NetworkFrame *frame = network.frag_ptr;
    size_t size = frame->message_size;
    uint8_t *pointer = frame->message_buffer;
    slipdev_send(pointer, size);   
}