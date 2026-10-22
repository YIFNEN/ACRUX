#pragma once

// Minimal Win32 waveOut backend. Call Update once per game frame.
bool AcruxAudio_Init();
void AcruxAudio_Update();
void AcruxAudio_SetPaused(bool paused);
void AcruxAudio_Shutdown();

