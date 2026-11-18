#include <rex/platform.h>
#if REX_PLATFORM_ANDROID

#include <jni.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>

namespace {

SDL_JoystickID g_pad_id = 0;
SDL_Gamepad* g_pad = nullptr;

bool EnsurePad() {
  if (g_pad) {
    return true;
  }
  if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
    if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
      return false;
    }
  }
  if (g_pad_id == 0) {
    SDL_VirtualJoystickDesc desc{};
    desc.version = sizeof(desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1u;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1u;
    desc.name = "Sonic4 Touch";
    g_pad_id = SDL_AttachVirtualJoystick(&desc);
    if (g_pad_id == 0) {
      return false;
    }
  }
  g_pad = SDL_OpenGamepad(g_pad_id);
  return g_pad != nullptr;
}

SDL_Joystick* PadJoystick() {
  return g_pad ? SDL_GetGamepadJoystick(g_pad) : nullptr;
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_s4ep1recomp_sonic4_Sonic4Activity_nativeSetButton(JNIEnv*, jclass, jint button,
                                                           jboolean down) {
  if (!EnsurePad()) {
    return;
  }
  if (SDL_Joystick* joystick = PadJoystick()) {
    SDL_SetJoystickVirtualButton(joystick, button, down == JNI_TRUE);
    SDL_UpdateJoysticks();
  }
}

extern "C" JNIEXPORT void JNICALL
Java_com_s4ep1recomp_sonic4_Sonic4Activity_nativeSetAxis(JNIEnv*, jclass, jint axis, jint value) {
  if (!EnsurePad()) {
    return;
  }
  if (SDL_Joystick* joystick = PadJoystick()) {
    SDL_SetJoystickVirtualAxis(joystick, axis, static_cast<Sint16>(value));
    SDL_UpdateJoysticks();
  }
}

#endif
