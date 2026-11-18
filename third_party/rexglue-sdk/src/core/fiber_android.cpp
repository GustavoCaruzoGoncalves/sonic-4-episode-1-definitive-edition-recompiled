#include <rex/platform.h>
#if REX_PLATFORM_ANDROID && REX_ARCH_ARM64

#include <rex/thread/fiber.h>

#include <cassert>
#include <cstdint>

// File-scope asm so this stays a C++ translation unit. Bionic has no ucontext.
// x0 = from, x1 = to. Layout matches Fiber::Context.
__asm__(
    ".text\n"
    ".global rex_fiber_swap\n"
    ".type rex_fiber_swap, %function\n"
    ".align 4\n"
    "rex_fiber_swap:\n"
    "stp x19, x20, [x0, #0]\n"
    "stp x21, x22, [x0, #16]\n"
    "stp x23, x24, [x0, #32]\n"
    "stp x25, x26, [x0, #48]\n"
    "stp x27, x28, [x0, #64]\n"
    "stp x29, x30, [x0, #80]\n"
    "mov x2, sp\n"
    "str x2, [x0, #96]\n"
    "stp d8, d9, [x0, #104]\n"
    "stp d10, d11, [x0, #120]\n"
    "stp d12, d13, [x0, #136]\n"
    "stp d14, d15, [x0, #152]\n"
    "ldp x19, x20, [x1, #0]\n"
    "ldp x21, x22, [x1, #16]\n"
    "ldp x23, x24, [x1, #32]\n"
    "ldp x25, x26, [x1, #48]\n"
    "ldp x27, x28, [x1, #64]\n"
    "ldr x2, [x1, #96]\n"
    "mov sp, x2\n"
    "ldp d8, d9, [x1, #104]\n"
    "ldp d10, d11, [x1, #120]\n"
    "ldp d12, d13, [x1, #136]\n"
    "ldp d14, d15, [x1, #152]\n"
    "ldp x29, x30, [x1, #80]\n"
    "ret\n"
    ".size rex_fiber_swap, .-rex_fiber_swap\n");

extern "C" void rex_fiber_swap(void* from, void* to);

namespace rex::thread {

thread_local Fiber* Fiber::tls_current_ = nullptr;

Fiber* Fiber::ConvertCurrentThread() {
  auto* f = new Fiber();
  f->is_thread_fiber_ = true;
  tls_current_ = f;
  return f;
}

Fiber* Fiber::Create(size_t stack_size, void (*entry)(void*), void* arg) {
  auto* f = new Fiber();
  f->entry_ = entry;
  f->arg_ = arg;
  f->stack_.resize(stack_size == 0 ? 65536 : stack_size);
  uintptr_t sp = reinterpret_cast<uintptr_t>(f->stack_.data() + f->stack_.size());
  sp &= ~uintptr_t{15};
  f->context_.sp = sp;
  f->context_.x19_x30[11] = reinterpret_cast<uintptr_t>(&Fiber::Trampoline);
  return f;
}

void Fiber::Trampoline() {
  Fiber* f = tls_current_;
  f->entry_(f->arg_);
}

void Fiber::SwitchTo(Fiber* target) {
  Fiber* from = tls_current_;
  tls_current_ = target;
  rex_fiber_swap(&from->context_, &target->context_);
}

void Fiber::Destroy() {
  if (is_thread_fiber_) {
    tls_current_ = nullptr;
  } else {
    assert(this != tls_current_ && "Destroy called on the currently running fiber");
  }
  delete this;
}

}  // namespace rex::thread

#endif
