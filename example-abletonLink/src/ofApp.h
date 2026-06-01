#pragma once

#include "ofMain.h"
#include "ofxKit.h"
#include "ofxVCVRack.h"
#include "ofxAbletonLinkKit.h"

// ─────────────────────────────────────────────────────────────────────────────
// ofApp — ofxVCVRack + Ableton Link + UDP bus inside ofxKit
//
// Link drives the beat clock:
//   • Gate on  at every beat boundary (quantised to Link transport)
//   • Gate off after `gateLenBeats` beats  (default 0.5 = eighth note)
//   • Step arpeggiator advances one step per beat
//
// "Link Sync" panel (registered by ofxAbletonLinkKit):
//   • Enable/disable Link, BPM drag, quantum, start/stop sync, peer count
//   • Beat / phase progress bar
//
// UDP bus (port 9001 / 9002) mirrors the example-udp commands.
// ─────────────────────────────────────────────────────────────────────────────

class ofApp : public ofBaseApp {
public:
    void setup()   override;
    void update()  override;
    void draw()    override;
    void exit()    override;

    entt::registry& rackRegistry() { return rack_.registry(); }

private:
    // ── Core ──────────────────────────────────────────────────────────────────
    rack::RackEngine     rack_;
    rack::RackUdpBus     bus_     { rack_ };
    ofxAbletonLinkKit    linkKit_;

    // ── Demo patch ────────────────────────────────────────────────────────────
    entt::entity modVCO_  { entt::null };
    entt::entity modADSR_ { entt::null };
    entt::entity modVCA_  { entt::null };
    entt::entity modFX_   { entt::null };
    entt::entity modOut_  { entt::null };

    // ── Link → gate state ─────────────────────────────────────────────────────
    double prevBeatFloor_ { -1.0 };   // last integer beat we acted on
    double gateOnAtBeat_  { -999.0 }; // beat position when gate fired
    double gateLenBeats_  { 0.5 };    // gate open duration in beats (0.5 = 8th note)

    // ── Step arpeggiator ──────────────────────────────────────────────────────
    std::vector<int> arpNotes_ { 60, 64, 67, 71, 72, 67, 64, 60 }; // C maj7 up/down
    int              arpStep_  { 0 };

    // ── Helpers ───────────────────────────────────────────────────────────────
    void buildPatch();
    void tickLink();          // called from update()
    void gateOn(int midiNote);
    void gateOff();
};
