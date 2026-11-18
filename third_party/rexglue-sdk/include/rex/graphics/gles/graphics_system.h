#pragma once

#include <rex/graphics/graphics_system.h>

namespace rex::graphics::gles {

// Experimental scaffold — not instantiated in default builds.
class GLESGraphicsSystem : public GraphicsSystem {
 public:
  GLESGraphicsSystem();
  ~GLESGraphicsSystem() override;

  std::string name() const override;

 protected:
  void CreateProvider(bool with_presentation) override;
  std::unique_ptr<CommandProcessor> CreateCommandProcessor() override;
};

}  // namespace rex::graphics::gles