#pragma once

#if defined(__linux__) || defined(__APPLE__)
#define FIBER_CALL
#else
#define FIBER_CALL __stdcall
#endif

void* convertThreadToFiber();
void* createFiber(void (*startAddress)(void*), void* userData);
void  switchToFiber(void* fiberAddress);
