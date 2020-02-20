#include <cstdio>
#include <cstdlib>
#include "../../Common/Common.h"
#include <Windows.h>

int error(char const* message)
{
	printf("%s (error = %lu)", message, ::GetLastError());
	return 1;
}

int main(const int argc, char const* argv[])
{

	if (argc < 3)
	{
		printf("Usage: Booster <threadid> <priority>\n");
		return 0;
	}
	
	const auto hDevice = ::CreateFile(
		LR"(\\.\PriorityBooster)",
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr
	);

	if (hDevice == INVALID_HANDLE_VALUE)
		return error("Failed to open device!");

	char* end{};

	ThreadData data{};

	data.ThreadId = std::strtol(argv[1], &end, 10);
	data.Priority = std::strtol(argv[2], &end, 10);

	DWORD returned{};

	const bool result = ::DeviceIoControl(
		hDevice,
		IOCTL_PRIORITY_BOOSTER_SET_PRIORITY,
		&data,
		sizeof(data),
		nullptr,
		0,
		&returned,
		nullptr);

	result ? printf("Priority change succeeded!") : error("Priority change failed!");

	::CloseHandle(hDevice);
	
}

