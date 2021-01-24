#include <ntddk.h>
#include "ThreadPriority.h"

void ThreadPrirotyUnload(PDRIVER_OBJECT DriverObject);
// all dispatch routines have the same prototype
NTSTATUS ThreadPriorityCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS ThreadPriorityDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

#define THREAD_PRIORITY_PREFIX "ThreadPriority: "

class AutoEnterLeaveFunction {
	LPCSTR _function;
public:
	AutoEnterLeaveFunction(LPCSTR function) {
		KdPrint((THREAD_PRIORITY_PREFIX "Enter: %s\n", _function = function));
	}
	~AutoEnterLeaveFunction() {
		KdPrint((THREAD_PRIORITY_PREFIX "Leave: %s\n", _function));
	}
};

#define AUTO_ENTER_LEAVE() AutoEnterLeaveFunction _aelf(__FUNCTION__)


extern "C" // this function is used by the kernel, and the kernel expects this function to have C linkage and not CPP linkage
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	AUTO_ENTER_LEAVE();

	DriverObject->DriverUnload = ThreadPrirotyUnload;
	// in this driver we only need to let other programs open and close a handle to our device, thus the same function will do for create and close
	DriverObject->MajorFunction[IRP_MJ_CREATE] =
		DriverObject->MajorFunction[IRP_MJ_CLOSE] = ThreadPriorityCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ThreadPriorityDeviceControl;
	PDEVICE_OBJECT DeviceObject;
	UNICODE_STRING deviceName /* name for kernel mode access */, NameForUserModeAccess /* name for user mode access */;
	RtlInitUnicodeString(&deviceName, L"\\Device\\ThreadPriority");
	NTSTATUS status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (status != STATUS_SUCCESS) {
		KdPrint((THREAD_PRIORITY_PREFIX "Error creating device, error number: %d\n", status));
		return status;
	}

	RtlInitUnicodeString(&NameForUserModeAccess, L"\\??\\ThreadPriority");
	IoCreateSymbolicLink(&NameForUserModeAccess, &deviceName);


	return STATUS_SUCCESS;
}


void ThreadPrirotyUnload(PDRIVER_OBJECT DriverObject) {
	AUTO_ENTER_LEAVE();

	UNICODE_STRING deviceName;
	RtlInitUnicodeString(&deviceName, L"\\??\\ThreadPriority");
	IoDeleteSymbolicLink(&deviceName);

	IoDeleteDevice(DriverObject->DeviceObject);
}


// This function is used to allow other programs open and close a handle to this driver's device
NTSTATUS ThreadPriorityCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	AUTO_ENTER_LEAVE();

	// the usual routine to return gracefully
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0; // number of bytes returned as a result
	IoCompleteRequest(Irp, IO_NO_INCREMENT); // completes the IRP and returns it to the IO manager
	return STATUS_SUCCESS;
}

// this is the implementation of the main functionality of this driver (setting the priority of a thread)
NTSTATUS ThreadPriorityDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	AUTO_ENTER_LEAVE();
	auto stack = IoGetCurrentIrpStackLocation(Irp); // getting parameters from the requester
	NTSTATUS status = STATUS_UNSUCCESSFUL; // initialize status to unsuccessful
	switch (stack->Parameters.DeviceIoControl.IoControlCode /*IoControlCode is the reason of call*/) {
	case IOCTL_THREAD_PRIORITY:
		if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ThreadPriorityData)) {
			status = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		ThreadPriorityData* data = (ThreadPriorityData*)(Irp->AssociatedIrp.SystemBuffer); // since we are using the "buffered IO" method to get data from the client, the IO manager put the client's buffer at this address

		// setting the threads priority
		PKTHREAD Thread;
		// this function gets a handle and returns the object of that handle (we will need the to give the object to another function). note that this function increments the counter of references to the object
		status = ObReferenceObjectByHandle(data->hThread, THREAD_SET_INFORMATION, *PsThreadType, UserMode, (PVOID*)&Thread, nullptr);
		if (status != STATUS_SUCCESS) break;
		KeSetPriorityThread(Thread, data->Priority);
		ObDereferenceObject(Thread); // decrementing the counter of references to the object

		status = STATUS_SUCCESS;
		break;
	}

	// the usual routine to return gracefully
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}