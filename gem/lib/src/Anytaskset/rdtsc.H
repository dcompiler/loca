#ifndef _SFP_RDTSC_H_
#define _SFP_RDTSC_H_

uint64_t __inline__ SFP_RDTSC() {
  unsigned int hi, lo;
  __asm__ volatile("xor %%eax, %%eax\n\t"
                   "cpuid\n\t"
                   "rdtsc\n\t"
                   "mov %%edx, %0\n\t"
                   "mov %%eax, %1\n\t" : "=r" (hi), "=r" (lo):: "eax", "ebx", "ecx", "edx");
  uint64_t ret = ((uint64_t)hi << 32) | lo;
  return ret / 1024;
}

#endif
