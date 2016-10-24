#ifndef MQTT_GUARD_Task_h__
#define MQTT_GUARD_Task_h__

//#include "./Message.h"
#include "./Event.h"

class MessageQueue
{
private:
	ThreadEvent event;
	EventQueue  eventArgs;
public:
	struct Args
	{
		AppEvent::e e;
		u32 param1;
		u32 param2;
		void* data;
	};
	bool post(AppEvent::e e, u32 param1, u32 param2, void* data);
	bool out(Args& args);
	bool out(AppEvent::e& e, u32& param1, u32& param2, void*& data);
	ThreadEvent::WaitResult wait(u32 millSecond);
};

class Task : public Thread
{
	friend class Application;
	friend class TaskList;
public:
	Task(u16 appId,bool async);
	~Task();
	u16  getApplicationId() { return appID; }
	u64  getSequenceId() { return seqID; }
public:
	bool handlePackage(bcp_packet_t* pkg);
	virtual void onEvent(AppEvent::e e, u32 param1, u32 param2, void* data);
protected:
	//the function should be OVERRIDE by its subclass
	virtual void doTask(){return;}
private:
	//called by Application
	virtual void run()OVERRIDE;
private:
	////list node in refList//////////////////////////////////////////////////////////////////////
	Task* prev;
	Task* next;
protected:
	u16  appID;	//ref in bcp
	u64  seqID; //ref in bcp
	bool isAsync;
	MessageQueue msgQueue;
};
#endif // GUARD_Task_h__
