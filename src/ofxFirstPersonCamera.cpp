#include "ofxFirstPersonCamera.h"

ofxFirstPersonCamera::ofxFirstPersonCamera()
{
  auto &events = ofEvents();
  ofAddListener(events.update      , this, &ofxFirstPersonCamera::update      , OF_EVENT_ORDER_BEFORE_APP);
  ofAddListener(events.keyPressed  , this, &ofxFirstPersonCamera::keyPressed  , OF_EVENT_ORDER_BEFORE_APP);
  ofAddListener(events.keyReleased , this, &ofxFirstPersonCamera::keyReleased , OF_EVENT_ORDER_BEFORE_APP);
  ofAddListener(events.mouseMoved  , this, &ofxFirstPersonCamera::mouseMoved  , OF_EVENT_ORDER_BEFORE_APP);
  ofAddListener(events.mouseDragged, this, &ofxFirstPersonCamera::mouseDragged, OF_EVENT_ORDER_BEFORE_APP);
}

ofxFirstPersonCamera::~ofxFirstPersonCamera()
{
  auto &events = ofEvents();
  ofRemoveListener(events.update      , this, &ofxFirstPersonCamera::update      , OF_EVENT_ORDER_BEFORE_APP);
  ofRemoveListener(events.keyPressed  , this, &ofxFirstPersonCamera::keyPressed  , OF_EVENT_ORDER_BEFORE_APP);
  ofRemoveListener(events.keyReleased , this, &ofxFirstPersonCamera::keyReleased , OF_EVENT_ORDER_BEFORE_APP);
  ofRemoveListener(events.mouseMoved  , this, &ofxFirstPersonCamera::mouseMoved  , OF_EVENT_ORDER_BEFORE_APP);
  ofRemoveListener(events.mouseDragged, this, &ofxFirstPersonCamera::mouseDragged, OF_EVENT_ORDER_BEFORE_APP);
}

bool ofxFirstPersonCamera::isControlled() const
{
  return m_isControlled;
}

void ofxFirstPersonCamera::toggleControl()
{
  m_isControlled ? disableControl() : enableControl();
}

