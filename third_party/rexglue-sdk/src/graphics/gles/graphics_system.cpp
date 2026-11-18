#include <rex/graphics/gles/graphics_system.h>

#include <rex/logging.h>

namespace rex::graphics::gles {

GLESGraphicsSystem::GLESGraphicsSystem() {}

GLESGraphicsSystem::~GLESGraphicsSystem() {}

std::string GLESGraphicsSystem::name() const {
  return "OpenGL ES (experimental)";
}

void GLESGraphicsSystem::CreateProvider(bool) {
  REXGPU_ERROR("GLES backend: CreateProvider not implemented");
}

std::unique_ptr<CommandProcessor> GLESGraphicsSystem::CreateCommandProcessor() {
  REXGPU_ERROR("GLES backend: CreateCommandProcessor not implemented");
  return nullptr;
}

}  // namespace rex::graphics::gles