#pragma once

#define IOCTL_THREAD_PRIORITY CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// struct used by the driver's client (usually from user mode) to ask the driver to set the priority of a given thread to a given value
typedef struct ThreadPriorityData {
	HANDLE hThread;
	int Priority;
} ThreadPriorityData;