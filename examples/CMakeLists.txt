add_executable(testegl testegl.cpp EGLRenderLoop.cpp)
target_link_directories(testegl PRIVATE ${PROJECT_SOURCE_DIR}/lib)
target_link_libraries(testegl ${TARGET_NAME} EGL GLESv2)
