#ifndef MQTT_GUARD_RemoteUnlockTask_h__
#define MQTT_GUARD_RemoteUnlockTask_h__

#include "./Task.h"
class RemoteUnlockTask : public Task {
public:
	virtual void run()override
	{
		e.wait(1000);
	}
	virtual bool handlePackage(void* data, int len) {
		e.post();
		return false;
	}
private:
	ThreadEvent e;
};
#endif // GUARD_RemoteUnlockTask_h__
