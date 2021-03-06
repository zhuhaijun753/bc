/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __VICPSYSTEM_H__
#define __VICPSYSTEM_H__

#include <stdint.h>
#include <binder/IBinder.h>
#include <binder/BinderService.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Mutex.h>
#include <utils/String16.h>
#include <utils/Vector.h>
#include "IVICPListener.h"
#include "IVICPSystem.h"

namespace android {

class myVICPListener:
	public RefBase
{
public:
	int32_t app_id;
	sp<IVICPListener> listener;

	myVICPListener(int32_t app_id, 
		const sp<IVICPListener>& listener) { 
		this->app_id = app_id;
		this->listener = listener;
	}
};

class VICPSystem : 
		public BnVICPSystem,
		public IBinder::DeathRecipient
{
public:
	char const* getServiceName() { 
		return "VICPSystem"; 
	}
	status_t publish(bool allowIsolated = false) {
		sp<IServiceManager> sm(defaultServiceManager());
		return sm->addService(
				String16(getServiceName()), this, allowIsolated);
	}
    void publishAndJoinThreadPool(bool allowIsolated = false) {
        publish(allowIsolated);
        joinThreadPool();
    }
    void notifyListeners(int32_t app_id, 
		uint8_t* buf, int32_t len);

private:
    Mutex mLock;
    Vector<sp<myVICPListener> > mListeners;

    void registerListener(int32_t app_id, 
		const sp<IVICPListener>& listener);
    void unregisterListener(int32_t app_id, 
		const sp<IVICPListener>& listener);
	void send(int32_t app_id, uint8_t *buf, int32_t len);
    status_t dump(int fd, const Vector<String16>& args);
    void binderDied(const wp<IBinder>& who);

	static void joinThreadPool() {
		sp<ProcessState> ps(ProcessState::self());
		ps->startThreadPool();
		ps->giveThreadPoolName();
		IPCThreadState::self()->joinThreadPool();
	}
};

};  // namespace android

#endif // __VICPSYSTEM_H__

