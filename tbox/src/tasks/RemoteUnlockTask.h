#ifndef MQTT_GUARD_RemoteUnlockTask_h__
#define MQTT_GUARD_RemoteUnlockTask_h__

#include "../inc/Task.h"
#include "../inc/Mqtt.h"
#include "../inc/Vehicle.h"
#include "./ErrorCode.h"
#include "./BCMessage.h"
#include "./TaskTable.h"

class RemoteUnlockTask : public Task {
public:
	const static int AppId = APPID_VKEY_ACTIVITION;
	const static int Druation = 30 * 10000;
	static Task* Create();
	RemoteUnlockTask():Task(APPID_VKEY_ACTIVITION,true),pkg(pkg){}
	
	void sendResponseError(Operation::Result ret);
	
	void sendResponseUnlocked();
	virtual void doTask()OVERRIDE;	
	void sendAck();
	void sendResponseTimeOut();
private:
	Timestamp   expireTime;
	bcp_packet_t* pkg;
};
#endif // GUARD_RemoteUnlockTask_h__
