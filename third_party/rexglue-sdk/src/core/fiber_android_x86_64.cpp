#include <rex/platform.h>
#if REX_PLATFORM_ANDROID && REX_ARCH_AMD64

#include <rex/thread/fiber.h>

#include <cassert>
#include <cstdint>

// SysV AMD64 fiber swap. rdi = from, rsi = to. Layout matches Fiber::Context.
__asm__(
    ".text\n"
    ".globl rex_fiber_swap\n"
    ".type rex_fiber_swap,@function\n"
    ".p2align 4\n"
    "rex_fiber_swap:\n"
    "    movq %rbx, 0(%rdi)\n"
    "    movq %rbp, 8(%rdi)\n"
    "    movq %r12, 16(%rdi)\n"
    "    movq %r13, 24(%rdi)\n"
    "    movq %r14, 32(%rdi)\n"
    "    movq %r15, 40(%rdi)\n"
    "    movq %rsp, 48(%rdi)\n"
    "    leaq 1f(%rip), %rax\n"
    "    movq %rax, 56(%rdi)\n"
    "    movdqu %xmm6, 64(%rdi)\n"
    "    movdqu %xmm7, 80(%rdi)\n"
    "    movdqu %xmm8, 96(%rdi)\n"
    "    movdqu %xmm9, 112(%rdi)\n"
    "    movdqu %xmm10, 128(%rdi)\n"
    "    movdqu %xmm11, 144(%rdi)\n"
    "    movdqu %xmm12, 160(%rdi)\n"
    "    movdqu %xmm13, 176(%rdi)\n"
    "    movdqu %xmm14, 192(%rdi)\n"
    "    movdqu %xmm15, 208(%rdi)\n"

    "    movq 0(%rsi), %rbx\n"
    "    movq 8(%rsi), %rbp\n"
    "    movq 16(%rsi), %r12\n"
    "    movq 24(%rsi), %r13\n"
    "    movq 32(%rsi), %r14\n"
    "    movq 40(%rsi), %r15\n"
    "    movq 48(%rsi), %rsp\n"
    "    movdqu 64(%rsi), %xmm6\n"
    "    movdqu 80(%rsi), %xmm7\n"
    "    movdqu 96(%rsi), %xmm8\n"
    "    movdqu 112(%rsi), %xmm9\n"
    "    movdqu 128(%rsi), %xmm10\n"
    "    movdqu 144(%rsi), %xmm11\n"
    "    movdqu 160(%rsi), %xmm12\n"
    "    movdqu 176(%rsi), %xmm13\n"
    "    movdqu 192(%rsi), %xmm14\n"
    "    movdqu 208(%rsi), %xmm15\n"
    "    jmp *56(%rsi)\n"
    "1:\n"
    "    ret\n");

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
  f->context_.rip = reinterpret_cast<uintptr_t>(&Fiber::Trampoline);
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

#endif  // REX_PLATFORM_ANDROID && REX_ARCH_AMD64