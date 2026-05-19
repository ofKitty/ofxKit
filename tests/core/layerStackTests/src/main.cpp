#include "ofxUnitTests.h"
#include "LayerStack.h"
#include "ofMain.h"

using ofkitty::LayerBase;
using ofkitty::LayerStack;

// A minimal concrete layer type used exclusively by these tests.
struct TestLayer : public LayerBase {
    int value = 0;
};

class ofApp : public ofxUnitTestsApp {
    void run() override {

        // ------------------------------------------------------------------
        // Construction invariant — always starts with one layer
        // ------------------------------------------------------------------
        ofLogNotice() << "--- construction ---";

        LayerStack<TestLayer> stack;
        ofxTest(stack.size() == 1,        "default stack has exactly one layer");
        ofxTest(stack.currentLayer == 0,  "currentLayer starts at 0");
        ofxTest(!stack.layers.empty(),    "layers vector is non-empty");
        ofxTest(stack.layers[0].name == "Layer 1", "first layer name is 'Layer 1'");

        // ------------------------------------------------------------------
        // addLayer
        // ------------------------------------------------------------------
        ofLogNotice() << "--- addLayer ---";

        int idx = stack.addLayer("Background");
        ofxTest(stack.size() == 2,          "size grows after addLayer");
        ofxTest(idx == 1,                   "addLayer returns the new index");
        ofxTest(stack.layers[1].name == "Background", "new layer has the given name");

        stack.addLayer(); // auto-name
        ofxTest(stack.size() == 3,          "anonymous addLayer grows size");
        ofxTest(!stack.layers[2].name.empty(), "auto-named layer has non-empty name");

        // ------------------------------------------------------------------
        // getActive / currentLayer
        // ------------------------------------------------------------------
        ofLogNotice() << "--- getActive ---";

        stack.currentLayer = 1;
        ofxTest(stack.getActive().name == "Background", "getActive() returns layer at currentLayer");

        // Out-of-range currentLayer is clamped
        stack.currentLayer = 99;
        stack.getActive(); // should not crash
        ofxTest(stack.currentLayer <= stack.size() - 1, "getActive clamps out-of-range currentLayer");

        // ------------------------------------------------------------------
        // removeLayer — minimum 1 layer enforced
        // ------------------------------------------------------------------
        ofLogNotice() << "--- removeLayer ---";

        // Remove down to 1 layer
        while (stack.size() > 1)
            stack.removeLayer(0);

        ofxTest(stack.size() == 1, "remove stops at 1 layer");

        stack.removeLayer(0); // should be a no-op
        ofxTest(stack.size() == 1, "removing the last layer is a no-op");

        // ------------------------------------------------------------------
        // moveUp / moveDown
        // ------------------------------------------------------------------
        ofLogNotice() << "--- moveUp / moveDown ---";

        LayerStack<TestLayer> s2;
        s2.addLayer("B");
        s2.addLayer("C"); // layers: [Layer 1, B, C]

        ofxTest(s2.layers[0].name == "Layer 1", "initial order: [0]=Layer1");
        ofxTest(s2.layers[1].name == "B",        "initial order: [1]=B");
        ofxTest(s2.layers[2].name == "C",        "initial order: [2]=C");

        s2.moveDown(0);   // Layer1 moves to [1]: [B, Layer1, C]
        ofxTest(s2.layers[0].name == "B",        "moveDown(0): B is now at 0");
        ofxTest(s2.layers[1].name == "Layer 1",  "moveDown(0): Layer1 moved to 1");

        s2.moveUp(2);     // C moves to [1]: [B, C, Layer1]
        ofxTest(s2.layers[1].name == "C",        "moveUp(2): C moved to 1");
        ofxTest(s2.layers[2].name == "Layer 1",  "moveUp(2): Layer1 moved to 2");

        // currentLayer tracking during moves
        LayerStack<TestLayer> s3;
        s3.addLayer("X"); // [Layer1, X]; cur=0
        s3.currentLayer = 0;
        s3.moveDown(0);   // Layer1 moves right, cur should follow to 1
        ofxTest(s3.currentLayer == 1, "currentLayer follows the active layer on moveDown");

        s3.moveUp(1);     // Layer1 moves left back to 0
        ofxTest(s3.currentLayer == 0, "currentLayer follows the active layer on moveUp");

        // ------------------------------------------------------------------
        // LayerBase defaults
        // ------------------------------------------------------------------
        ofLogNotice() << "--- LayerBase defaults ---";

        LayerStack<TestLayer> s4;
        const TestLayer& first = s4.layers[0];
        ofxTest(first.visible == true,    "new layer is visible by default");
        ofxTest(first.locked  == false,   "new layer is not locked by default");
        ofxTest(first.parentIndex == -1,  "new layer has no parent by default");
    }
};

#include "app/ofAppNoWindow.h"
#include "app/ofAppRunner.h"

int main() {
    ofInit();
    auto window = std::make_shared<ofAppNoWindow>();
    auto app    = std::make_shared<ofApp>();
    ofRunApp(window, app);
    return ofRunMainLoop();
}
