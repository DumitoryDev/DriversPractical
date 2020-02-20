#include <ntifs.h>
#include <ntddk.h>
#include "../../Common/Common.h"


UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\PriorityBooster");
UNICODE_STRING SymLinkDevice  = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");

DRIVER_DISPATCH PriorityBoosterCreateClose;
DRIVER_DISPATCH PriorityBoosterDeviceControl;
DRIVER_UNLOAD Unload;

extern "C" __declspec(code_seg("INIT"))
DRIVER_INITIALIZE DriverEntry;

NTSTATUS SetBoosterThread(IN PIO_STACK_LOCATION stack);

extern "C" __declspec(code_seg("INIT"))
NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegPath)
{

	UNREFERENCED_PARAMETER(pDriverObject);
	UNREFERENCED_PARAMETER(pRegPath);

	PDEVICE_OBJECT pDeviceObject = nullptr;

	auto status = ::IoCreateDevice(
		pDriverObject,
		0,
		&DeviceName,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&pDeviceObject
	);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create device object! Code error - (0x%08x)\n",status));
	}

	
	::IoCreateSymbolicLink(&SymLinkDevice,&DeviceName);


	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create symbol link! Code error - (0x%08x)\n",status));
		if(pDeviceObject != nullptr) 	
			::IoDeleteDevice(pDeviceObject);
	}

	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;
	pDriverObject->DriverUnload = Unload;
	
	return STATUS_SUCCESS;
}

__declspec(code_seg("PAGE"))
void Unload(IN PDRIVER_OBJECT pDriverObject)
{
	PAGED_CODE();
	
	::IoDeleteSymbolicLink(&SymLinkDevice);
	::IoDeleteDevice(pDriverObject->DeviceObject);
	
}

_Use_decl_annotations_  __declspec(code_seg("PAGE"))
NTSTATUS PriorityBoosterCreateClose(IN PDEVICE_OBJECT pDeviceObject,IN PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDeviceObject);
	PAGED_CODE();
	/*
	    An IRP is a semi-documented structure that represents a request, typically coming from one of the
		managers in the Executive: I/O Manager, Plug & Play Manager or Power Manager. With a simple
		software driver, that would most likely be the I/O Manager. Regardless of the creator of the IRP, the
		driver’s purpose is to handle the IRP, which means looking at the details of the request and doing
		what needs to be done to complete it.
	 */
	
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	
	/*
	 *  To actually complete the IRP, we call IoCompleteRequest. This function has a lot to do, but basically it propagates the IRP back to its  creator (typically the I/O Manager) and that manager notifies
		the client that the operation has completed. The second argument is a temporary priority boost
		value that a driver can provide to its client. In most cases a value of zero is best (IO_NO_INCREMENT
		is defined as zero), because the request completed synchronously, so no reason the caller should get
		a priority boost. Again, more information on this function is provided in chapter 6.

	 */
	
	IoCompleteRequest(pIrp,IO_NO_INCREMENT);
	return STATUS_SUCCESS;
		
}

_Use_decl_annotations_ __declspec(code_seg("PAGE"))
NTSTATUS PriorityBoosterDeviceControl(IN PDEVICE_OBJECT pDeviceObject,IN PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDeviceObject);

	PAGED_CODE();
	
	const auto stack = ::IoGetCurrentIrpStackLocation(pIrp);
	auto status = STATUS_SUCCESS;

	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{

	case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY:
		status = SetBoosterThread(stack);
		break;

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;		
	}
	
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = 0;
	::IoCompleteRequest(pIrp,IO_NO_INCREMENT);
		
	return status;
		
}

__declspec(code_seg("PAGE"))
NTSTATUS SetBoosterThread(IN PIO_STACK_LOCATION stack)
{
	PAGED_CODE();
	auto status = STATUS_SUCCESS;
	
	if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ThreadData))
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	const auto data = static_cast<ThreadData*>(stack->Parameters.DeviceIoControl.Type3InputBuffer);

	if (data == nullptr)
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (data->Priority < 1 || data->Priority > 31)
	{
		return STATUS_INVALID_PARAMETER;
	}

	PETHREAD Thread{};

	status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadId), &Thread);

	if (!NT_SUCCESS(status))
	{
		return status;
	}
	
	KeSetPriorityThread(static_cast<PKTHREAD>(Thread),data->Priority);
	ObDereferenceObjectDeferDelete(Thread);
	KdPrint(("Thread Priority change for %d to %d succeeded!",data->ThreadId, data->Priority));
	
	return status;
	
}

