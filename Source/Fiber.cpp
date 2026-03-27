#include "Fiber.h"
#include <Rush/UtilLog.h>

#if defined(__linux__) || defined(__APPLE__)

void* convertThreadToFiber()
{
	Log::error("convertThreadToFiber not implemented");
	return nullptr; // TODO
}

void* createFiber(void (*startAddress)(void*), void* userData)
{
	Log::error("createFiber not implemented");
	return nullptr; // TODO
}

void switchToFiber(void* fiberAddress)
{
	Log::error("switchToFiber not implemented");
	// TODO
}

#else
#include <windows.h>

void* convertThreadToFiber() { return ConvertThreadToFiber(nullptr); }

void* createFiber(void (*startAddress)(void*), void* userData)
{
	return CreateFiber(0, (LPFIBER_START_ROUTINE)startAddress, userData);
}

void switchToFiber(void* fiberAddress) { SwitchToFiber(fiberAddress); }

#endif
