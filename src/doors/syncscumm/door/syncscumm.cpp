/* SyncSCUMM -- ScummVM as a Synchronet door.
 * M1 skeleton: OSystem_Synchronet is a null-equivalent backend; the
 * frame-dump graphics manager (video_dump.cpp) is swapped in by
 * initBackend(). Terminal I/O via libtermgfx arrives in M2+.
 * GPLv2+, like the ScummVM tree this compiles into.
 */

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

/* xpdev's ini_file.h (-> genwrap.h) declares things like strupr()/strlwr()
 * and uses printf in an attribute -- names common/forbidden.h poisons into
 * unusable macros once common/scummsys.h has been included. Pull it in
 * first, before scummsys.h does that poisoning (ini_file.h wraps itself in
 * extern "C" already; no wrapper needed here). */
#include "ini_file.h"

#define FORBIDDEN_SYMBOL_EXCEPTION_FILE
#define FORBIDDEN_SYMBOL_EXCEPTION_stdout
#define FORBIDDEN_SYMBOL_EXCEPTION_stderr
#define FORBIDDEN_SYMBOL_EXCEPTION_fputs
#define FORBIDDEN_SYMBOL_EXCEPTION_exit
#define FORBIDDEN_SYMBOL_EXCEPTION_time_h
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv
/* Sysop subtitles.ini read (resolveSubtitles(), below) opens it with plain
 * libc stdio -- xpdev's iniReadFile() takes a FILE*, matching the house
 * pattern in syncduke_config.c/syncretro_config.c/syncmoo1_config.c. */
#define FORBIDDEN_SYMBOL_EXCEPTION_fopen
#define FORBIDDEN_SYMBOL_EXCEPTION_fclose

#include "common/scummsys.h"

#if defined(USE_SYNCHRONET_DRIVER)

#include "common/events.h"
#include "backends/fs/posix/posix-fs.h"
#include "common/fs.h"
#include "backends/modular-backend.h"
#include "backends/mutex/null/null-mutex.h"
#include "backends/saves/default/default-saves.h"
#include "backends/timer/default/default-timer.h"
#include "backends/events/default/default-events.h"
#include "backends/graphics/null/null-graphics.h"
#include "audio_term.h"
#include "video_dump.h"
#include "video_term.h"
#include "backends/fs/posix/posix-fs-factory.h"
#include "base/main.h"
#include "common/config-manager.h"
#include "common/str.h"

extern "C" {
#include "sst_io.h"
}

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
	void addSysArchivesToSearchSet(Common::SearchSet &s, int priority) override;

private:
	timeval _startTime;
};

OSystem_Synchronet::OSystem_Synchronet() {
	_fsFactory = new POSIXFilesystemFactory();
}

