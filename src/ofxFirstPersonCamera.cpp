#include "ofxFirstPersonCamera.h"

#ifdef OFX_FPC_EVDEV_MOUSE
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <cstring>

namespace {

// ofAppEGLWindow multiplies every relative motion count by its mouseScaleX/Y,
// which default to 2.0. Match it so that a given sensitivity feels the same
// whether we read the device ourselves or fall back to window mouse events.
const float kEglMouseScale = 2.0f;

constexpr int kBitsPerLong = sizeof(long) * 8;

constexpr int bitmapLongs(int bits)
{
  return (bits - 1) / kBitsPerLong + 1;
}

bool testBit(int bit, const unsigned long* bitmap)
{
  return (bitmap[bit / kBitsPerLong] >> (bit % kBitsPerLong)) & 1ul;
}

// A pointing device we can use is one that reports relative motion on both
// axes and has at least a left button: that rules out volume knobs, tilt
// wheels and the multi-touch/absolute devices that only speak EV_ABS.
bool isRelativePointer(int fd)
{
  unsigned long types[bitmapLongs(EV_MAX)];
  memset(types, 0, sizeof(types));
  if (ioctl(fd, EVIOCGBIT(0, sizeof(types)), types) < 0) return false;
  if (!testBit(EV_REL, types) || !testBit(EV_KEY, types)) return false;

  unsigned long axes[bitmapLongs(REL_MAX)];
  memset(axes, 0, sizeof(axes));
  if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(axes)), axes) < 0) return false;
  if (!testBit(REL_X, axes) || !testBit(REL_Y, axes)) return false;

  unsigned long keys[bitmapLongs(KEY_MAX)];
  memset(keys, 0, sizeof(keys));
  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) return false;

  return testBit(BTN_LEFT, keys);
}

} // namespace
#endif

namespace {

// Bindings are matched case insensitively. The ASCII path reports the
// character the keyboard produced, so tapping shift halfway through a
// movement turns 'w' into 'W': the release no longer matches the binding
// that the press did and the action stays latched on until the key is
// pressed and released again with the same modifiers. Folding both the
// incoming key and the binding to lower case keeps a press and its release
// looking alike. GLFW keycodes for letters are the upper case ASCII values
// and nothing else lives in the 'a'-'z' range, so this is a no-op there
// beyond letting users write their bindings either way.
int foldKey(int key)
{
  if (key >= 'A' && key <= 'Z') return key - 'A' + 'a';
  return key;
}

bool keyMatches(int key, int binding)
{
  return foldKey(key) == foldKey(binding);
}

} // namespace

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

#ifdef OFX_FPC_EVDEV_MOUSE
  closeMouseDevices();
#endif
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

#ifdef OFX_FPC_EVDEV_MOUSE
  openMouseDevices();

  // Throw away whatever piled up in the device buffers while we were not
  // looking, otherwise the camera jumps on the first frame
  float discard_x;
  float discard_y;
  pollMouseDevices(discard_x, discard_y);
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

#ifdef OFX_FPC_EVDEV_MOUSE
  closeMouseDevices();
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

#ifdef OFX_FPC_EVDEV_MOUSE

