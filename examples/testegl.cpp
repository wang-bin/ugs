#include "EGLRenderLoop.h"
#include "ugs/PlatformSurface.h"
#include <GLES2/gl2.h>
#include <iostream>

int main(int argc, char* argv[])
{
  EGLRenderLoop loop;
  loop.onDraw([](PlatformSurface* s) {
    float r = float(intptr_t(s) % 1000) / 1000.0f;
    glClearColor(r, 1.0f, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return true;
  }).onResize([](PlatformSurface*, int w, int h) {
      glViewport(0, 0, w, h);
  }).onContextCreated([](PlatformSurface*, void*) {
    std::clog << "onContextCreated" << std::endl;
  }).onDestroyContext([](PlatformSurface*, void*) {
    std::clog << "onDestroyContext" << std::endl;
  });

  auto surface = loop.add(PlatformSurface::create()).lock();
  surface->resize(640, 480);
  surface = loop.add(PlatformSurface::create()).lock();
  surface->resize(480, 320);
  loop.start();
  loop.update();
  loop.waitForStopped();
  return 0;
}