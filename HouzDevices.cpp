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
#define rfChannel		0x5D   
#define rfPipeMain		0xF0F0F0F0AA
#define rfPipeBedr		0xF0F0F0F066
#define rfPipeSensors	0xF0F0F0F011

const uint64_t talk_pipes[5] = { 0xF0F0F0F0D2LL, 0xF0F0F0F0C3LL, 0xF0F0F0F0B4LL, 0xF0F0F0F0A5LL, 0xF0F0F0F096LL };
const uint64_t listen_pipes[5] = { 0x3A3A3A3AD2LL, 0x3A3A3A3AC3LL, 0x3A3A3A3AB4LL, 0x3A3A3A3AA5LL, 0x3A3A3A3A96LL };

unsigned long _radioPayLoad;

HouzDevices::HouzDevices(byte NodeId, RF24 &_radio, byte _rfStatusLed, Print &serial){
	console = &serial;
	radio = &_radio;
	node_id = NodeId;
};

bool HouzDevices::radioSetup(byte _rfStatusLed)
{
	console->println("::RF setup");
	radio_status = 0;

	//determine device pipes
	console->print("device: ");
	console->println(node_id);

	printf_begin();
	radio->begin();
	radio->setPALevel(RF24_PA_HIGH);
	radio->setChannel(rfChannel);
	radio->openWritingPipe(rfPipeMain);
	radio->openReadingPipe(1, rfPipeSensors);

	//radio status
	radio_status = (rfChannel == radio->getChannel()); //test if radio is enabled
	console->print("radio: ");
	console->println(radio_status ? "online" : "offline");
	if (radio_status) {
		radio->printDetails();
		radio->startListening();
		radio_status = 1;
	}
	rfStatusLed = _rfStatusLed;
	pinMode(rfStatusLed, OUTPUT);
	return true;
};

bool HouzDevices::radioReady()
{
	return radio_status;
};

bool HouzDevices::radioRead()
{
	//if radio is not enabled, discard anything
	if (!radio_status) { return false; };

	if (radio->available()) {
		while (radio->available()) {             // While there is data ready
			radio->read(&_radioPayLoad, sizeof(unsigned long));  // Get the payload
			digitalWrite(rfStatusLed, HIGH);       // Notify receive
		}

		console->print("RF recv> ");
		console->println(_radioPayLoad, HEX);

		delay(250);
		radio->startListening();
		digitalWrite(rfStatusLed, LOW);
		return true;
	}
	else
	{
		return false;
	}

};

bool HouzDevices::radioSend(u8 deviceCmd, u8 deviceId, u32 devicePayload) {
	return radioWrite(encode(deviceCmd, deviceId, devicePayload));
}

bool HouzDevices::radioWrite(u32 rfMessage) {
	if (!radio_status) { return false; };
	bool result; 
	radio->stopListening();
	if (!radio->write(&rfMessage, sizeof(unsigned long))) {
		console->println(F("failed"));
		result = false;
	}
	else {
		console->print("sent> 0x");
		console->println(rfMessage, HEX);
		result = true;
	}
	radio->startListening();
	return result;
};

unsigned long HouzDevices::encode(byte _cmd, u8 deviceId, u32 devicePayload)
{
	unsigned long retVal = 0xD;
	retVal = (retVal << 4) + _cmd;
	retVal = (retVal << 8) + deviceId;
	retVal = (retVal << 16) + devicePayload;
	return retVal;
}

deviceData HouzDevices::decode(u32 rawData) {
	deviceData decoded;
	decoded.raw = rawData;
	decoded.hasData = false;

	if (((rawData >> 28) == 0xD)) {
		//parse values
		decoded.hasData = true;
		decoded.cmd = ((rawData >> 24) & 0x0F);
		decoded.id = ((rawData >> 16) & 0x0FF);
		decoded.payload = ((rawData) & 0x0000FFFF);
	}
	return decoded;
};

deviceData HouzDevices::receivedData() {
	deviceData device = decode(_radioPayLoad);
	_radioPayLoad = 0;
	return device;
};

String HouzDevices::deviceToString(deviceData device) {
	if (!device.hasData) {
		return "[device has no data]";
	}
	String result;
	result = "[id:";
	result = result + (device.id);
	result = result + ("|cmd:");
	result = result + (device.cmd);
	result = result + ("|payload:");
	result = result + (device.payload);
	result = result + ("|raw:");
	result = result + (device.raw);
	result = result + ("]");
	return result;
};
