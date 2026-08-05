#include "hephaiston/PlatformTrackpadGestures.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>

namespace hephaiston {
namespace {
id gGestureMonitor = nil;
NSWindow* gTargetWindow = nil;
void* gUserData = nullptr;
TrackpadPinchCallback gCallback = nullptr;
TrackpadScrollCallback gScrollCallback = nullptr;
}

void installTrackpadGestureCallbacks(GLFWwindow* window, void* userData, TrackpadPinchCallback pinchCallback, TrackpadScrollCallback scrollCallback) {
    removeTrackpadPinchCallback();
    if (!window || !pinchCallback) return;
    gTargetWindow = glfwGetCocoaWindow(window);
    gUserData = userData;
    gCallback = pinchCallback;
    gScrollCallback = scrollCallback;
    gGestureMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskMagnify | NSEventMaskScrollWheel)
        handler:^NSEvent* (NSEvent* event) {
            if (event.window == gTargetWindow && event.type == NSEventTypeMagnify && gCallback) {
                // Positive magnification is pinch-out, matching viewport
                // zoom-in's positive wheel convention.
                gCallback(gUserData, static_cast<float>(event.magnification));
            }
            if (event.window == gTargetWindow && event.type == NSEventTypeScrollWheel &&
                event.hasPreciseScrollingDeltas && gScrollCallback) {
                gScrollCallback(gUserData, static_cast<float>(event.scrollingDeltaY));
            }
            return event;
        }];
}

void removeTrackpadPinchCallback() {
    if (gGestureMonitor != nil) {
        [NSEvent removeMonitor:gGestureMonitor];
        gGestureMonitor = nil;
    }
    gTargetWindow = nil;
    gUserData = nullptr;
    gCallback = nullptr;
    gScrollCallback = nullptr;
}

} // namespace hephaiston
