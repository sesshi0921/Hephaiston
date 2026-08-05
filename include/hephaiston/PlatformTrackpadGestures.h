#pragma once

struct GLFWwindow;

namespace hephaiston {

using TrackpadPinchCallback = void (*)(void* userData, float magnification);
using TrackpadScrollCallback = void (*)(void* userData, float deltaY);

// Registers a native pinch recognizer when the platform exposes one. The
// no-op implementation keeps the Core portable on GLFW platforms without a
// magnification event API.
void installTrackpadGestureCallbacks(GLFWwindow* window, void* userData, TrackpadPinchCallback pinchCallback, TrackpadScrollCallback scrollCallback);
void removeTrackpadPinchCallback();

} // namespace hephaiston
