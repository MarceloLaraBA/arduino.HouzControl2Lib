/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Devices Config
#include <HouzSonyRemote.h>
#include <HouzIrCodes.h>
#include <QueueArray.h>
#include <RF24.h>

// commands
#define CMD_QUERY			0xA
#define CMD_VALUE			0xB
#define CMD_SET				0xC
#define CMD_EVENT			0xD
#define CMD_STATUS			0xE

// Master
#define server_node			0

// Office
#define office_node			0x1
#define office_AC			0x11 //Air Conditioner on/off
#define office_AC_temp		0x12 //Air Conditioner temperature
#define office_light		0x13 //ceiling light
#define office_switchLed	0x14 //wall switch led
#define office_switch  		0x15 //wall switch
#define office_ir			0x16 //ir 
#define external_lightSensor	0x17 //light sensor [0-1024]
#define external_tempSensor	0x18 //temperature [celsius /100]

// Bedroom
#define bedroom_node		0x2
#define bedroom_AC			0x23 //Air Conditioner on/off
#define bedroom_AC_temp		0x24 //Air Conditioner temperature
#define bedroom_light		0x25 //Ceiling light
#define bedroom_switchLed	0x26 //Wall switch led
#define bedroom_switch		0x27 //Wall switch
#define bedroom_ir			0x28 //ir

// Living
#define living_node			0x3	 //N1DC04F0F0\n
#define living_switch		0x31
#define living_switchLed	0x32 //N1DC320099\n 
#define living_mainLight	0x33 //N1DC330002\n - center
#define living_dicroLight	0x34 //N1DC340000\n - 2x4 dicro array
#define living_spotLight	0x35 //N1DC350000\n - spotlights
#define living_auxLight		0x36 //N1DC360000\n - guidance leds
#define living_arrLight		0x37 //N1DC370000\n - raw fx controller
#define living_AC			0x38 //N1DC380001\n
#define living_AC_temp		0x39 //N1DC390018\n


// Frontdesk
#define frontdesk_node		0x5

// device command
class deviceData {
public:
	u32  raw;
	bool hasData;

	u8	 id;
	u32  payload;

	byte node;
	u8	 cmd;
	u8	 media;

	String message;
};

class serverPacket {
public:
	u8 act;
	deviceData dev;
};

class radioPacket {
public:
	u32 message;
	byte node;
	u8 retries;
	unsigned long nextRetry;
};

// Server Contract //////////////////////////////////////////////////////////////////////////////////////////////////
#define action_log				0x0
#define action_rfSentOk			0x1
#define action_rfSentFail		0x2
#define action_rfReceived		0x3
#define action_irReceived		0x4

#define media_serial			0x0
#define media_rf				0x1
#define media_ir				0x2


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Declarations

class HouzDevices{
public:
	HouzDevices(byte NodeId, RF24 &_radio, byte _rfStatusLed, Stream &serial);
	HouzDevices(byte NodeId, RF24 &_radio, byte _rfStatusLed, Stream &serial, u8 dataPin, u8 latchPin, u8 clockPin);
	bool setup();

	//commands
	bool hasData();
	void pushData(deviceData device);
	deviceData getData();

	deviceData decode(u32 rawData, u32 rfNodeStation);
	unsigned long encode(u8 _cmd, u8 deviceId, u32 devicePayload);
	String deviceToString(deviceData device);
	String packetToString(serverPacket packet);	
	String packetToString(u8 action, deviceData device);	

	//radio
	bool radioSetup();
	bool radioReady();
	bool radioSend(deviceData device);
	bool radioSend(deviceData device, byte nodeId);
	bool radioSend(u8 deviceCmd, u8 deviceId, u32 devicePayload);
	bool radioSend(u8 deviceCmd, u8 deviceId, u32 devicePayload, byte nodeId);

	//serial
	bool serialRead();
	deviceData serialData();

	//output
	void setIo(u32 io, bool status);
	void setIo(word ioRawValue);
	word getIoStatus();
	bool getIo(u32 io);
	void ioAnim(u8 animLength, word anim[], u8 delay);

	//helpers
	unsigned long StrToHex(char str[]);
	unsigned long StrToHex(String str);
	deviceData decode(String str);
	void statusLedBlink();
	void statusLedBlink(int numTimes);

private:
	void init(byte NodeId, RF24 &_radio, byte _rfStatusLed, Stream & serial);

	//commands
	QueueArray<deviceData> commandsQueue;

	//radio
	RF24* radio;
	void setPipes(byte nodeId);

	byte node_id;
	String node_name;
	bool radio_status;
	byte rfStatusLed;
	unsigned long _radioPayLoad;
	uint8_t _radioNode;
	
	bool radioRead();
	bool radioWrite();
	QueueArray<radioPacket> radioSendQueue;

	//serial
	Stream* console;
	String serialBuffer;
	bool handleCommand(deviceData inCommand);

	//output
	bool ioReady;
	u8 dataPin;
	u8 latchPin;
	u8 clockPin;
	word ioStatus;
	void ioRender();

};