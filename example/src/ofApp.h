#pragma once

#include "ofMain.h"

#include "ofxFirstPersonCamera.h"

class ofApp : public ofBaseApp {

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void mousePressed(int x, int y, int button);

		void resetCamera();

		ofxFirstPersonCamera camera;

		std::vector<ofCylinderPrimitive> cylinders;
};
