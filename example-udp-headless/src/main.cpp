// ============================================================================
// example-udp-headless — ofxVCVRack DSP server with UDP bus
// ============================================================================
// A windowless audio + network server.
//
//   • Loads "vcvrack_patch.json" from the working directory if it exists,
//     otherwise builds a default VCO → ADSR → VCA → FX → Output patch.
//   • Starts a UDP command server on :9001.
//   • Broadcasts module state (params + LEDs) to 255.255.255.255:9002 @ 30 Hz.
//   • Saves the patch to "vcvrack_patch.json" on exit (Ctrl+C / SIGINT).
//
// Timing note:
//   pdsp::Engine runs its own audio-thread at the configured sample rate —
//   that is the authoritative DSP tick.  bus_.update() here only drains the
//   UDP queue and sends state deltas; 10 ms resolution is plenty.
//   For beat-synchronised gate triggers, add ofxAbletonLink and send
//   {"t":"gate","m":"adsr_1","v":1} / {"v":0} datagrams from a Link client.
//
// Send JSON datagrams to localhost:9001 to control the engine:
//
//   Set param:   {"t":"set_p",  "m":"vco_0", "i":0,   "v":72.0}
//   Gate on:     {"t":"gate",   "m":"adsr_1","v":1}
//   Gate off:    {"t":"gate",   "m":"adsr_1","v":0}
//   Connect:     {"t":"conn",   "from":"vco_0","fp":1,"to":"vca_2","tp":0}
//   Full snap:   {"t":"get_snap"}
//   Load patch:  {"t":"load",   "modules":[…],"cables":[…]}
//   Note on/off: {"t":"note_on","note":69,"vel":1.0}  /  {"t":"note_off"}
//
// Receive state stream on :9002 (one datagram per changed value per tick):
//
//   Param:       {"t":"p","m":"vco_0","i":0,"v":69.0}
//   LED:         {"t":"l","m":"adsr_1","i":0,"v":0.95}
//   Snapshot:    {"t":"snap","modules":[…],"cables":[…]}
// ============================================================================

#include "ofMain.h"
#include "ofxVCVRack.h"
#include <csignal>
#include <atomic>

static std::atomic<bool> s_quit { false };

extern "C" void handleSigint(int) { s_quit = true; }

// ─────────────────────────────────────────────────────────────────────────────
static void buildDefaultPatch(rack::RackEngine& rack,
                               entt::entity& vco, entt::entity& adsr,
                               entt::entity& vca, entt::entity& fx,
                               entt::entity& out)
{
    vco  = rack.addModule("vco");
    adsr = rack.addModule("adsr");
    vca  = rack.addModule("vca");
    fx   = rack.addModule("sendfx");
    out  = rack.addModule("output");

    rack.connect(vco,  1, vca,  0);
    rack.connect(adsr, 1, vca,  1);
    rack.connect(vca,  2, fx,   0);
    rack.connect(vca,  2, fx,   1);
    rack.connect(fx,   2, out,  0);
    rack.connect(fx,   3, out,  1);

    if (auto* p = rack::GetParam(vco,  rack.registry(), 0)) p->value = 69.f;   // A4
    if (auto* p = rack::GetParam(adsr, rack.registry(), 0)) p->value = 0.01f;  // A
    if (auto* p = rack::GetParam(adsr, rack.registry(), 1)) p->value = 0.3f;   // D
    if (auto* p = rack::GetParam(adsr, rack.registry(), 2)) p->value = 0.7f;   // S
    if (auto* p = rack::GetParam(adsr, rack.registry(), 3)) p->value = 0.5f;   // R
    if (auto* p = rack::GetParam(vca,  rack.registry(), 0)) p->value = 0.8f;   // Level
    if (auto* p = rack::GetParam(fx,   rack.registry(), 0)) p->value = 0.4f;   // Wet
}

// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    std::signal(SIGINT, handleSigint);

    const std::string patchFile     = "vcvrack_patch.json";
    const int         serverPort    = (argc > 1) ? std::atoi(argv[1]) : 9001;
    const int         broadcastPort = (argc > 2) ? std::atoi(argv[2]) : 9002;
    const std::string broadcastHost = (argc > 3) ? argv[3] : "255.255.255.255";
    const float       broadcastHz   = 30.f;

    // ── Engine ────────────────────────────────────────────────────────────────
    rack::RackEngine rack;
    rack.setupHeadless(44100, 256, 3);

    entt::entity modVCO = entt::null, modADSR = entt::null,
                 modVCA = entt::null, modFX   = entt::null,
                 modOut = entt::null;

    if (ofFile(patchFile).exists()) {
        ofLogNotice("headless") << "Loading patch from " << patchFile;
        rack.loadSettings(patchFile);
        // Resolve tracked entities after load
        rack.registry().view<rack::module_component>().each(
            [&](auto e, const rack::module_component& mc) {
                if      (mc.typeId == "vco")    modVCO  = e;
                else if (mc.typeId == "adsr")   modADSR = e;
                else if (mc.typeId == "vca")    modVCA  = e;
                else if (mc.typeId == "sendfx") modFX   = e;
                else if (mc.typeId == "output") modOut  = e;
            });
    } else {
        ofLogNotice("headless") << "No patch found — building default patch";
        buildDefaultPatch(rack, modVCO, modADSR, modVCA, modFX, modOut);
    }

    // ── UDP bus ───────────────────────────────────────────────────────────────
    rack::RackUdpBus bus(rack);
    bus.startServer(serverPort);
    bus.startBroadcast(broadcastHost, broadcastPort, broadcastHz);
    bus.sendSnapshot();

    ofLogNotice("headless") << "Running — Ctrl+C to stop";
    ofLogNotice("headless") << "  Listening for commands on :" << serverPort;
    ofLogNotice("headless") << "  Broadcasting state  → " << broadcastHost << ":" << broadcastPort;
    ofLogNotice("headless") << "  Patch file          → " << patchFile;

    // ── Custom message handler: trigger the gate from UDP ────────────────────
    // (The built-in "gate" message writes to PatchNode::set(), which works for
    // explicitly-set constant inputs. Here we also support adsr_dsp_component.)
    bus.onUnknownMessage = [&](const ofJson& msg) {
        std::string t = msg.value("t", "");
        if (t == "note_on") {
            int   note = msg.value("note", 69);
            float vel  = msg.value("vel",  1.f);
            if (auto* vco = rack.registry().try_get<rack::vco_dsp_component>(modVCO))
                vco->dsp->cvPitchIn.set(static_cast<float>(note));
            if (auto* adsr = rack.registry().try_get<rack::adsr_dsp_component>(modADSR))
                adsr->dsp->gateIn.set(vel > 0.f ? 1.f : 0.f);
        } else if (t == "note_off") {
            if (auto* adsr = rack.registry().try_get<rack::adsr_dsp_component>(modADSR))
                adsr->dsp->gateIn.set(0.f);
        }
    };

    // ── Main loop ─────────────────────────────────────────────────────────────
    constexpr int kTickMs = 10;
    while (!s_quit) {
        bus.update();
        ofSleepMillis(kTickMs);
    }

    // ── Graceful shutdown ─────────────────────────────────────────────────────
    ofLogNotice("headless") << "Shutting down...";
    rack.saveSettings(patchFile);
    ofLogNotice("headless") << "Patch saved → " << patchFile;

    bus.stop();
    rack.exit();
    return 0;
}
