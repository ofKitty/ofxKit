#include "ofApp.h"
#include <map>

using namespace ofkitty;

static const std::string kPatchFile = "vcvrack_patch.json";

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::setup()
{
    ofSetFrameRate(60);
    ofBackground(18, 18, 24);

    // ── ofxKit runtime ────────────────────────────────────────────────────────
    runtime().setDataSubdir("vcvrack-udp");
    runtime().enableAllBuiltInWindows();
    runtime().setPassthruCentralNode(false);   // rack panels dock into the centre

    // ── Rack (GUI mode) ───────────────────────────────────────────────────────
    // Audio prefs come from the Runtime so sample rate / buffer size survive
    // an app restart without touching code.
    rack_.setup(runtime().audioSampleRate(),
                runtime().audioBufferSize(), 3);

    // Let the Runtime restart the audio stream when the user changes the
    // Audio device in Preferences → Audio.
    runtime().setAudioRestartCallback([this] {
        rack_.exit();
        rack_.setup(runtime().audioSampleRate(),
                    runtime().audioBufferSize(), 3);
        buildDemoPatch();
        if (busRunning_) { stopBus(); startBus(); }
    });

    buildDemoPatch();
    registerRuntimeIntegration();
    startBus();
}

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::buildDemoPatch()
{
    modVCO_  = rack_.addModule("vco");
    modADSR_ = rack_.addModule("adsr");
    modVCA_  = rack_.addModule("vca");
    modFX_   = rack_.addModule("sendfx");
    modOut_  = rack_.addModule("output");

    rack_.connect(modVCO_,  1, modVCA_,  0);
    rack_.connect(modADSR_, 1, modVCA_,  1);
    rack_.connect(modVCA_,  2, modFX_,   0);
    rack_.connect(modVCA_,  2, modFX_,   1);
    rack_.connect(modFX_,   2, modOut_,  0);
    rack_.connect(modFX_,   3, modOut_,  1);
}

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::startBus()
{
    if (busRunning_) return;
    bool ok = bus_.startServer(udpPrefs_.serverPort);
    ok     &= bus_.startBroadcast(udpPrefs_.broadcastHost,
                                   udpPrefs_.broadcastPort,
                                   udpPrefs_.broadcastHz);
    if (ok) {
        bus_.sendSnapshot();
        busRunning_ = true;
        ofLogNotice("example-udp") << "UDP bus started — server :"
            << udpPrefs_.serverPort << "  broadcast "
            << udpPrefs_.broadcastHost << ":" << udpPrefs_.broadcastPort;
    }
}

void ofApp::stopBus()
{
    bus_.stop();
    busRunning_ = false;
}

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::registerRuntimeIntegration()
{
    // ── Rack menu group ───────────────────────────────────────────────────────
    runtime().addMenuBarGroup("Rack", [this] { drawRackMenuGroup(); });

    // ── Module windows — drawn every frame inside the Runtime ImGui context ──
    // editModeOnly = false keeps them visible outside edit mode.
    runtime().registerWindow({
        "Modules###rack.modules", "Rack", true, false,
        [this](bool& /*vis*/) {
            rack_.draw();   // one ImGui window per module + ImPatch cables
        }
    });

    // ── Preferences page ─────────────────────────────────────────────────────
    runtime().registerPreferencePage({
        "VCVRack", "UDP Bus",
        "vcvrack.prefs.udp",
        [this] { drawUdpPreferencePage(); }
    });

    // ── Status bar ────────────────────────────────────────────────────────────
    runtime().registerStatusItem({
        "vcvrack.status.udp", "VCVRack",
        true,
        [this] { drawUdpStatusItem(); }
    });

    // ── Persist UDP settings in appPrefs.json ─────────────────────────────────
    runtime().registerPrefSerializer(
        "vcvrack.udp",
        [this](ofJson& j) {
            j["vcvrack.udp"] = {
                { "serverPort",    udpPrefs_.serverPort    },
                { "broadcastHost", udpPrefs_.broadcastHost },
                { "broadcastPort", udpPrefs_.broadcastPort },
                { "broadcastHz",   udpPrefs_.broadcastHz   },
            };
        },
        [this](const ofJson& j) {
            if (!j.contains("vcvrack.udp")) return;
            const auto& u = j["vcvrack.udp"];
            if (u.contains("serverPort"))    udpPrefs_.serverPort    = u["serverPort"];
            if (u.contains("broadcastHost")) udpPrefs_.broadcastHost = u["broadcastHost"].get<std::string>();
            if (u.contains("broadcastPort")) udpPrefs_.broadcastPort = u["broadcastPort"];
            if (u.contains("broadcastHz"))   udpPrefs_.broadcastHz   = u["broadcastHz"];
        });
}