// Subtitles: user > sysop > auto (DESIGN.md M2 follow-up). Decides whether
// dialogue is superimposed as text, since the BASS CD talkie's speech has
// no audio path to play through until M4.
//
// Hook-point safety: called from the top of initBackend(), below. main.cpp
// makes its own ordering guarantee explicit right at the system.initBackend()
// call site: "Init the backend. Must take place after all config data
// (including the command line params) was read." By that point
// scummvm_main() has already run ConfMan.loadConfigFile()/
// loadDefaultConfigFile() (the "-c" file) AND Base::processSettings() (which
// resolves our door's trailing "sky" argument to the active game-domain
// target -- pulling in that domain's on-disk data if the "-c" file already
// has a matching "[sky]" section). So every persistent ConfMan domain is
// fully populated, and nothing has consumed or overwritten "subtitles" yet,
// by the time initBackend() -- and this function -- run. Doing this lazily
// on first updateScreen() would work too, but only after already drawing at
// least one frame with the wrong answer; this hook has no such window.
static void resolveSubtitles() {
	// 1) USER: a persistent domain (the active game-domain target loaded
	// from the "-c" file's own section, or its global "[scummvm]" app
	// domain) already has an opinion -- leave it alone entirely. Checked
	// directly against those two Domains (not the generic ConfMan::hasKey(),
	// which also matches kTransientDomain/kSessionDomain) since this is the
	// one caller that specifically must NOT see its own not-yet-written
	// session default and mistake it for a user preference.
	Common::ConfigManager::Domain *active = ConfMan.getActiveDomain();
	Common::ConfigManager::Domain *app =
		ConfMan.getDomain(Common::ConfigManager::kApplicationDomain);
	bool userSet = (active && active->contains("subtitles")) ||
	               (app && app->contains("subtitles"));
	if (userSet) {
		fputs("syncscumm: subtitles: user preference respected\n", stderr);
		return;
	}

	// 2) SYSOP: syncscumm.ini, read relative to CWD -- the door's
	// startup_dir (xtrn/syncscumm/install-xtrn.ini's startup_dir comment: the
	// vendored scummvm/ tree, i.e. the process's actual CWD when the
	// Terminal Server execs this binary). A missing file, missing key, or an
	// explicit "auto" all defer to step 3; only "on"/"off" decide here.
	int sysopOn = -1;   // -1 = no opinion (auto), 0 = off, 1 = on
	FILE *f = fopen("syncscumm.ini", "r");
	if (f != NULL) {
		str_list_t ini = iniReadFile(f);
		fclose(f);
		char val[INI_MAX_VALUE_LEN];
		iniGetString(ini, ROOT_SECTION, "subtitles", "auto", val);
		if (scumm_stricmp(val, "on") == 0)
			sysopOn = 1;
		else if (scumm_stricmp(val, "off") == 0)
			sysopOn = 0;
		strListFree(&ini);
	}
	if (sysopOn >= 0) {
		ConfMan.setBool("subtitles", sysopOn != 0, Common::ConfigManager::kSessionDomain);
		fputs(sysopOn ? "syncscumm: subtitles: sysop on\n" : "syncscumm: subtitles: sysop off\n", stderr);
		return;
	}

	// 3) AUTO: on with no working audio this session, off otherwise. Written
	// to kSessionDomain -- the same domain ScummVM's own "-n"/"--subtitles"
	// command-line flag would use for this key (base/commandLine.cpp's
	// sessionSettings[] list), i.e. never saved to disk, exactly like a
	// command-line override. sst_io_audio_available() answers for the
	// SESSION, not just the terminal: a sysop "[audio] enabled = false"
	// answers instantly with no probe at all, otherwise it briefly waits for
	// the terminal's capability reply (see sst_io.h) -- a confirmed digital
	// audio tier resolves off, anything else -- tone-only, silent, headless
	// -- resolves on.
	bool audio = sst_io_audio_available() != 0;
	ConfMan.setBool("subtitles", !audio, Common::ConfigManager::kSessionDomain);
	fputs(audio ? "syncscumm: subtitles auto -> off (audio available this session)\n"
	            : "syncscumm: subtitles auto -> on (no audio this session)\n", stderr);
}

void OSystem_Synchronet::initBackend() {
	gettimeofday(&_startTime, 0);
	resolveSubtitles();
	_savefileManager = new DefaultSaveFileManager();
	_timerManager = new DefaultTimerManager();
	_eventManager = new DefaultEventManager(this);
	// After resolveSubtitles(), deliberately: that is what decides whether
	// this session will hear audio -- the sysop switch first, with no
	// blocking at all when it is off, otherwise a bounded wait for the
	// terminal's capability reply (see sst_io.h) -- and the answer decides
	// whether sst_io_audio_stream() streams or discards what we pull.
	_mixerManager = new SyncscummMixerManager();
	_mixerManager->init();
	_graphicsManager = new SyncscummTermGraphicsManager();
	BaseBackend::initBackend();
}

