#include "ofApp.h"

/*****************************************************
// Based on ofxFPSCamera example by Ivaylo Getov, 2014
*****************************************************/

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetVerticalSync(true);
	ofSetFrameRate(60);
	ofBackground(0);
	ofSetWindowTitle("ofxFirstPersonCamera");
	ofSetWindowPosition(ofGetScreenWidth()/2 - ofGetWidth()/2, ofGetScreenHeight()/2 - ofGetHeight()/2);

	camera.movespeed = 4.0f;
	camera.setNearClip(1.0f);
	camera.setFarClip(10000.0f);
	resetCamera();

	cylinders.resize(50);
	for (auto & cylinder : cylinders) {
		cylinder.set(20, 100);
		cylinder.setPosition(ofRandom(-600, 600), 0, ofRandom(-600, 600));
	}

	camera.enableControl();
}

//--------------------------------------------------------------
void ofApp::resetCamera(){
	camera.setPosition(0, 0, 600);
	camera.setOrientation(glm::vec3(0, 0, 0));
	camera.upvector = glm::vec3(0, 1, 0);
}

//--------------------------------------------------------------
void ofApp::update(){
}

//--------------------------------------------------------------
void ofApp::draw(){
	ofEnableDepthTest();

	camera.begin();

		ofSetColor(100);
		ofPushMatrix();
			ofTranslate(0, -50, 0);
			ofRotateDeg(90, 0, 0, 1);
			ofDrawGridPlane(60.0f, 20, false);
		ofPopMatrix();

		ofSetColor(255);
		for (auto & cylinder : cylinders) {
			cylinder.drawWireframe();
		}

	camera.end();

	ofDisableDepthTest();

	ofSetColor(255);
	ofDrawBitmapString("mouse: look around"
					   "\nw/s: forward/backwards"
					   "\na/d: strafe left/right"
					   "\ne/c: move up/down"
					   "\nq/r: roll left/right"
					   "\nf: reset roll"
					   "\n"
					   "\nclick or tab: toggle camera control"
					   "\n1: reset camera to (0,0,600)"
					   "\ng: toggle full-screen"
					   "\nsee ofApp.cpp for available methods and vars",
					   30, 30);

	ofDrawBitmapString(camera.isControlled() ? "camera control is ON" : "camera control is OFF (click to grab)",
					   30, ofGetHeight()-30);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	// Note: w a s d e c q r f are consumed by the camera, so the app-level
	// shortcuts below deliberately stay clear of them.
	switch (key) {
		case OF_KEY_TAB:
			camera.toggleControl();
			break;

		case '1':
			resetCamera();
			break;

		case 'g':
			ofToggleFullscreen();
			break;

		default:
			break;
	}
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
	camera.toggleControl();
}