// ── Rack menu group ───────────────────────────────────────────────────────────
void ofApp::drawRackMenuGroup()
{
    if (ImGui::MenuItem("Save Patch"))
        rack_.saveSettings(ofToDataPath(kPatchFile));

    if (ImGui::MenuItem("Load Patch")) {
        rack_.loadSettings(ofToDataPath(kPatchFile));
        // Refresh the module entities we track
        auto view = rack_.registry().view<rack::module_component>();
        for (auto e : view) {
            const auto& mc = rack_.registry().get<rack::module_component>(e);
            if      (mc.typeId == "vco")    modVCO_  = e;
            else if (mc.typeId == "adsr")   modADSR_ = e;
            else if (mc.typeId == "vca")    modVCA_  = e;
            else if (mc.typeId == "sendfx") modFX_   = e;
            else if (mc.typeId == "output") modOut_  = e;
        }
        if (busRunning_) bus_.sendSnapshot();
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Add Module")) {
        for (const auto& def : rack::RackModuleFactory::Types())
            if (ImGui::MenuItem(def.displayName.c_str()))
                rack_.addModule(def.typeId);
        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (busRunning_) {
        if (ImGui::MenuItem("Restart UDP Bus")) { stopBus(); startBus(); }
        if (ImGui::MenuItem("Send Snapshot"))   bus_.sendSnapshot();
    } else {
        if (ImGui::MenuItem("Start UDP Bus"))   startBus();
    }
}

// ── UDP preference page ───────────────────────────────────────────────────────
void ofApp::drawUdpPreferencePage()
{
    ImGui::TextDisabled("Changes take effect on Restart UDP Bus.");
    ImGui::Spacing();

    ImGui::SetNextItemWidth(120.f);
    ImGui::InputInt("Server port (receive)",  &udpPrefs_.serverPort);

    char hostBuf[64];
    std::strncpy(hostBuf, udpPrefs_.broadcastHost.c_str(), sizeof(hostBuf) - 1);
    hostBuf[sizeof(hostBuf)-1] = '\0';
    ImGui::SetNextItemWidth(160.f);
    if (ImGui::InputText("Broadcast host", hostBuf, sizeof(hostBuf)))
        udpPrefs_.broadcastHost = hostBuf;

    ImGui::SetNextItemWidth(120.f);
    ImGui::InputInt("Broadcast port (send)", &udpPrefs_.broadcastPort);

    ImGui::SetNextItemWidth(120.f);
    ImGui::SliderFloat("Broadcast Hz", &udpPrefs_.broadcastHz, 1.f, 120.f, "%.0f Hz");

    ImGui::Spacing();
    if (ImGui::Button("Restart UDP Bus")) { stopBus(); startBus(); }
    ImGui::SameLine();
    if (ImGui::Button("Send Snapshot") && busRunning_) bus_.sendSnapshot();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Message format (JSON datagrams):");
    ImGui::TextDisabled(R"(  Incoming → {"t":"set_p","m":"vco_0","i":0,"v":72.0})");
    ImGui::TextDisabled(R"(  Incoming → {"t":"gate","m":"adsr_1","v":1})");
    ImGui::TextDisabled(R"(  Incoming → {"t":"conn","from":"vco_0","fp":1,"to":"vca_2","tp":0})");
    ImGui::TextDisabled(R"(  Outgoing ← {"t":"p","m":"vco_0","i":0,"v":69.0})");
    ImGui::TextDisabled(R"(  Outgoing ← {"t":"l","m":"adsr_1","i":0,"v":0.95})");
}

// ── Status bar item ───────────────────────────────────────────────────────────
void ofApp::drawUdpStatusItem()
{
    if (busRunning_) {
        ImGui::TextDisabled("UDP :%d → %s:%d @ %.0fHz",
            udpPrefs_.serverPort,
            udpPrefs_.broadcastHost.c_str(),
            udpPrefs_.broadcastPort,
            udpPrefs_.broadcastHz);
    } else {
        ImGui::TextDisabled("UDP off");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::update()
{
    bus_.update();
}

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::draw()
{
    // Raw OF drawing goes here.
    // The Runtime draws the ImGui overlay (rack windows, menus, inspector)
    // via OF_EVENT_ORDER_AFTER_APP — nothing extra needed here.
    if (!runtime().isEditMode()) {
        ofSetColor(255, 255, 255, 40);
        ofDrawBitmapString("CTRL+E / TAB  edit mode  |  Space=gate  z/x/c=keys", 14, ofGetHeight() - 14);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::exit()
{
    rack_.saveSettings(ofToDataPath(kPatchFile));
    stopBus();
    rack_.exit();
}

// ─────────────────────────────────────────────────────────────────────────────
// Keyboard gate / pitch control
// ─────────────────────────────────────────────────────────────────────────────
void ofApp::keyPressed(int key)
{
    if (runtime().isEditMode()) return;   // ImGui is capturing keys

    if (key == ' ') {
        if (auto* dsp = rack_.registry().try_get<rack::adsr_dsp_component>(modADSR_)) {
            static bool on = false;
            on = !on;
            dsp->dsp->gateIn.set(on ? 1.f : 0.f);
        }
        return;
    }

    static const std::map<int,int> noteMap {
        {'z',60},{'s',61},{'x',62},{'d',63},{'c',64},
        {'v',65},{'g',66},{'b',67},{'h',68},{'n',69},
        {'j',70},{'m',71},{',',72}
    };
    if (auto it = noteMap.find(key); it != noteMap.end()) {
        if (auto* vco  = rack_.registry().try_get<rack::vco_dsp_component> (modVCO_))
            vco->dsp->cvPitchIn.set(static_cast<float>(it->second));
        if (auto* adsr = rack_.registry().try_get<rack::adsr_dsp_component>(modADSR_))
            adsr->dsp->gateIn.set(1.f);
    }
}

void ofApp::keyReleased(int key)
{
    if (runtime().isEditMode()) return;
    static const std::map<int,int> noteMap {
        {'z',60},{'s',61},{'x',62},{'d',63},{'c',64},
        {'v',65},{'g',66},{'b',67},{'h',68},{'n',69},
        {'j',70},{'m',71},{',',72}
    };
    if (noteMap.count(key))
        if (auto* adsr = rack_.registry().try_get<rack::adsr_dsp_component>(modADSR_))
            adsr->dsp->gateIn.set(0.f);
}
