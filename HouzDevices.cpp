/*
Name:		master_logger.ino
Created:	13-Apr-17 22:55:23
Author:	DarkAngel
*/

#include "Arduino.h"
#include "HouzDevices.h"
#include <QueueArray.h>

#include <RF24.h>
#include <printf.h>

//radio setup
#define rfChannel	0x5B   
#define rf_server_tx	0xA0
#define rf_server_rx	0xB0

#define rf_office_tx	0xA1
#define rf_office_rx	0xB1

#define rf_bedroom_tx	0xA2
#define rf_bedroom_rx	0xB2

#define rf_living_tx	0xA3
#define rf_living_rx	0xB3

#define rf_led_low 32
#define rf_led_idle 64
#define rf_led_high 255


//const uint64_t talk_pipes[5] = { 0xF0F0F0F0D2LL, 0xF0F0F0F0C3LL, 0xF0F0F0F0B4LL, 0xF0F0F0F0A5LL, 0xF0F0F0F096LL };
//const uint64_t listen_pipes[5] = { 0x3A3A3A3AD2LL, 0x3A3A3A3AC3LL, 0x3A3A3A3AB4LL, 0x3A3A3A3AA5LL, 0x3A3A3A3A96LL };

unsigned long _radioPayLoad;
HouzDevices::HouzDevices(byte NodeId, RF24 &_radio, byte _rfStatusLed, Stream &serial) {
	init(NodeId, _radio, _rfStatusLed, serial);
};

HouzDevices::HouzDevices(byte NodeId, RF24 &_radio, byte _rfStatusLed, Stream &serial, u8 _dataPin, u8 _latchPin, u8 _clockPin) {
	//74HC595 shift register setup
	ioReady = true;
	dataPin = _dataPin;
	latchPin = _latchPin;
	clockPin = _clockPin;
	ioStatus = 0;
	pinMode(latchPin, OUTPUT);
	pinMode(clockPin, OUTPUT);
	pinMode(dataPin, OUTPUT);
	digitalWrite(dataPin, 0);
	digitalWrite(clockPin, 0);
	digitalWrite(latchPin, 0);
	shiftOut(dataPin, clockPin, MSBFIRST, 0);
	shiftOut(dataPin, clockPin, MSBFIRST, 0);
	digitalWrite(latchPin, 1);

	init(NodeId, _radio, _rfStatusLed, serial);
};

void HouzDevices::init(byte NodeId, RF24 &_radio, byte _rfStatusLed, Stream &serial) {
	console = &serial;
	radio = &_radio;
	node_id = NodeId;
	rfStatusLed = _rfStatusLed;
	pinMode(rfStatusLed, OUTPUT);

}



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

void HouzDevices::statusLedBlink() {
	statusLedBlink(1);
};
void HouzDevices::statusLedBlink(int numTimes) {
	int x;
	for (int _t = 0; _t < numTimes; _t++)	{
		x = 2;
		for (int i = rf_led_high - rf_led_idle; i > rf_led_idle; i = i + x) {
			analogWrite(rfStatusLed, i);
			if (i > rf_led_high) x = -2;
			delay(2);
		}
	}

};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Radio Stuff 
bool HouzDevices::radioSetup()
{
	console->println("::RF setup");
	radio_status = 0;
	pinMode(rfStatusLed, OUTPUT);
	analogWrite(rfStatusLed, rf_led_low);

	//setup rf24 module
	printf_begin();
	radio->begin();
	radio->setPALevel(RF24_PA_MAX); //RF24_PA_HIGH | RF24_PA_LOW
	radio->setDataRate(RF24_1MBPS);
	radio->enableDynamicAck();
	radio->setCRCLength(RF24_CRC_8);
	radio->setChannel(rfChannel);
	radio->setRetries(15, 15);

	//determine device pipes
	setPipes(node_id);
	console->print("device: ");
	console->print(node_name);
	console->print(" [");
	console->print(node_id);
	console->println("]");

	//radio status
	radio_status = (rfChannel == radio->getChannel()); //test if radio is enabled
	console->print("status: ");
	console->println(radio_status ? "online" : "offline");

	if (!radio_status) return false;
	radio->printDetails();
	radio->startListening();
	statusLedBlink(5);


	//notify server
	if(node_id!=server_node)
		radioSend(CMD_STATUS, node_id, 0xF0F0);

	return true;
};

