#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../fvelib/fvelib.h"

static void PrintUsage(void)
{
	printf("Usage:\n");
	printf("  fvecli status <volume>\n");
	printf("  fvecli unlock-password <volume> <password>\n");
	printf("  fvecli unlock-recovery <volume> <recovery-password>\n");
	printf("  fvecli lock <volume> [--dismount]\n");
	printf("  fvecli decrypt <volume> [flags]\n");
	printf("  fvecli off <volume> [flags]\n");
	printf("\n");
	printf("Examples:\n");
	printf("  fvecli status C:\n");
	printf("  fvecli unlock-password D: my-password\n");
	printf("  fvecli unlock-recovery D: 111111-222222-333333-444444-555555-666666-777777-888888\n");
	printf("  fvecli off D:\n");
}

static HRESULT MbsToWide(const char* input, PWSTR* output)
{
	int cch;
	PWSTR buffer;

	if (input == NULL || output == NULL)
		return E_INVALIDARG;

	*output = NULL;
	cch = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, input, -1, NULL, 0);
	if (cch == 0)
		return HRESULT_FROM_WIN32(GetLastError());

	buffer = (PWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)cch * sizeof(WCHAR));
	if (buffer == NULL)
		return E_OUTOFMEMORY;

	if (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, input, -1, buffer, cch) == 0)
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		HeapFree(GetProcessHeap(), 0, buffer);
		return hr;
	}

	*output = buffer;
	return S_OK;
}

static void FreeWide(PWSTR value)
{
	if (value != NULL)
		HeapFree(GetProcessHeap(), 0, value);
}

static int ParseDword(const char* text, DWORD* value)
{
	char* end = NULL;
	unsigned long parsed;

	if (text == NULL || value == NULL || text[0] == '\0')
		return 0;

	errno = 0;
	parsed = strtoul(text, &end, 0);
	if (errno != 0 || end == text || *end != '\0' || parsed > 0xFFFFFFFFul)
		return 0;

	*value = (DWORD)parsed;
	return 1;
}

static void PrintHr(const char* operation, HRESULT hr)
{
	printf("%s failed: hr=0x%08lX (%s)\n", operation, (unsigned long)(DWORD)hr, FveLibErrorName(hr));
}

static int IsLockedStatusHr(HRESULT hr)
{
	if (FVE_LIB_HRESULT_IS_VOLUME_LOCKED(hr))
		return 1;

	switch ((DWORD)hr)
	{
	case 0x8031000Du:
	case 0x80310044u:
		return 1;
	default:
		return 0;
	}
}

static void PrintVolumeInfo(const FVE_LIB_VOLUME_INFO* info)
{
	printf("VolumeStatus:      %s (%lu)\n", FveLibVolumeStatusName(info->VolumeStatus), (unsigned long)info->VolumeStatus);
	printf("ProtectionStatus:  %s (%lu)\n", FveLibProtectionStatusName(info->ProtectionStatus), (unsigned long)info->ProtectionStatus);
	printf("LockStatus:        %s (%lu)\n", FveLibLockStatusName(info->LockStatus), (unsigned long)info->LockStatus);
	printf("EncryptedPercent:  %u%%\n", (unsigned int)info->EncryptionPercentage);
	printf("EncryptionFlags:   0x%08lX\n", (unsigned long)info->EncryptionFlags);
	printf("VolumeSize:        %llu bytes\n", (unsigned long long)info->VolumeSize);
	printf("EncryptedSize:     %llu bytes\n", (unsigned long long)info->EncryptedSize);
}

static int InitFveLib(void)
{
	HRESULT hr = FveLibInit();
	if (FAILED(hr))
	{
		PrintHr("FveLibInit", hr);
		return 0;
	}
	return 1;
}

static int RunStatus(const char* volumeArg)
{
	PWSTR volume = NULL;
	FVE_LIB_VOLUME_INFO info;
	HRESULT hr;

	hr = MbsToWide(volumeArg, &volume);
	if (FAILED(hr))
	{
		PrintHr("MbsToWide(volume)", hr);
		return 2;
	}

	hr = FveLibGetStatusByPath(volume, &info);
	FreeWide(volume);
	if (FVE_LIB_FAILED(hr))
	{
		if (IsLockedStatusHr(hr))
		{
			ZeroMemory(&info, sizeof(info));
			info.VolumeStatus = FVE_LIB_VOLUME_FULLY_ENCRYPTED;
			info.ProtectionStatus = FVE_LIB_PROTECTION_ON;
			info.LockStatus = FVE_LIB_LOCK_LOCKED;
			info.EncryptionPercentage = 100;
			PrintVolumeInfo(&info);
			return 0;
		}
		PrintHr("FveLibGetStatusByPath", hr);
		return 2;
	}

	PrintVolumeInfo(&info);
	return 0;
}

