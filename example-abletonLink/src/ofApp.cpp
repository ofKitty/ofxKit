#include "ofApp.h"
#include <cmath>

using namespace ofkitty;

static const std::string kPatchFile = "vcvrack_link_patch.json";

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::setup()
{
    ofSetFrameRate(60);
    ofBackground(18, 18, 24);

    runtime().setDataSubdir("vcvrack-link");
    runtime().enableAllBuiltInWindows();
    runtime().setPassthruCentralNode(false);

    // ── Rack ──────────────────────────────────────────────────────────────────
    rack_.setup(runtime().audioSampleRate(), runtime().audioBufferSize(), 3);
    runtime().setAudioRestartCallback([this] {
        rack_.exit();
        rack_.setup(runtime().audioSampleRate(), runtime().audioBufferSize(), 3);
        buildPatch();
    });

    buildPatch();

    // ── Ableton Link ──────────────────────────────────────────────────────────
    linkKit_.setup(120.0);
    linkKit_.registerWithRuntime();   // adds "Link Sync" panel to View menu

    // ── UDP bus ───────────────────────────────────────────────────────────────
    bus_.startServer(9001);
    bus_.startBroadcast("255.255.255.255", 9002, 30.f);
    bus_.sendSnapshot();

    // Extra UDP commands: bpm, play, stop, arp, gate_len
    bus_.onUnknownMessage = [this](const ofJson& msg) {
        std::string t = msg.value("t", "");

        if (t == "bpm") {
            linkKit_.link().setTempo(msg.value("v", 120.0));

        } else if (t == "play") {
            linkKit_.link().setIsPlaying(true);

        } else if (t == "stop") {
            linkKit_.link().setIsPlaying(false);
            gateOff();

        } else if (t == "gate_len") {
            gateLenBeats_ = std::clamp(msg.value("v", 0.5), 0.05, 0.95);

        } else if (t == "arp") {
            if (msg.contains("notes") && msg["notes"].is_array()) {
                arpNotes_.clear();
                for (auto& n : msg["notes"]) arpNotes_.push_back(n.get<int>());
                arpStep_ = 0;
            }

        } else if (t == "note_on") {
            gateOn(msg.value("note", 69));
        } else if (t == "note_off") {
            gateOff();
        }
    };

    // ── Rack module panel (always visible, not edit-mode-only) ───────────────
    runtime().registerWindow({
        "Modules###rack.modules", "Rack", true, false,
        [this](bool& /*vis*/) { rack_.draw(); }
    });

    // ── Rack menu group ───────────────────────────────────────────────────────
    runtime().addMenuBarGroup("Rack", [this] {
        if (ImGui::MenuItem("Save Patch")) rack_.saveSettings(ofToDataPath(kPatchFile));
        if (ImGui::MenuItem("Load Patch")) { rack_.loadSettings(ofToDataPath(kPatchFile)); bus_.sendSnapshot(); }
        ImGui::Separator();
        if (ImGui::BeginMenu("Add Module")) {
            for (const auto& def : rack::RackModuleFactory::Types())
                if (ImGui::MenuItem(def.displayName.c_str()))
                    rack_.addModule(def.typeId);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::SetNextItemWidth(80.f);
        ImGui::SliderFloat("Gate len", reinterpret_cast<float*>(&gateLenBeats_), 0.05f, 0.95f, "%.2f beats");
    });

    // ── Status bar ────────────────────────────────────────────────────────────
    runtime().registerStatusItem({
        "vcvrack.link.status", "VCVRack",
        true,
        [this] {
            auto status = linkKit_.link().update();
            if (linkKit_.isEnabled()) {
                ImGui::Text("Link  %.1f BPM  beat %.2f  %zu peer%s",
                    linkKit_.lastBpm(),
                    status.beat,
                    linkKit_.peers(),
                    linkKit_.peers() == 1 ? "" : "s");
            } else {
                ImGui::TextDisabled("Link off");
            }
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::buildPatch()
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
void ofApp::gateOn(int midiNote)
{
    if (auto* vco = rack_.registry().try_get<rack::vco_dsp_component>(modVCO_))
        vco->dsp->cvPitchIn.set(static_cast<float>(midiNote));
    if (auto* adsr = rack_.registry().try_get<rack::adsr_dsp_component>(modADSR_))
        adsr->dsp->gateIn.set(1.f);
}

void ofApp::gateOff()
{
    if (auto* adsr = rack_.registry().try_get<rack::adsr_dsp_component>(modADSR_))
        adsr->dsp->gateIn.set(0.f);
    gateOnAtBeat_ = -999.0;
}

// ── Link tick — called every frame ───────────────────────────────────────────
void ofApp::tickLink()
{
    if (!linkKit_.isEnabled()) return;

    auto   status  = linkKit_.link().update();
    double beat    = status.beat;

    if (!status.isPlaying) return;

    // ── Beat boundary: fire gate + advance arp ────────────────────────────────
    double curFloor = std::floor(beat);
    if (curFloor > prevBeatFloor_) {
        prevBeatFloor_ = curFloor;

        if (!arpNotes_.empty()) {
            int note = arpNotes_[arpStep_ % static_cast<int>(arpNotes_.size())];
            ++arpStep_;
            gateOnAtBeat_ = beat;
            gateOn(note);
        }
    }

    // ── Gate off after gateLenBeats ───────────────────────────────────────────
    if (gateOnAtBeat_ > -999.0 && (beat - gateOnAtBeat_) >= gateLenBeats_)
        gateOff();
}

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::update()
{
    linkKit_.update(ofGetLastFrameTime());
    tickLink();
    bus_.update();
}

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::draw()
{
    if (!runtime().isEditMode()) {
        ofSetColor(255, 255, 255, 40);
        ofDrawBitmapString("CTRL+E / TAB  edit mode  |  Link panel: View > Link Sync", 14, ofGetHeight() - 14);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void ofApp::exit()
{
    rack_.saveSettings(ofToDataPath(kPatchFile));
    bus_.stop();
    rack_.exit();
}
