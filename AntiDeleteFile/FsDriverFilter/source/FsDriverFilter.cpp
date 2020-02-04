#include <fltKernel.h>

PFLT_FILTER pFilterHandle = nullptr;
static UNICODE_STRING ProtectedExtension = RTL_CONSTANT_STRING(L"PROTECTED");

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT pDriverObject, _In_ PUNICODE_STRING pRegistryPath);

__declspec(code_seg("PAGE")) FLT_PREOP_CALLBACK_STATUS SEC_ENTRY PreAntiDelete(
	_Inout_ PFLT_CALLBACK_DATA pData, 
	_In_ PCFLT_RELATED_OBJECTS pFltObjects, 
	_Flt_CompletionContext_Outptr_ PVOID* pCompletionContext);

NTSTATUS SEC_ENTRY MiniUnload(FLT_FILTER_UNLOAD_FLAGS Flags);

#pragma alloc_text(INIT, DriverEntry)


CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
	{ IRP_MJ_CREATE,
		0,
		static_cast<PFLT_PRE_OPERATION_CALLBACK>(PreAntiDelete),
		nullptr,
		nullptr },
		// DELETE_ON_CLOSE creation flag.
		{ IRP_MJ_SET_INFORMATION,
			0, static_cast<PFLT_PRE_OPERATION_CALLBACK>(PreAntiDelete),
			nullptr,
			nullptr },		// FileInformationClass == FileDispositionInformation(Ex).

		{ IRP_MJ_OPERATION_END,
			0,
			nullptr,
			nullptr,
			nullptr}
};

const FLT_REGISTRATION FilterRegistration = {
	sizeof(FLT_REGISTRATION),
	FLT_REGISTRATION_VERSION,
	0,
	nullptr,
	Callbacks,
	static_cast<PFLT_FILTER_UNLOAD_CALLBACK>(MiniUnload),
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,

};

__declspec(code_seg("PAGE")) FLT_PREOP_CALLBACK_STATUS SEC_ENTRY PreAntiDelete(_Inout_ PFLT_CALLBACK_DATA pData,
	_In_ PCFLT_RELATED_OBJECTS pFltObjects,
	_Flt_CompletionContext_Outptr_ PVOID* pCompletionContext)
{
	PAGED_CODE(); //check IQRL LEVEL (IRQL <= APC_LEVEL)

	UNREFERENCED_PARAMETER(pCompletionContext);

	const auto ret = FLT_PREOP_SUCCESS_NO_CALLBACK;

	auto IsDirectory = false;

	auto status = FltIsDirectory(
		pFltObjects->FileObject,
		pFltObjects->Instance,
		reinterpret_cast<PBOOLEAN>(&IsDirectory));

	if (NT_SUCCESS(status) && IsDirectory)
	{
		return ret;
	}


	if (pData->Iopb->MajorFunction == IRP_MJ_CREATE
		&&
		!FlagOn(pData->Iopb->Parameters.Create.Options, FILE_DELETE_ON_CLOSE))
	{
		return ret;

	}

	if (pData->Iopb->MajorFunction == IRP_MJ_SET_INFORMATION) {
		switch (pData->Iopb->Parameters.SetFileInformation.FileInformationClass) {
		case FileRenameInformation:
		case FileRenameInformationEx:
		case FileDispositionInformation:
		case FileDispositionInformationEx:
		case FileRenameInformationBypassAccessCheck:
		case FileRenameInformationExBypassAccessCheck:
		case FileShortNameInformation:
			break;
		default:
			return ret;
		}
	}

	PFLT_FILE_NAME_INFORMATION pFileNameInfo = nullptr;

	if (pFltObjects->FileObject == nullptr)
	{
		return ret;
	}

	status = FltGetFileNameInformation(
		pData,
		FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
		&pFileNameInfo);


	if (!NT_SUCCESS(status))
	{
		return ret;
	}

	status = FltParseFileNameInformation(pFileNameInfo);

	if (!NT_SUCCESS(status))
	{
		return ret;
	}

	if (!RtlCompareUnicodeString(&pFileNameInfo->Extension, &ProtectedExtension, TRUE))
	{
		DbgPrint("Protecting file deletion/rename! File: %ws", pFileNameInfo->Name.Buffer);
		pData->IoStatus.Status = STATUS_ACCESS_DENIED;
		pData->IoStatus.Information = 0;

		return FLT_PREOP_COMPLETE;
	}


	return ret;
}

NTSTATUS SEC_ENTRY MiniUnload(IN FLT_FILTER_UNLOAD_FLAGS Flags)
{

	UNREFERENCED_PARAMETER(Flags);

	KdPrint(("WFP driver unload!"));

	if (pFilterHandle != nullptr)
	{
		FltUnregisterFilter(pFilterHandle);
	}

	return STATUS_SUCCESS;
}

extern "C" 
 NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT pDriverObject, _In_ PUNICODE_STRING pRegistryPath)
{

	UNREFERENCED_PARAMETER(pRegistryPath);
	NTSTATUS status = 0;
	KdPrint(("DriverEntry called.\n"));


	status = FltRegisterFilter(
		pDriverObject,
		&FilterRegistration,
		&pFilterHandle
	);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to register filter: <0x%08x>.\n", status));
		return status;
	}

	// Start filtering I/O.
	status = FltStartFiltering(pFilterHandle);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to start filter: <0x%08x>.\n", status));
		// If we fail, we need to unregister the minifilter.
		FltUnregisterFilter(pFilterHandle);
	}

	return status;
}