void ofxFirstPersonCamera::enableControl()
{
#ifdef TARGET_GLFW_WINDOW
  auto win = dynamic_cast<ofAppGLFWWindow*>(ofGetWindowPtr());
  if (!win) {
    ofLogError("ofxFirstPersonCamera") << "needs an ofAppGLFWWindow, control not enabled";
    return;
  }

  m_glfwWindow = win->getGLFWWindow();
  glfwSetInputMode(m_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#else
  ofHideCursor();
#endif

  // Ignore whatever the pointer was doing before we grabbed it
  m_isMouseInited = false;
  centerCursor();

  m_isControlled = true;
}

void ofxFirstPersonCamera::disableControl()
{
#ifdef TARGET_GLFW_WINDOW
  if (m_glfwWindow) {
    glfwSetInputMode(m_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
#else
  ofShowCursor();
#endif

  m_isControlled = false;
}

// Puts the cursor back in the middle of the window so that every mouse event
// can be read as a delta from the center. glfwGetWindowSize() reports screen
// coordinates, which is also what glfwSetCursorPos() expects.
void ofxFirstPersonCamera::centerCursor()
{
#ifdef TARGET_GLFW_WINDOW
  if (!m_glfwWindow) return;

  int win_w;
  int win_h;
  glfwGetWindowSize(m_glfwWindow, &win_w, &win_h);
  glfwSetCursorPos(m_glfwWindow, win_w / 2.0, win_h / 2.0);
#endif
}

void ofxFirstPersonCamera::update(ofEventArgs&)
{
  if (!m_isControlled) return;

  // Keep movement frame rate independent, normalized to 60 fps. Guards
  // against the bogus frame times reported on the very first frames.
  float delta = ofGetLastFrameTime();
  if (delta <= 0.0f || delta > 0.25f) delta = 1.0f / 60.0f;
  const float step = delta * 60.0f;

  { // Roll
    const Actions doa = m_doa;
    const int rolldir = doa.RollLeft - doa.RollRight;

    if (rolldir) {
      this->rollDeg(rolldir * rollspeed * step);
      upvector = this->getUpDir();
    }

    if (doa.RollReset) {
      this->rollDeg(-this->getRollDeg());
      upvector = glm::vec3(0.0f, 1.0f, 0.0f);
    }
  }
  { // Position
    const Actions doa = m_doa;

    const float look = doa.Forward - doa.Backward;
    const float side = doa.Right   - doa.Left;
    const float up   = doa.Up      - doa.Down;

    if (look != 0 || side != 0 || up != 0)
    {
      const glm::vec3 lookdir = this->getLookAtDir();
      const glm::vec3 sidedir = this->getSideDir();
      const glm::vec3 updir   = this->getUpDir();
      const float speed = movespeed * step;
      this->move(lookdir * speed * look +
                 sidedir * speed * side +
                   updir * speed * up);
    }
  }
}

void ofxFirstPersonCamera::nodeRotate(ofMouseEventArgs& mouse)
{
  if (!m_isControlled) return;

#ifdef TARGET_GLFW_WINDOW
  if (!m_glfwWindow) return;
  const float win_center_x = ofGetWidth()  * 0.5f;
  const float win_center_y = ofGetHeight() * 0.5f;

  if (!m_isMouseInited) {
  // Swallows the first mouse move, which is a jump from wherever the
  // pointer happened to be rather than a real delta
    m_isMouseInited = true;
    centerCursor();
    return;
  }

  const float xdiff = (win_center_x - mouse.x) * sensitivity;
  const float ydiff = (win_center_y - mouse.y) * sensitivity;
#else
  if (!m_isMouseInited) {
    m_isMouseInited = true;
    return;
  }
  
  const float xdiff = (ofGetPreviousMouseX() - mouse.x) * sensitivity;
  const float ydiff = (ofGetPreviousMouseY() - mouse.y) * sensitivity;
#endif

  this->rotateDeg(ydiff, this->getSideDir());
  this->rotateDeg(xdiff, upvector);

  centerCursor();
}

void ofxFirstPersonCamera::mouseMoved(ofMouseEventArgs& mouse)
{
  nodeRotate(mouse);
}

void ofxFirstPersonCamera::mouseDragged(ofMouseEventArgs& mouse)
{
  nodeRotate(mouse);
}

void ofxFirstPersonCamera::keyPressed(ofKeyEventArgs& keys)
{
  Actions doa = m_doa;
#ifdef TARGET_GLFW_WINDOW
  const int key = keys.keycode;
#else
  const int key = keys.key;
#endif

  if      (key == keyUp       ) doa.Up        = true;
  else if (key == keyDown     ) doa.Down      = true;
  else if (key == keyLeft     ) doa.Left      = true;
  else if (key == keyRight    ) doa.Right     = true;
  else if (key == keyForward  ) doa.Forward   = true;
  else if (key == keyBackward ) doa.Backward  = true;

  else if (key == keyRollLeft ) doa.RollLeft  = true;
  else if (key == keyRollRight) doa.RollRight = true;
  else if (key == keyRollReset) doa.RollReset = true;

  m_doa = doa;
}

void ofxFirstPersonCamera::keyReleased(ofKeyEventArgs& keys)
{
  Actions doa = m_doa;
#ifdef TARGET_GLFW_WINDOW
  const int key = keys.keycode;
#else
  const int key = keys.key;
#endif

  if      (key == keyUp       ) doa.Up        = false;
  else if (key == keyDown     ) doa.Down      = false;
  else if (key == keyLeft     ) doa.Left      = false;
  else if (key == keyRight    ) doa.Right     = false;
  else if (key == keyForward  ) doa.Forward   = false;
  else if (key == keyBackward ) doa.Backward  = false;

  else if (key == keyRollLeft ) doa.RollLeft  = false;
  else if (key == keyRollRight) doa.RollRight = false;
  else if (key == keyRollReset) doa.RollReset = false;

  m_doa = doa;
}
