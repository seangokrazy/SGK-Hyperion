#pragma once
#include <cstdint>
// Prevent subnormal tails from taking the slow floating-point path. Restore
// the caller's mode before returning to the host.
struct AudioFP {
#if defined(__arm__)
 uint32_t previous;
 AudioFP(){asm volatile("vmrs %0, fpscr":"=r"(previous));uint32_t mode=previous|(1u<<24);asm volatile("vmsr fpscr, %0"::"r"(mode));}
 ~AudioFP(){asm volatile("vmsr fpscr, %0"::"r"(previous));}
#endif
};
