

add_executable(testegl testegl.cpp EGLRenderLoop.cpp)
target_link_directories(testegl PRIVATE ${PROJECT_SOURCE_DIR}/lib)
target_link_libraries(testegl ${TARGET_NAME} EGL GLESv2)

if(WIN32)
  add_executable(testd3d11 testd3d11.cpp D3D11RenderLoop.cpp)
  target_link_libraries(testd3d11 ${TARGET_NAME} d3d11 d3dcompiler)
endif()