static int RunUnlockPassword(const char* volumeArg, const char* passwordArg)
{
	PWSTR volume = NULL;
	PWSTR password = NULL;
	HRESULT hr;

	hr = MbsToWide(volumeArg, &volume);
	if (FAILED(hr))
	{
		PrintHr("MbsToWide(volume)", hr);
		return 2;
	}
	hr = MbsToWide(passwordArg, &password);
	if (FAILED(hr))
	{
		FreeWide(volume);
		PrintHr("MbsToWide(password)", hr);
		return 2;
	}

	hr = FveLibUnlockWithPasswordByPath(volume, password);
	FreeWide(password);
	FreeWide(volume);
	if (FVE_LIB_FAILED(hr))
	{
		PrintHr("FveLibUnlockWithPasswordByPath", hr);
		return 2;
	}

	printf("Volume unlocked with password.\n");
	return 0;
}

static int RunUnlockRecovery(const char* volumeArg, const char* recoveryArg)
{
	PWSTR volume = NULL;
	PWSTR recovery = NULL;
	HRESULT hr;

	hr = MbsToWide(volumeArg, &volume);
	if (FAILED(hr))
	{
		PrintHr("MbsToWide(volume)", hr);
		return 2;
	}
	hr = MbsToWide(recoveryArg, &recovery);
	if (FAILED(hr))
	{
		FreeWide(volume);
		PrintHr("MbsToWide(recovery)", hr);
		return 2;
	}

	hr = FveLibUnlockWithRecoveryPasswordByPath(volume, recovery);
	FreeWide(recovery);
	FreeWide(volume);
	if (FVE_LIB_FAILED(hr))
	{
		PrintHr("FveLibUnlockWithRecoveryPasswordByPath", hr);
		return 2;
	}

	printf("Volume unlocked with recovery password.\n");
	return 0;
}

static int RunLock(const char* volumeArg, BOOL dismountFirst)
{
	PWSTR volume = NULL;
	HRESULT hr;

	hr = MbsToWide(volumeArg, &volume);
	if (FAILED(hr))
	{
		PrintHr("MbsToWide(volume)", hr);
		return 2;
	}

	hr = FveLibLockVolumeByPath(volume, dismountFirst);
	FreeWide(volume);
	if (FAILED(hr))
	{
		PrintHr("FveLibLockVolumeByPath", hr);
		return 2;
	}

	printf("Volume locked.\n");
	return 0;
}

static int RunDecrypt(const char* volumeArg, const char* flagsArg)
{
	PWSTR volume = NULL;
	HRESULT hr;
	DWORD flags = 0;

	hr = MbsToWide(volumeArg, &volume);
	if (FAILED(hr))
	{
		PrintHr("MbsToWide(volume)", hr);
		return 2;
	}

	if (flagsArg != NULL)
	{
		if (!ParseDword(flagsArg, &flags))
		{
			FreeWide(volume);
			printf("Invalid flags: %s\n", flagsArg);
			return 2;
		}
		hr = FveLibStartDecryptionExByPath(volume, flags);
	} else {
		hr = FveLibStartDecryptionByPath(volume);
	}

	FreeWide(volume);
	if (FAILED(hr))
	{
		PrintHr("FveLibStartDecryptionByPath", hr);
		return 2;
	}

	printf("BitLocker decryption started.\n");
	return 0;
}

static int RunCommand(int argc, char* argv[])
{
	const char* command;

	if (argc < 2 || _stricmp(argv[1], "help") == 0 ||
		_stricmp(argv[1], "-h") == 0 || _stricmp(argv[1], "--help") == 0 ||
		_stricmp(argv[1], "/?") == 0) {
		PrintUsage();
		return argc < 2 ? 1 : 0;
	}

	if (!InitFveLib())
	{
		return 2;
	}

	command = argv[1];
	if (_stricmp(command, "status") == 0)
	{
		if (argc != 3)
		{
			PrintUsage();
			return 1;
		}
		return RunStatus(argv[2]);
	}
	if (_stricmp(command, "unlock-password") == 0)
	{
		if (argc != 4) {
			PrintUsage();
			return 1;
		}
		return RunUnlockPassword(argv[2], argv[3]);
	}
	if (_stricmp(command, "unlock-recovery") == 0)
	{
		if (argc != 4)
		{
			PrintUsage();
			return 1;
		}
		return RunUnlockRecovery(argv[2], argv[3]);
	}
	if (_stricmp(command, "lock") == 0)
	{
		BOOL dismountFirst = FALSE;
		if (argc != 3 && argc != 4)
		{
			PrintUsage();
			return 1;
		}
		if (argc == 4)
		{
			if (_stricmp(argv[3], "--dismount") != 0)
			{
				printf("Unknown lock option: %s\n", argv[3]);
				return 1;
			}
			dismountFirst = TRUE;
		}
		return RunLock(argv[2], dismountFirst);
	}
	if (_stricmp(command, "decrypt") == 0 || _stricmp(command, "off") == 0)
	{
		if (argc != 3 && argc != 4)
		{
			PrintUsage();
			return 1;
		}
		return RunDecrypt(argv[2], argc == 4 ? argv[3] : NULL);
	}

	printf("Unknown command: %s\n", command);
	PrintUsage();
	return 1;
}

int main(int argc, char* argv[])
{
	int result = RunCommand(argc, argv);
	FveLibFini();
	return result;
}
