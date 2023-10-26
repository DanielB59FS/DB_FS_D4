#ifndef AUDIOHELPER_H
#define AUDIOHELPER_H



void PlaySound(const char* sound);
void PlayMusic(const char* music, GW::AUDIO::GAudio audioEngine,
	float volume, bool play);

#endif