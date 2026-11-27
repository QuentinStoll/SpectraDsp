#pragma once

#include <JuceHeader.h>

// Audio device policy for the standalone application.
namespace StandaloneAudio {

// Windows only. WASAPI exclusive mode takes the whole output endpoint, so while
// Spectra runs no other application (YouTube in a browser, for instance) can
// play through that device. Spectra no longer starts in exclusive mode: when
// the saved audio setup uses it, the setup moves once to Windows shared mode,
// on the same devices at 48 kHz. Choosing exclusive mode again afterwards in
// Options > Audio/MIDI Settings is respected.
//
// Call from the message thread, once the standalone window exists. It does
// nothing in a plugin host, on other platforms, or when the setup is already
// shared.
void moveOffExclusiveMode();

}  // namespace StandaloneAudio
