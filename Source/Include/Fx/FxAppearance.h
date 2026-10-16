#pragma once

#include "Components/CustomLookAndFeel.h"
#include "Fx/FxCatalog.h"
#include "Stylesheet.h"

namespace Fx {
struct Appearance {
    juce::Colour colour;
    CustomLookAndFeel::RigIcon icon;
    const char* caption;
};

inline Appearance appearanceFor(SignalChain::Stage stage) noexcept {
    switch (stage) {
        case SignalChain::Stage::Cab:
            return {ProfilerStyle::Colors::rigCab, CustomLookAndFeel::RigIcon::Cabinet, "CAB"};
        case SignalChain::Stage::Eq:
        case SignalChain::Stage::EqParametric:
        case SignalChain::Stage::EqTone:
        case SignalChain::Stage::EqDynamic:
            return {ProfilerStyle::Colors::rigEq, CustomLookAndFeel::RigIcon::EqFaders, captionFor(stage)};
        case SignalChain::Stage::Pedal:
            return {ProfilerStyle::Colors::rigPedal, CustomLookAndFeel::RigIcon::Pedal, "DRIVE"};
        case SignalChain::Stage::PitchHarmonizer:
        case SignalChain::Stage::PitchDrop:
        case SignalChain::Stage::PitchOctaver:
            return {ProfilerStyle::Colors::rigPitch, CustomLookAndFeel::RigIcon::Pitch, captionFor(stage)};
        case SignalChain::Stage::ReverbPlate:
        case SignalChain::Stage::ReverbHall:
        case SignalChain::Stage::ReverbShimmer:
        case SignalChain::Stage::ReverbSpring:
        case SignalChain::Stage::ReverbGranular:
            return {ProfilerStyle::Colors::rigReverb, CustomLookAndFeel::RigIcon::Reverb, captionFor(stage)};
        case SignalChain::Stage::DelayTape:
        case SignalChain::Stage::DelayPingPong:
        case SignalChain::Stage::DelayDark:
        case SignalChain::Stage::DelayTapeExtreme:
        case SignalChain::Stage::DelayReverse:
            return {ProfilerStyle::Colors::rigDelay, CustomLookAndFeel::RigIcon::Delay, captionFor(stage)};
        case SignalChain::Stage::ChorusEnsemble:
        case SignalChain::Stage::ChorusLead:
            return {ProfilerStyle::Colors::rigChorus, CustomLookAndFeel::RigIcon::Mod, captionFor(stage)};
        case SignalChain::Stage::Phaser4:
        case SignalChain::Stage::Phaser8:
            return {ProfilerStyle::Colors::rigPhaser, CustomLookAndFeel::RigIcon::Mod, captionFor(stage)};
        case SignalChain::Stage::FlangerSubtle:
        case SignalChain::Stage::FlangerHard:
            return {ProfilerStyle::Colors::rigFlanger, CustomLookAndFeel::RigIcon::Mod, captionFor(stage)};
        case SignalChain::Stage::CompBlack:
        case SignalChain::Stage::CompBrutal:
        case SignalChain::Stage::CompClear:
        case SignalChain::Stage::NoiseGate:
            return {ProfilerStyle::Colors::rigDynamics, CustomLookAndFeel::RigIcon::Dynamics, captionFor(stage)};
        case SignalChain::Stage::Tuner:
            return {ProfilerStyle::Colors::rigTuner, CustomLookAndFeel::RigIcon::Tuner, "TUNE"};
        case SignalChain::Stage::Amp:
        case SignalChain::Stage::Empty:
        default:
            return {ProfilerStyle::Colors::rigAmp, CustomLookAndFeel::RigIcon::AmpHead, captionFor(stage)};
    }
}
}  // namespace Fx
