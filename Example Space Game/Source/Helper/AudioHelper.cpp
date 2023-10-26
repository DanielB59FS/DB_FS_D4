#include "AudioHelper.h"
#include "../Entities/Prefabs.h"



void PlaySound(const char* prefabSound) {
	flecs::entity sound;
	TeamYellow::RetreivePrefab(prefabSound, sound);
	GW::AUDIO::GSound soundFX = *sound.get<GW::AUDIO::GSound>();
	soundFX.Play();
}

void PlayMusic(const char* music, GW::AUDIO::GAudio audioEngine,
	float volume, bool play){
	GW::AUDIO::GMusic currentTrack;
	currentTrack.Create(music, audioEngine, volume);
	currentTrack.Play(play);

}