void HouzDevices::setPipes(byte nodeId) {
	switch (node_id)
	{
	case server_node: node_name = "server_node";
		radio->openWritingPipe(rf_server_tx);
		radio->openReadingPipe(1, rf_office_tx);
		radio->openReadingPipe(2, rf_bedroom_tx);
		radio->openReadingPipe(3, rf_living_tx);
		break;

	case office_node: node_name = "office_node";
		radio->openWritingPipe(rf_office_tx);
		radio->openReadingPipe(1, rf_office_rx);
		break;

	case bedroom_node: node_name = "bedroom_node";
		radio->openWritingPipe(rf_bedroom_tx);
		radio->openReadingPipe(1, rf_bedroom_rx);
		break;

	case living_node: node_name = "living_node";
		radio->openWritingPipe(rf_living_tx);
		radio->openReadingPipe(1, rf_living_rx);
		break;

	default:
		break;
	}
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
	uint8_t pipe_num;
	if (!radio->available(&_radioNode)) { return false; };

	delay(50); 
	statusLedBlink();

	//get payload
	while (radio->available()) {
		radio->read(&_radioPayLoad, sizeof(unsigned long));
	}

	//decode payload
	_device = decode(_radioPayLoad, _radioNode);
	_radioNode = 0;
	_radioPayLoad = 0;

	//server must notify host
	if (node_id == server_node) 
		console->println(packetToString(action_rfReceived, receivedData()));

	//prepare for next packet
	radio->startListening();

	//handle pong back
	if (_device.hasData && _device.id == node_id && _device.cmd == CMD_QUERY) {
		radioSend(CMD_STATUS, _device.id, !_device.payload);
		return false;
	}

	return _device.hasData;
};

deviceData HouzDevices::receivedData() {
	return _device;
};

bool HouzDevices::radioSend(u8 deviceCmd, u8 deviceId, u32 devicePayload) {
	return radioSend(deviceCmd, deviceId, devicePayload, server_node);
};

bool HouzDevices::radioSend(deviceData device) {
	bool result = radioSend(device.cmd, device.id, device.payload, device.node);
	console->println(packetToString((result ? action_rfSentOk : action_rfSentFail), device));
	return true;
};

bool HouzDevices::radioSend(u8 deviceCmd, u8 deviceId, u32 devicePayload, byte nodeId) {

	//enqueue send
	radioPacket packet;
	packet.message = encode(deviceCmd, deviceId, devicePayload);
	packet.node = nodeId;
	packet.retries = 0;
	radioSendQueue.enqueue(packet);
	console->println("[radioSend] packet enqueued");
	return true;

	//return radioWrite(encode(deviceCmd, deviceId, devicePayload), nodeId);
};

void HouzDevices::taskManager() {
	radioWrite(); //radio send
};

