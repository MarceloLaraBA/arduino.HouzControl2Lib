/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Devices Config
#include <HouzSonyRemote.h>
#include <HouzIrCodes.h>
#include <RF24.h>

// commands
#define CMD_QUERY			0xA
#define CMD_VALUE			0xB
#define CMD_SET				0xC


#define server_node			0

// External
#define external_node		0x1
#define external_lightSensor	0x11 //light sensor [0-1024]
#define external_tempSensor	0x12 //temperature [celsius /100]

// Bedroom
#define bedroom_node		0x2
#define bedroom_AC			0x23 //Air Conditioner
#define bedroom_light		0x24 //Ceiling light
#define bedroom_switchLed	0x25 //Wall switch led

// Office
#define office_node			0x3
#define office_AC			0x31 //Air Conditioner
#define office_light		0x32 //Air Conditioner


// Living
#define living_node			0x4

// Frontdesk
#define frontdesk_node		0x5


// Decoded result
class deviceData {
public:
	bool hasData;
	u8	id;
	u8	cmd;
	u32	payload;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Declarations

class HouzDevices{
public:
	HouzDevices(byte NodeId, RF24 &_radio, byte _rfStatusLed, Print &serial);

	unsigned long encode(u8 _cmd, u8 deviceId, u32 devicePayload);
	deviceData decode(u32 rawData);

	bool radioSetup(byte _rfStatusLed);
	bool radioReady();
	bool radioSend(u8 deviceCmd, u8 deviceId, u32 devicePayload);

	bool radioRead();
	void radioWrite(u32 rfMessage);
	deviceData receivedData();

private:

	byte node_id;
	bool radio_status;
	byte rfStatusLed;
	unsigned long _radioPayLoad;
	Print* console;
	RF24* radio;

};