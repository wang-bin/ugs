# UGS: Universal Graphic Surface

### PlatformSurface

The layer between platform depended window/view/surface handle and GFX context. For example OpenGL context can be created via `nativeHandleForGL()` depending on `type()` and `nativeResource()` when necessary.

- A wrapper for platform dependent handle: macOS NSView, iOS UIView, android jni Surface object
- Internally created handle: win32, x11, gbm, wayland, rpi dispmanx


### RenderLoop

It's an abstract class implements threaded rendering loop with any number of `PlatformSurface`.

A derived class only need to implement how to manage GFX context for a given surface, for example, create/destroy/active/swap OpenGL context.

Do your rendering jobs in `RenderLoop` callbacks.
