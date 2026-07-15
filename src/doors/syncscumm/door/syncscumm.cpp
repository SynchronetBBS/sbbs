/* SyncSCUMM -- ScummVM as a Synchronet door.
 * M1 skeleton: OSystem_Synchronet is a null-equivalent backend; the
 * frame-dump graphics manager (video_dump.cpp) is swapped in by
 * initBackend(). Terminal I/O via libtermgfx arrives in M2+.
 * GPLv2+, like the ScummVM tree this compiles into.
 */

#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#define FORBIDDEN_SYMBOL_EXCEPTION_FILE
#define FORBIDDEN_SYMBOL_EXCEPTION_stdout
#define FORBIDDEN_SYMBOL_EXCEPTION_stderr
#define FORBIDDEN_SYMBOL_EXCEPTION_fputs
#define FORBIDDEN_SYMBOL_EXCEPTION_exit
#define FORBIDDEN_SYMBOL_EXCEPTION_time_h
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "common/scummsys.h"

#if defined(USE_SYNCHRONET_DRIVER)

#include "common/events.h"
#include "backends/modular-backend.h"
#include "backends/mutex/null/null-mutex.h"
#include "backends/saves/default/default-saves.h"
#include "backends/timer/default/default-timer.h"
#include "backends/events/default/default-events.h"
#include "backends/mixer/null/null-mixer.h"
#include "backends/graphics/null/null-graphics.h"
#include "video_dump.h"
#include "backends/fs/posix/posix-fs-factory.h"
#include "base/main.h"

class OSystem_Synchronet : public ModularMixerBackend, public ModularGraphicsBackend, Common::EventSource {
public:
	OSystem_Synchronet();
	virtual ~OSystem_Synchronet() {}

	void initBackend() override;
	bool pollEvent(Common::Event &event) override;
	Common::MutexInternal *createMutex() override;
	uint32 getMillis(bool skipRecord = false) override;
	void delayMillis(uint msecs) override;
	void getTimeAndDate(TimeDate &td, bool skipRecord = false) const override;
	void quit() override;
	void logMessage(LogMessageType::Type type, const char *message) override;

private:
	timeval _startTime;
};

OSystem_Synchronet::OSystem_Synchronet() {
	_fsFactory = new POSIXFilesystemFactory();
}

void OSystem_Synchronet::initBackend() {
	gettimeofday(&_startTime, 0);
	_savefileManager = new DefaultSaveFileManager();
	_timerManager = new DefaultTimerManager();
	_eventManager = new DefaultEventManager(this);
	_mixerManager = new NullMixerManager();
	_mixerManager->init();
	_graphicsManager = new SyncscummDumpGraphicsManager();
	BaseBackend::initBackend();
}

bool OSystem_Synchronet::pollEvent(Common::Event &event) {
	((DefaultTimerManager *)getTimerManager())->checkTimers();
	((NullMixerManager *)_mixerManager)->update(1);
	return false;
}

Common::MutexInternal *OSystem_Synchronet::createMutex() {
	return new NullMutexInternal();
}

uint32 OSystem_Synchronet::getMillis(bool skipRecord) {
	timeval now;
	gettimeofday(&now, 0);
	return (uint32)((now.tv_sec - _startTime.tv_sec) * 1000 +
		(now.tv_usec - _startTime.tv_usec) / 1000);
}

void OSystem_Synchronet::delayMillis(uint msecs) {
	usleep(msecs * 1000);
}

void OSystem_Synchronet::getTimeAndDate(TimeDate &td, bool skipRecord) const {
	time_t curTime = time(0);
	struct tm t = *localtime(&curTime);
	td.tm_sec = t.tm_sec;
	td.tm_min = t.tm_min;
	td.tm_hour = t.tm_hour;
	td.tm_mday = t.tm_mday;
	td.tm_mon = t.tm_mon;
	td.tm_year = t.tm_year;
	td.tm_wday = t.tm_wday;
}

void OSystem_Synchronet::quit() {
	destroy();
	exit(0);
}

void OSystem_Synchronet::logMessage(LogMessageType::Type type, const char *message) {
	FILE *output = (type == LogMessageType::kInfo || type == LogMessageType::kDebug)
		? stdout : stderr;
	fputs(message, output);
	fflush(output);
}

int main(int argc, char *argv[]) {
	g_system = new OSystem_Synchronet();
	assert(g_system);
	int res = scummvm_main(argc, argv);
	g_system->destroy();
	return res;
}

#endif /* USE_SYNCHRONET_DRIVER */
