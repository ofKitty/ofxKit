#pragma once

#include "ofMain.h"
#include "ofxKit.h"
#include "ofxVCVRack.h"

// ─────────────────────────────────────────────────────────────────────────────
// ofApp — ofxVCVRack + UDP bus inside an ofxKit runtime
//
// The Runtime overlay (CTRL+E / TAB) provides:
//   - Properties panel inspecting rack ECS components
//   - Preferences → VCVRack UDP   (port / host / hz settings)
//   - Status bar item             (server port + broadcast address)
//   - Rack menu group             (save / load / add module)
//
// Keyboard (when Edit mode is off):
//   Space       — gate toggle
//   z/x/c/…    — MIDI-style keyboard (C4 → C5)
// ─────────────────────────────────────────────────────────────────────────────

class ofApp : public ofBaseApp {
public:
    void setup()   override;
    void update()  override;
    void draw()    override;
    void exit()    override;
    void keyPressed (int key) override;
    void keyReleased(int key) override;

    // Expose the rack's registry so main.cpp can pass it to Runtime::attach
    entt::registry& rackRegistry() { return rack_.registry(); }

private:
    // ── Core ──────────────────────────────────────────────────────────────────
    rack::RackEngine  rack_;
    rack::RackUdpBus  bus_  { rack_ };

    // ── Demo patch entities ───────────────────────────────────────────────────
    entt::entity modVCO_  { entt::null };
    entt::entity modADSR_ { entt::null };
    entt::entity modVCA_  { entt::null };
    entt::entity modFX_   { entt::null };
    entt::entity modOut_  { entt::null };

    // ── UDP settings (persisted via Runtime pref serializer) ──────────────────
    struct UdpPrefs {
        int         serverPort    { 9001 };
        std::string broadcastHost { "255.255.255.255" };
        int         broadcastPort { 9002 };
        float       broadcastHz   { 30.f };
    } udpPrefs_;

    bool busRunning_ { false };

    // ── Helpers ───────────────────────────────────────────────────────────────
    void buildDemoPatch();
    void startBus();
    void stopBus();

    void registerRuntimeIntegration();   // menus, prefs, status bar, windows
    void drawRackMenuGroup();
    void drawUdpPreferencePage();
    void drawUdpStatusItem();
};