bool HouzDevices::radioWrite() {
	if (!radio_status) return false;
	if (radioSendQueue.isEmpty()) return true;

	radioPacket packet = radioSendQueue.peek();

	//wait when retrying
	if (packet.retries > 0) {
		if (packet.nextRetry > millis()) {
			return true;
		}
	}
	packet = radioSendQueue.dequeue();
	//open write pipe
	uint64_t writeAddress;
	radio->stopListening();
	switch (node_id) {
	case server_node:
		switch (packet.node) {
		case office_node: writeAddress = rf_office_rx; break;
		case bedroom_node: writeAddress = rf_bedroom_rx; break;
		case living_node: writeAddress = rf_living_rx; break;
		}
		radio->openWritingPipe(writeAddress);
	};

	//send
	bool result = 0;
	result = radio->write(&packet.message, sizeof(unsigned long), 0);
	if (!result) {
		packet.retries++;
		packet.nextRetry = millis() + 1000;
		if (packet.retries <= 10) {
			console->print("[radioWrite error] ");
			console->print(packet.retries);
			console->println("");
			radioSendQueue.enqueue(packet);
		}
		else
			if (node_id != server_node)
				console->println("[radioWrite error] packet dropped..");
	}
	else {
		console->print("[radioWrite ok] ");
		console->println(deviceToString(decode(packet.message, packet.node)));
	}

	radio->startListening();
	return result;
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

unsigned long HouzDevices::encode(u8 _cmd, u8 deviceId, u32 devicePayload)
{
	unsigned long retVal = 0xD;
	retVal = (retVal << 4) + _cmd;
	retVal = (retVal << 8) + deviceId;
	retVal = (retVal << 16) + devicePayload;
	return retVal;
}

deviceData HouzDevices::decode(String str) {
	deviceData dev;
	dev.node = StrToHex(str.substring(1, 2));
	dev.cmd = StrToHex(str.substring(3, 4));
	dev.id = StrToHex(str.substring(4, 6));
	dev.payload = StrToHex(str.substring(6, 10));
	dev.raw = StrToHex(str.substring(3, 10));
	dev.hasData = (dev.id != 0);
	return dev;
}

String HouzDevices::deviceToString(deviceData device) {
	if (!device.hasData) {return "{error:\"device has no data\",raw:\"" + String(device.raw, HEX) + "\"}";}
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
		return handleCommand(serialBuffer);
	}else{
		serialBuffer += (char)inChar;
	}
  }
  return false;
}
deviceData HouzDevices::serialData() {
	if (serialBuffer[0] != 'N') {
		console->println("\n> " + serialBuffer);
		return;
	}

	deviceData dev = decode(serialBuffer);
	serialBuffer = "";
	return dev;
};

bool HouzDevices::handleCommand(String inCommand){
	
//unknown command
	if (serialBuffer.length() != 10 || serialBuffer[0] != 'N' || serialBuffer[2] != 'D') {
		_device.hasData = false;

		//ignore command on server
		if (node_id == server_node) {
			console->println("\n{act:0, error: \"" + inCommand + "\"}");
			serialBuffer = "";
			return false;
		}

		return true;
	};

//decode device
	_device = decode(serialBuffer);
	if (!_device.hasData) return true;
	serialBuffer = "";


//radio must be ready
	if (!radio_status) {
		console->println("\n{act:0, error: \"radio not ready\"}");
		return false;
	};

//send 
    radioSend(_device);
	return false;
};
 


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IO stuff | shifted outputs

void HouzDevices::setIo(u32 io, bool status) {
	//set bit
	if(status)
		bitSet(ioStatus, io);
	else
		bitClear(ioStatus, io);

	console->print("\nio: ");
	console->println(ioStatus);

	//push
	ioRender();
};
void HouzDevices::setIo(word ioRawValue) {
	ioStatus = ioRawValue;
	ioRender();
};


bool HouzDevices::getIo(u32 io) {
	return bitRead(ioStatus, io);
};

void HouzDevices::ioAnim(u8 animLength, word anim[], u8 interval) {
	for (int i = 0; i < animLength; i++) {
		ioStatus = anim[i];
		ioRender();
		delay(interval);
	}
}

void HouzDevices::ioRender() {
	digitalWrite(latchPin, LOW);
	shiftOut(dataPin, clockPin, MSBFIRST, (ioStatus >> 8));
	shiftOut(dataPin, clockPin, MSBFIRST, ioStatus);
	digitalWrite(latchPin, HIGH);

	console->print("ioRender: ");
	console->println(ioStatus, BIN);
}

word HouzDevices::getIoStatus() {
	return ioStatus;
}