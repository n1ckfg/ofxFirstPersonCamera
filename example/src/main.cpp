#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){

	// ofxFirstPersonCamera drives the GLFW cursor directly, so it needs a
	// GLFW window - which ofGLFWWindowSettings guarantees.
	ofGLFWWindowSettings settings;
	settings.setSize(1024, 768);
	settings.windowMode = OF_WINDOW; //can also be OF_FULLSCREEN

	auto window = ofCreateWindow(settings);

	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();

}