// The EGL window has no cursor to warp, so instead of reading a position and
// pushing it back to the center we read the raw motion counts the mice emit.
// Those have no bounds to begin with, which is exactly what we want.
void ofxFirstPersonCamera::openMouseDevices()
{
  closeMouseDevices();

  DIR* dir = opendir("/dev/input");
  if (!dir) {
    ofLogWarning("ofxFirstPersonCamera") << "cannot read /dev/input, falling back to window mouse events";
    return;
  }

  while (const dirent* entry = readdir(dir)) {
    if (strncmp(entry->d_name, "event", 5) != 0) continue;

    const std::string node = "/dev/input/" + std::string(entry->d_name);

    const int fd = open(node.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;

    if (!isRelativePointer(fd)) {
      close(fd);
      continue;
    }

    // Exclusive access keeps ofAppEGLWindow (and every other reader) from
    // seeing the same motion, at the price of the app losing mouse buttons
    if (grabMouseDevice && ioctl(fd, EVIOCGRAB, 1) < 0) {
      ofLogWarning("ofxFirstPersonCamera") << "could not grab " << node;
    }

    ofLogVerbose("ofxFirstPersonCamera") << "reading raw mouse motion from " << node;
    m_mouseFds.push_back(fd);
  }

  closedir(dir);

  if (m_mouseFds.empty()) {
    ofLogWarning("ofxFirstPersonCamera")
      << "no readable mouse in /dev/input (is this user in the 'input' group?), "
      << "falling back to window mouse events, which stop at the window edges";
  }
}

void ofxFirstPersonCamera::closeMouseDevices()
{
  for (const int fd : m_mouseFds) {
    if (grabMouseDevice) ioctl(fd, EVIOCGRAB, 0);
    close(fd);
  }

  m_mouseFds.clear();
}

// Drains every pending event and sums the motion since the last frame.
// Returns false when there is no device to read, so that the caller can fall
// back to the window's own mouse events.
bool ofxFirstPersonCamera::pollMouseDevices(float& xdelta, float& ydelta)
{
  xdelta = 0.0f;
  ydelta = 0.0f;

  if (m_mouseFds.empty()) return false;

  for (const int fd : m_mouseFds) {
    input_event ev;

    while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
      if (ev.type != EV_REL) continue;

      if      (ev.code == REL_X) xdelta += ev.value;
      else if (ev.code == REL_Y) ydelta += ev.value;
    }
  }

  return true;
}

// Deltas are physical mouse movement, already accumulated over the frame, so
// unlike the movement keys they must not be scaled by the frame time
void ofxFirstPersonCamera::applyRawRotation()
{
  float xdelta;
  float ydelta;
  if (!pollMouseDevices(xdelta, ydelta)) return;
  if (xdelta == 0.0f && ydelta == 0.0f) return;

  const float xdiff = -xdelta * kEglMouseScale * sensitivity;
  const float ydiff = -ydelta * kEglMouseScale * sensitivity;

  this->rotateDeg(ydiff, this->getSideDir());
  this->rotateDeg(xdiff, upvector);
}

#endif

void ofxFirstPersonCamera::update(ofEventArgs&)
{
  if (!m_isControlled) return;

#ifdef OFX_FPC_EVDEV_MOUSE
  applyRawRotation();
#endif

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
#ifdef OFX_FPC_EVDEV_MOUSE
  // update() is already turning the camera with the unclamped device deltas
  if (!m_mouseFds.empty()) return;
#endif

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

  if      (keyMatches(key, keyUp       )) doa.Up        = true;
  else if (keyMatches(key, keyDown     )) doa.Down      = true;
  else if (keyMatches(key, keyLeft     )) doa.Left      = true;
  else if (keyMatches(key, keyRight    )) doa.Right     = true;
  else if (keyMatches(key, keyForward  )) doa.Forward   = true;
  else if (keyMatches(key, keyBackward )) doa.Backward  = true;

  else if (keyMatches(key, keyRollLeft )) doa.RollLeft  = true;
  else if (keyMatches(key, keyRollRight)) doa.RollRight = true;
  else if (keyMatches(key, keyRollReset)) doa.RollReset = true;

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

  if      (keyMatches(key, keyUp       )) doa.Up        = false;
  else if (keyMatches(key, keyDown     )) doa.Down      = false;
  else if (keyMatches(key, keyLeft     )) doa.Left      = false;
  else if (keyMatches(key, keyRight    )) doa.Right     = false;
  else if (keyMatches(key, keyForward  )) doa.Forward   = false;
  else if (keyMatches(key, keyBackward )) doa.Backward  = false;

  else if (keyMatches(key, keyRollLeft )) doa.RollLeft  = false;
  else if (keyMatches(key, keyRollRight)) doa.RollRight = false;
  else if (keyMatches(key, keyRollReset)) doa.RollReset = false;

  m_doa = doa;
}