bool OSystem_Synchronet::pollEvent(Common::Event &event) {
	((DefaultTimerManager *)getTimerManager())->checkTimers();
	((SyncscummMixerManager *)_mixerManager)->tick();

	sst_io_pump();
	if (sst_io_quit_requested() || sst_io_hung_up()) {
		static bool sentQuit = false;
		if (!sentQuit) {
			sentQuit = true;
			event.type = Common::EVENT_QUIT;
			return true;
		}
	}

	sst_input_event_t iev;
	if (sst_io_next_event(&iev)) {
		switch (iev.type) {
		case SST_EV_MOUSE_MOVE:
			_graphicsManager->warpMouse(iev.x, iev.y);   /* compositor draws the cursor here */
			event.type = Common::EVENT_MOUSEMOVE;
			event.mouse = Common::Point(iev.x, iev.y);
			return true;
		case SST_EV_MOUSE_DOWN:
		case SST_EV_MOUSE_UP: {
			bool down = (iev.type == SST_EV_MOUSE_DOWN);
			event.mouse = Common::Point(iev.x, iev.y);
			if (iev.button == 0)
				event.type = down ? Common::EVENT_LBUTTONDOWN : Common::EVENT_LBUTTONUP;
			else if (iev.button == 2)
				event.type = down ? Common::EVENT_RBUTTONDOWN : Common::EVENT_RBUTTONUP;
			else
				event.type = down ? Common::EVENT_MBUTTONDOWN : Common::EVENT_MBUTTONUP;
			return true;
		}
		case SST_EV_WHEEL:
			event.mouse = Common::Point(iev.x, iev.y);
			event.type = (iev.wheel < 0) ? Common::EVENT_WHEELUP : Common::EVENT_WHEELDOWN;
			return true;
		default:
			break;   /* key events: later task */
		}
	}
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

// Engine runtime data (sky.cpt, lure.dat, ...) and, later, GUI themes are
// found via the search set instead of --extrapath: SYNCSCUMM_DATA names the
// directory (the door install sets it; dev runs point it at
// scummvm/dists/engine-data). SearchMan invokes this at priority -1, so
// explicit game paths always win.
void OSystem_Synchronet::addSysArchivesToSearchSet(Common::SearchSet &s, int priority) {
	const char *data = getenv("SYNCSCUMM_DATA");
	if (data && *data)
		s.add("syncscumm-data", new Common::FSDirectory(data, 4), priority);
	// Last resort, matching the default OSystem behavior (cf. null.cpp).
	s.addDirectory(".", ".", priority - 1);
}

int main(int argc, char *argv[]) {
	sst_io_init(argc, argv);
	atexit(sst_io_shutdown);   /* quit()'s exit(0) still restores the terminal */

	// ScummVM's own argument parser rejects options it doesn't know, so the
	// door-only argv entries sst_io_init() just resolved (-s<fd>, a
	// DOOR32.SYS path) must not reach scummvm_main() -- build a filtered
	// copy with everything sst_io_init() did NOT consume.
	char *filteredArgv[64];
	int   filteredArgc = 0;
	// A door invocation never legitimately has this many args -- fail loudly
	// rather than silently truncate the game path/options off the end of
	// filteredArgv (review finding, M2 Task 4).
	if (argc >= (int)(sizeof(filteredArgv) / sizeof(filteredArgv[0]))) {
		char msg[96];
		snprintf(msg, sizeof(msg), "syncscumm: too many arguments (%d >= %d)\n",
		         argc, (int)(sizeof(filteredArgv) / sizeof(filteredArgv[0])));
		fputs(msg, stderr);
		exit(1);
	}
	for (int i = 0; i < argc && filteredArgc < (int)(sizeof(filteredArgv) / sizeof(filteredArgv[0])); i++) {
		if (i == 0 || !sst_io_consumed(i))
			filteredArgv[filteredArgc++] = argv[i];
	}

	g_system = new OSystem_Synchronet();
	assert(g_system);
	int res = scummvm_main(filteredArgc, filteredArgv);
	g_system->destroy();
	return res;
}

#endif /* USE_SYNCHRONET_DRIVER */
