/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#include "system.h"

#ifdef SUPPORT_AUDIO

#include <cassert>
#include <cstdint>
#include <chrono>
#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_version.h>

#include "sdl_audio.h"
#include "output.h"

using namespace std::chrono_literals;

void sdl_audio_callback(void* userdata, uint8_t* stream, int length) {
	// no mutex locking required, SDL does this before calling

	static_cast<GenericAudio*>(userdata)->Decode(stream, length);
}

AudioDecoder::Format sdl_format_to_format(Uint16 format) {
	switch (format) {
		case AUDIO_U8:
			return AudioDecoder::Format::U8;
		case AUDIO_S8:
			return AudioDecoder::Format::S8;
		case AUDIO_U16SYS:
			return AudioDecoder::Format::U16;
		case AUDIO_S16SYS:
			return AudioDecoder::Format::S16;
		default:
			Output::Warning("Couldn't find GenericAudio format for {:#x}", format);
			assert(false);
	}

	return (AudioDecoder::Format)-1;
}

SdlAudio::SdlAudio(const Game_ConfigAudio& cfg) :
	GenericAudio(cfg)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		Output::Warning("Couldn't init audio: {}", SDL_GetError());
		return;
	}

	const int frequency = 44100;

	SDL_AudioSpec want = {};
	SDL_AudioSpec have = {};
	want.freq = frequency;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 2048;
	want.callback = sdl_audio_callback;
	want.userdata = this;
	bool init_success = SDL_OpenAudio(&want, &have) >= 0;

	if (!init_success) {
		Output::Warning("Couldn't open audio: {}", SDL_GetError());
		return;
	}

	SetFormat(have.freq, sdl_format_to_format(have.format), have.channels);

	// Start Audio
	SDL_PauseAudio(0);
}

SdlAudio::~SdlAudio() {
	SDL_CloseAudio();
}

void SdlAudio::LockMutex() const {
	SDL_LockAudio();
}

void SdlAudio::UnlockMutex() const {
	SDL_UnlockAudio();
}

#endif
