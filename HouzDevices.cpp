/*
Name:		master_logger.ino
Created:	13-Apr-17 22:55:23
Author:	DarkAngel
*/

#include "Arduino.h"
#include "HouzDevices.h"

#include <RF24.h>
#include <printf.h>

//radio setup
#define rfChannel		0x5B   
#define rfPipeServer	0xF0F0F0F010LL
#define rfPipeSensors	0xF0F0F0F011LL
#define rfPipeBedroom	0xF0F0F0F022LL

//const uint64_t talk_pipes[5] = { 0xF0F0F0F0D2LL, 0xF0F0F0F0C3LL, 0xF0F0F0F0B4LL, 0xF0F0F0F0A5LL, 0xF0F0F0F096LL };
//const uint64_t listen_pipes[5] = { 0x3A3A3A3AD2LL, 0x3A3A3A3AC3LL, 0x3A3A3A3AB4LL, 0x3A3A3A3AA5LL, 0x3A3A3A3A96LL };

unsigned long _radioPayLoad;
HouzDevices::HouzDevices(byte NodeId, RF24 &_radio, byte _rfStatusLed, Stream &serial){
	console = &serial;
	radio = &_radio;
	node_id = NodeId;
	rfStatusLed = _rfStatusLed;
};
 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers 
unsigned long HouzDevices::StrToHex(char str[])
{
  return (long)strtol(str, 0, 16);
};
unsigned long HouzDevices::StrToHex(String str)
{
  return (long)strtol(str.c_str(), 0, 16);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Radio Stuff 
bool HouzDevices::radioSetup()
{
	console->println("::RF setup");
	radio_status = 0;

	//determine device pipes
	console->print("device: ");
	console->println(node_id);

	printf_begin();
	radio->begin();
	radio->setPALevel(RF24_PA_LOW); //RF24_PA_HIGH
	radio->setChannel(rfChannel);
	radio->setRetries(15, 15);
	setPipes(node_id);

	//radio status
	radio_status = (rfChannel == radio->getChannel()); //test if radio is enabled
	console->print("radio: ");
	console->println(radio_status ? "online" : "offline");
	if (radio_status) {
		radio->printDetails();
		radio->startListening();
		radio_status = 1;
	}
	pinMode(rfStatusLed, OUTPUT);
	return true;
};

void HouzDevices::setPipes(byte nodeId) {
	switch (node_id)
	{
	case server_node:
		radio->openWritingPipe(rfPipeServer);

		radio->openReadingPipe(1, rfPipeBedroom);
		radio->openReadingPipe(2, rfPipeSensors);
		break;

	case external_node:
		radio->openWritingPipe(rfPipeSensors);

		radio->openReadingPipe(1, rfPipeServer);
		radio->openReadingPipe(2, rfPipeBedroom);
		break;

	case bedroom_node:
		break;
	default:
		break;
	}
	//radio->setAutoAck(1);

}

bool HouzDevices::radioReady()
{
	radio->printDetails();
	return radio_status;
};

bool HouzDevices::radioRead()
{
	//if radio is not enabled, discard anything
	if (!radio_status) { return false; };
	if (!radio->available()) { return false; };

	//get payload
	while (radio->available()) {
		radio->read(&_radioPayLoad, sizeof(unsigned long));
		digitalWrite(rfStatusLed, HIGH);
	}
	delay(100);
	radio->startListening();
	digitalWrite(rfStatusLed, LOW);
	//prepare for next packet
	return true;
};

deviceData HouzDevices::receivedData() {
	deviceData device = decode(_radioPayLoad, 0); //TODO: handle source node
	_radioPayLoad = 0;
	return device;
};

bool HouzDevices::radioWrite(u32 rfMessage, byte nodeId) {
	if (!radio_status) { return false; };
	radio->stopListening();
	bool result = radio->write(&rfMessage, sizeof(unsigned long));
	radio->startListening();
	return result;
}

bool HouzDevices::radioSend(u8 deviceCmd, u8 deviceId, u32 devicePayload) {
	return radioWrite(encode(deviceCmd, deviceId, devicePayload));
};

bool HouzDevices::radioWrite(u32 rfMessage) {
	return radioWrite(rfMessage, server_node);
};

bool HouzDevices::radioSend(deviceData device) {
	bool result = radioWrite(encode(device.cmd, device.id, device.payload));
	console->println(packetToString((result ? action_rfSentOk : action_rfSentFail), device));
	return true;
};

unsigned long HouzDevices::encode(u8 _cmd, u8 deviceId, u32 devicePayload)
{
	unsigned long retVal = 0xD;
	retVal = (retVal << 4) + _cmd;
	retVal = (retVal << 8) + deviceId;
	retVal = (retVal << 16) + devicePayload;
	return retVal;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Device Stuff 
deviceData HouzDevices::decode(u32 rawData, u32 nodeId) {
	deviceData decoded;
	decoded.raw = rawData;
	decoded.hasData = false;

	if (((rawData >> 28) == 0xD)) {
		decoded.node = nodeId;

		//parse values
		decoded.hasData = true;
		decoded.cmd = ((rawData >> 24) & 0x0F);
		decoded.id = ((rawData >> 16) & 0x0FF);
		decoded.payload = ((rawData) & 0x0000FFFF);
	}
	return decoded;
};

String HouzDevices::deviceToString(deviceData device) {
	if (!device.hasData) {return "{error=\"device has no data\"}";}
	String result;
	result = "{node:0x";
	result = result + String(device.node, HEX);
	result = result + ", id:0x";
	result = result + String(device.id, HEX);
	result = result + (", cmd:0x");
	result = result + String(device.cmd, HEX);
	result = result + (", payload:0x");
	result = result + String(device.payload, HEX);
	result = result + (", raw:0x");
	result = result + String(device.raw, HEX);
	result = result + ("}");
	return result;
};

String HouzDevices::packetToString(serverPacket packet) {
	return "{act:" + String(packet.act) +  ", dev:" + deviceToString(packet.dev) + "}";
};

String HouzDevices::packetToString(u8 action, deviceData device) {
	return "{act:" + String(action) +  ", dev:" + deviceToString(device) + "}";
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serial stuff 

bool HouzDevices::serialRead(){
  while (console->available() > 0) {
    int inChar = console->read();
	if (inChar == '\n') {
		handleCommand(serialBuffer);
		serialBuffer = "";
	}else{
		serialBuffer += (char)inChar;
		}
    }
};

void HouzDevices::handleCommand(String inCommand){
  if(inCommand.substring(0,1) != "N" || inCommand.length() > 10){
      console->println("\n{act:0, error: \"" + inCommand + "\"}");
      return;
    }
	if (!radio_status) { 
      console->println("\n{act:0, error: \"radio not ready\"}");
      return;
	 };

//parse device
    deviceData dev;
    dev.node = StrToHex(inCommand.substring(1,2));
    dev.cmd = StrToHex(inCommand.substring(3,4));
    dev.id = StrToHex(inCommand.substring(4,6));
    dev.payload = StrToHex(inCommand.substring(6,10));
    dev.raw = StrToHex(inCommand.substring(3,10));
	dev.hasData = true;
    
//send 
    radioSend(dev);
 };
 
