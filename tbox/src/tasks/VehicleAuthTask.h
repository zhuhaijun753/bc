#ifndef GUARD_VehicleAuthTask_h__
#define GUARD_VehicleAuthTask_h__

#include "../inc/Task.h"
#include "../inc/Mqtt.h"
#include "./BCMessage.h"
#include "./TaskTable.h"
class VehicleAuthTask : public Task {
public:
	const static int AppId = APPID_AUTHENTICATION;
	VehicleAuthTask(u32 appId);
	static Task* Create(u32 appId);
protected:
	virtual void doTask();
private:
	void reqAuth();
private:
	Timestamp fire;
};
#endif // GUARD_VehicleAuthTask_h__
