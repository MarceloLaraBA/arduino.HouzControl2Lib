/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Devices Config
#include <HouzSonyRemote.h>
#include <HouzIrCodes.h>
#include <RF24.h>

// commands
#define CMD_QUERY			0xA
#define CMD_VALUE			0xB
#define CMD_SET				0xC
#define CMD_STATUS			0xD

#define server_node			0

// External
#define external_node		0x1
#define external_lightSensor	0x11 //light sensor [0-1024]
#define external_tempSensor	0x12 //temperature [celsius /100]

// Bedroom
#define bedroom_node		0x2
#define bedroom_AC			0x23 //Air Conditioner on/off
#define bedroom_AC_temp		0x24 //Air Conditioner temperature
#define bedroom_light		0x25 //Ceiling light
#define bedroom_switchLed	0x26 //Wall switch led

// Office
#define office_node			0x3
#define office_AC			0x31 //Air Conditioner on/off
#define office_AC_temp		0x32 //Air Conditioner temperature
#define office_light		0x33 //Air Conditioner

// Living
#define living_node			0x4

// Frontdesk
#define frontdesk_node		0x5

// Decoded result
class deviceData {
public:
	u32 raw;
	bool hasData;

	u8	id;
	u8	media;
	u32 node;
	u8	cmd;
	u32 payload;
};

class serverPacket {
public:
	u8 act;
	deviceData dev;
};

// Server Contract //////////////////////////////////////////////////////////////////////////////////////////////////
#define action_log				0x0
#define action_rfSentOk			0x1
#define action_rfSentFail		0x2
#define action_rfReceived		0x3
#define action_irReceived		0x4

#define media_serial			0x0
#define media_rf				0x1
#define media_serial			0x2


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Declarations

class HouzDevices{
public:
	HouzDevices(byte NodeId, RF24 &_radio, byte _rfStatusLed, Stream &serial);

	deviceData receivedData();
	deviceData decode(u32 rawData, u32 rfNodeStation);
	unsigned long encode(u8 _cmd, u8 deviceId, u32 devicePayload);
	void handleCommand(String inCommand);
	String deviceToString(deviceData device);
	String packetToString(serverPacket packet);	
	String packetToString(u8 action, deviceData device);	

	bool radioSetup();
	bool radioReady();
	bool radioRead();
	bool radioWrite(u32 rfMessage);
	bool radioWrite(u32 rfMessage, byte rfNodeStation);
	bool radioSend(deviceData device);
	bool radioSend(u8 deviceCmd, u8 deviceId, u32 devicePayload);

	bool serialRead();
	deviceData serialData();
	

	unsigned long StrToHex(char str[]);
	unsigned long StrToHex(String str);


private:

	byte node_id;
	bool radio_status;
	byte rfStatusLed;
	unsigned long _radioPayLoad;
	unsigned long _pipe_num;
	Stream* console;
	RF24* radio;
	void setPipes(byte nodeId);

	String serialBuffer;
};