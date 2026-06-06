/*
 *  FVEUnlocker  --  FVE API Bitlocker Unlock Tool
 *  Copyright (C) 2026  A1ive.
 *
 *  FVEUnlocker is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  FVEUnlocker is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with FVEUnlocker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fvelib.h"

#include <string.h>
#include <wctype.h>

#define FVE_LIB_STATUS_OUTPUT_SIZE 0x80u
#define FVE_LIB_STATUS_OUTPUT_VERSION 2u
#define FVE_LIB_STATUS_OUTPUT_LEGACY_VERSION 1u
#define FVE_LIB_AUTH_ELEMENT_SIZE 584u
#define FVE_LIB_AUTH_MAGIC_PASSPHRASE 578
#define FVE_LIB_AUTH_MAGIC_RECOVERY_PASSWORD 32
#define FVE_LIB_SECRET_TYPE_PASSPHRASE 0x00800000u
#define FVE_LIB_SECRET_TYPE_RECOVERY_PASSWORD 0x00080000u
#define FVE_LIB_UNLOCK_SETTINGS_SIZE 56u
#define FVE_LIB_UNLOCK_SETTINGS_VERSION 1u
#define FVE_LIB_HRESULT_NOT_SUPPORTED ((HRESULT)0x80310001u)
#define FVE_LIB_HRESULT_NOT_ENCRYPTED ((HRESULT)0x80310008u)
#define FVE_LIB_HRESULT_NOT_BITLOCKER_VOLUME ((HRESULT)0x80310049u)

typedef struct FVE_GET_STATUS_OUTPUT
{
	DWORD Size;
	DWORD Version;
	DWORD Reserved1;
	DWORD ConversionStatus;
	UINT64 PercentComplete;
	BYTE Reserved2[0x20];
	DWORD ProtectionStatus;
	BYTE Reserved3[0x44];
} FVE_GET_STATUS_OUTPUT;

_Static_assert(sizeof(FVE_GET_STATUS_OUTPUT) == FVE_LIB_STATUS_OUTPUT_SIZE, "FVE_GET_STATUS_OUTPUT must be 0x80 bytes");

typedef struct FVE_AUTH_ELEMENT
{
	LONG MagicValue;
	LONG MustBeOne;
	BYTE Data[FVE_LIB_AUTH_ELEMENT_SIZE - (2u * sizeof(LONG))];
} FVE_AUTH_ELEMENT;

_Static_assert(sizeof(FVE_AUTH_ELEMENT) == FVE_LIB_AUTH_ELEMENT_SIZE, "FVE_AUTH_ELEMENT must be 584 bytes");

typedef struct FVE_UNLOCK_SETTINGS
{
	DWORD Size;
	DWORD Version;
	DWORD SecretType;
	DWORD AuthElementCount;
	FVE_AUTH_ELEMENT** AuthElements;
	PVOID Reserved;
} FVE_UNLOCK_SETTINGS;

typedef HRESULT(WINAPI* PFN_FveOpenVolumeW)(LPCWSTR volumePath, DWORD accessMode, HANDLE* volumeHandle);
typedef HRESULT(WINAPI* PFN_FveCloseVolume)(HANDLE volumeHandle);
typedef HRESULT(WINAPI* PFN_FveCloseVolumeForUnlock)(HANDLE volumeHandle, FVE_UNLOCK_SETTINGS* unlockSettings, DWORD flags, DWORD secretType);
typedef HRESULT(WINAPI* PFN_FveGetStatusW)(LPCWSTR volumePath, FVE_GET_STATUS_OUTPUT* statusInfo);
typedef HRESULT(WINAPI* PFN_FveGetStatus)(HANDLE volumeHandle, FVE_GET_STATUS_OUTPUT* statusInfo);
typedef HRESULT(WINAPI* PFN_FveUnlockVolume)(HANDLE volumeHandle, PVOID authElement);
typedef HRESULT(WINAPI* PFN_FveUnlockVolumeWithAccessMode)(HANDLE volumeHandle, FVE_UNLOCK_SETTINGS* unlockSettings, DWORD flags);
typedef HRESULT(WINAPI* PFN_FveLockVolume)(HANDLE volumeHandle, DWORD dismountFirst);
typedef HRESULT(WINAPI* PFN_FveConversionDecrypt)(HANDLE volumeHandle);
typedef HRESULT(WINAPI* PFN_FveConversionDecryptEx)(HANDLE volumeHandle, DWORD flags);
typedef HRESULT(WINAPI* PFN_FveConversionEncrypt)(HANDLE volumeHandle);
typedef HRESULT(WINAPI* PFN_FveConversionEncryptEx)(HANDLE volumeHandle, DWORD flags);
typedef HRESULT(WINAPI* PFN_FveAuthElementFromPassPhraseW)(LPCWSTR passphrase, FVE_AUTH_ELEMENT* authElement);
typedef HRESULT(WINAPI* PFN_FveAuthElementFromRecoveryPasswordW)(LPCWSTR recoveryPassword, FVE_AUTH_ELEMENT* authElement);
typedef HRESULT(WINAPI* PFN_InternalFveIsVolumeEncrypted)(HANDLE volumeHandle);

typedef struct FVE_API
{
	HMODULE ApiDll;
	PFN_FveOpenVolumeW OpenVolumeW;
	PFN_FveCloseVolume CloseVolume;
	PFN_FveCloseVolumeForUnlock CloseVolumeForUnlock;
	PFN_FveGetStatusW GetStatusW;
	PFN_FveGetStatus GetStatus;
	PFN_FveUnlockVolume UnlockVolume;
	PFN_FveUnlockVolumeWithAccessMode UnlockVolumeWithAccessMode;
	PFN_FveLockVolume LockVolume;
	PFN_FveConversionDecrypt ConversionDecrypt;
	PFN_FveConversionDecryptEx ConversionDecryptEx;
	PFN_FveConversionEncrypt ConversionEncrypt;
	PFN_FveConversionEncryptEx ConversionEncryptEx;
	PFN_FveAuthElementFromPassPhraseW AuthElementFromPassPhraseW;
	PFN_FveAuthElementFromRecoveryPasswordW AuthElementFromRecoveryPasswordW;
	PFN_InternalFveIsVolumeEncrypted InternalFveIsVolumeEncrypted;
} FVE_API;

static FVE_API gFve;

static HRESULT CopyTrimmed(PCWSTR input, PWSTR output, size_t cchOutput)
{
	PCWSTR start = input;
	PCWSTR end;
	size_t length;

	if (input == NULL || output == NULL || cchOutput == 0)
		return E_INVALIDARG;

	while (*start != L'\0' && iswspace(*start))
		++start;

	end = start;
	while (*end != L'\0')
		++end;
	while (end > start && iswspace(*(end - 1)))
		--end;

	length = (size_t)(end - start);
	if (length + 1 > cchOutput)
		return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

	for (size_t i = 0; i < length; ++i)
		output[i] = start[i];
	output[length] = L'\0';
	return S_OK;
}

static HRESULT SetDrivePath(WCHAR letter, PWSTR output, size_t cchOutput)
{
	if (output == NULL || cchOutput < 3)
		return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

	output[0] = towupper(letter);
	output[1] = L':';
	output[2] = L'\0';
	return S_OK;
}

static BOOL IsDrivePath(PCWSTR path)
{
	return path != NULL && iswalpha(path[0]) && path[1] == L':' && path[2] == L'\0';
}

static BOOL IsRawGuidString(PCWSTR text)
{
	static const size_t guidLength = 36;

	if (text == NULL)
		return FALSE;

	for (size_t i = 0; i < guidLength; ++i)
	{
		if (i == 8 || i == 13 || i == 18 || i == 23)
		{
			if (text[i] != L'-')
				return FALSE;
		} else if (!iswxdigit(text[i])) {
			return FALSE;
		}
	}

	return text[guidLength] == L'\0';
}

static HRESULT SetVolumeGuidPath(PCWSTR guid, PWSTR output, size_t cchOutput)
{
	static const WCHAR prefix[] = L"\\\\?\\Volume{";
	static const WCHAR suffix[] = L"}\\";
	const size_t prefixLength = ARRAYSIZE(prefix) - 1;
	const size_t suffixLength = ARRAYSIZE(suffix) - 1;
	const size_t guidLength = 36;
	size_t outIndex = 0;

	if (guid == NULL || output == NULL)
		return E_INVALIDARG;
	if (cchOutput < prefixLength + guidLength + suffixLength + 1)
		return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

	for (size_t i = 0; i < prefixLength; ++i)
		output[outIndex++] = prefix[i];
	for (size_t i = 0; i < guidLength; ++i)
		output[outIndex++] = guid[i];
	for (size_t i = 0; i < suffixLength; ++i)
		output[outIndex++] = suffix[i];
	output[outIndex] = L'\0';
	return S_OK;
}

static BOOL TryGetVolumeGuidPathForDrive(PCWSTR drivePath, PWSTR output, size_t cchOutput)
{
	WCHAR mountPoint[4];

	if (!IsDrivePath(drivePath) || output == NULL || cchOutput == 0 || cchOutput > MAXDWORD)
		return FALSE;

	mountPoint[0] = drivePath[0];
	mountPoint[1] = L':';
	mountPoint[2] = L'\\';
	mountPoint[3] = L'\0';

	return GetVolumeNameForVolumeMountPointW(mountPoint, output, (DWORD)cchOutput);
}

static void InitStatusOutput(FVE_GET_STATUS_OUTPUT* output, DWORD version)
{
	ZeroMemory(output, sizeof(*output));
	output->Size = FVE_LIB_STATUS_OUTPUT_SIZE;
	output->Version = version;
}

static FVE_LIB_VOLUME_STATUS VolumeStatusFromRaw(DWORD status)
{
	switch (status)
	{
	case 0:
		return FVE_LIB_VOLUME_FULLY_DECRYPTED;
	case 1:
		return FVE_LIB_VOLUME_FULLY_ENCRYPTED;
	case 2:
		return FVE_LIB_VOLUME_ENCRYPTION_IN_PROGRESS;
	case 3:
		return FVE_LIB_VOLUME_DECRYPTION_IN_PROGRESS;
	case 4:
		return FVE_LIB_VOLUME_ENCRYPTION_PAUSED;
	case 5:
		return FVE_LIB_VOLUME_DECRYPTION_PAUSED;
	default:
		return FVE_LIB_VOLUME_FULLY_DECRYPTED;
	}
}

static FVE_LIB_PROTECTION_STATUS ProtectionStatusFromRaw(DWORD status)
{
	switch (status)
	{
	case 0:
		return FVE_LIB_PROTECTION_OFF;
	case 1:
		return FVE_LIB_PROTECTION_ON;
	default:
		return FVE_LIB_PROTECTION_UNKNOWN;
	}
}

static FVE_LIB_LOCK_STATUS LockStatusFromRaw(DWORD protectionStatus)
{
	return protectionStatus == 1 ? FVE_LIB_LOCK_LOCKED : FVE_LIB_LOCK_UNLOCKED;
}

static void VolumeInfoFromOutput(const FVE_GET_STATUS_OUTPUT* output, FVE_LIB_VOLUME_INFO* volumeInfo)
{
	volumeInfo->VolumeStatus = VolumeStatusFromRaw(output->ConversionStatus);
	volumeInfo->ProtectionStatus = ProtectionStatusFromRaw(output->ProtectionStatus);
	volumeInfo->LockStatus = LockStatusFromRaw(output->ProtectionStatus);
}

static BOOL IsNotEncryptedStatusHr(HRESULT hr)
{
	switch ((DWORD)hr)
	{
	case (DWORD)FVE_LIB_HRESULT_NOT_SUPPORTED:
	case (DWORD)FVE_LIB_HRESULT_NOT_ENCRYPTED:
	case (DWORD)FVE_LIB_HRESULT_NOT_BITLOCKER_VOLUME:
		return TRUE;
	default:
		return FALSE;
	}
}

static void SetLockedVolumeInfo(FVE_LIB_VOLUME_INFO* volumeInfo)
{
	ZeroMemory(volumeInfo, sizeof(*volumeInfo));
	volumeInfo->VolumeStatus = FVE_LIB_VOLUME_FULLY_ENCRYPTED;
	volumeInfo->ProtectionStatus = FVE_LIB_PROTECTION_ON;
	volumeInfo->LockStatus = FVE_LIB_LOCK_LOCKED;
}

static void SetNotEncryptedVolumeInfo(FVE_LIB_VOLUME_INFO* volumeInfo)
{
	ZeroMemory(volumeInfo, sizeof(*volumeInfo));
	volumeInfo->VolumeStatus = FVE_LIB_VOLUME_FULLY_DECRYPTED;
	volumeInfo->ProtectionStatus = FVE_LIB_PROTECTION_OFF;
	volumeInfo->LockStatus = FVE_LIB_LOCK_UNLOCKED;
}

static void SetEncryptedUnlockedVolumeInfo(FVE_LIB_VOLUME_INFO* volumeInfo)
{
	ZeroMemory(volumeInfo, sizeof(*volumeInfo));
	volumeInfo->VolumeStatus = FVE_LIB_VOLUME_FULLY_ENCRYPTED;
	volumeInfo->ProtectionStatus = FVE_LIB_PROTECTION_ON;
	volumeInfo->LockStatus = FVE_LIB_LOCK_UNLOCKED;
}

static HRESULT QueryStatusByPath(PCWSTR volumePath, FVE_LIB_VOLUME_INFO* volumeInfo)
{
	FVE_GET_STATUS_OUTPUT output;
	HRESULT hr;

	InitStatusOutput(&output, FVE_LIB_STATUS_OUTPUT_VERSION);
	hr = gFve.GetStatusW(volumePath, &output);
	if ((DWORD)hr == (DWORD)E_INVALIDARG)
	{
		InitStatusOutput(&output, FVE_LIB_STATUS_OUTPUT_LEGACY_VERSION);
		hr = gFve.GetStatusW(volumePath, &output);
	}
	if (FVE_LIB_HRESULT_IS_VOLUME_LOCKED(hr))
	{
		SetLockedVolumeInfo(volumeInfo);
		return S_OK;
	}
	if (IsNotEncryptedStatusHr(hr))
	{
		SetNotEncryptedVolumeInfo(volumeInfo);
		return S_OK;
	}
	if (FAILED(hr))
		return hr;

	VolumeInfoFromOutput(&output, volumeInfo);
	return S_OK;
}

static HRESULT QueryStatusByHandle(HANDLE volumeHandle, FVE_LIB_VOLUME_INFO* volumeInfo)
{
	FVE_GET_STATUS_OUTPUT output;
	HRESULT hr;

	if (volumeHandle == NULL || volumeHandle == INVALID_HANDLE_VALUE)
		return E_HANDLE;

	InitStatusOutput(&output, FVE_LIB_STATUS_OUTPUT_VERSION);
	hr = gFve.GetStatus(volumeHandle, &output);
	if ((DWORD)hr == (DWORD)E_INVALIDARG)
	{
		InitStatusOutput(&output, FVE_LIB_STATUS_OUTPUT_LEGACY_VERSION);
		hr = gFve.GetStatus(volumeHandle, &output);
	}
	if (FVE_LIB_HRESULT_IS_VOLUME_LOCKED(hr))
	{
		SetLockedVolumeInfo(volumeInfo);
		return S_OK;
	}
	if (IsNotEncryptedStatusHr(hr))
	{
		SetNotEncryptedVolumeInfo(volumeInfo);
		return S_OK;
	}
	if (FAILED(hr))
		return hr;

	VolumeInfoFromOutput(&output, volumeInfo);
	return S_OK;
}

static HRESULT QueryStatusByOpenVolume(PCWSTR volumePath, FVE_LIB_VOLUME_INFO* volumeInfo)
{
	HANDLE volumeHandle = NULL;
	HRESULT hr = FveLibOpenVolume(volumePath, FVE_LIB_ACCESS_READ_ONLY, &volumeHandle);
	HRESULT closeHr;
	HRESULT encryptedHr;

	if (IsNotEncryptedStatusHr(hr))
	{
		SetNotEncryptedVolumeInfo(volumeInfo);
		return S_OK;
	}
	if (FVE_LIB_FAILED(hr))
		return hr;
	if (volumeHandle == NULL || volumeHandle == INVALID_HANDLE_VALUE)
		return E_HANDLE;

	hr = QueryStatusByHandle(volumeHandle, volumeInfo);
	if (FAILED(hr) && gFve.InternalFveIsVolumeEncrypted != NULL)
	{
		encryptedHr = gFve.InternalFveIsVolumeEncrypted(volumeHandle);
		if (encryptedHr == S_FALSE || IsNotEncryptedStatusHr(encryptedHr))
		{
			SetNotEncryptedVolumeInfo(volumeInfo);
			hr = S_OK;
		} else if (encryptedHr == S_OK) {
			SetEncryptedUnlockedVolumeInfo(volumeInfo);
			hr = S_OK;
		}
	}
	closeHr = FveLibCloseVolume(volumeHandle);
	if (FVE_LIB_FAILED(hr))
		return hr;
	return closeHr;
}

static void InitAuthElement(FVE_AUTH_ELEMENT* authElement, DWORD secretType)
{
	ZeroMemory(authElement, sizeof(*authElement));
	authElement->MagicValue = secretType == FVE_LIB_SECRET_TYPE_RECOVERY_PASSWORD ?
		FVE_LIB_AUTH_MAGIC_RECOVERY_PASSWORD : FVE_LIB_AUTH_MAGIC_PASSPHRASE;
	authElement->MustBeOne = 1;
}

static void InitUnlockSettings(FVE_UNLOCK_SETTINGS* unlockSettings, DWORD secretType, FVE_AUTH_ELEMENT** authElements)
{
	ZeroMemory(unlockSettings, sizeof(*unlockSettings));
	unlockSettings->Size = FVE_LIB_UNLOCK_SETTINGS_SIZE;
	unlockSettings->Version = FVE_LIB_UNLOCK_SETTINGS_VERSION;
	unlockSettings->SecretType = secretType;
	unlockSettings->AuthElementCount = 1;
	unlockSettings->AuthElements = authElements;
	unlockSettings->Reserved = NULL;
}

static HRESULT CreatePassphraseAuth(PCWSTR password, FVE_AUTH_ELEMENT* authElement)
{
	HRESULT hr;

	if (password == NULL || authElement == NULL)
		return E_INVALIDARG;

	InitAuthElement(authElement, FVE_LIB_SECRET_TYPE_PASSPHRASE);
	hr = gFve.AuthElementFromPassPhraseW(password, authElement);
	if (FAILED(hr))
		return hr;

	return S_OK;
}

static HRESULT CreateRecoveryAuth(PCWSTR recoveryPassword, FVE_AUTH_ELEMENT* authElement)
{
	WCHAR formatted[64];
	PCWSTR passwordToUse = recoveryPassword;
	HRESULT hr;

	if (recoveryPassword == NULL || authElement == NULL)
		return E_INVALIDARG;

	hr = FveLibFormatRecoveryPassword(recoveryPassword, formatted, ARRAYSIZE(formatted));
	if (SUCCEEDED(hr))
		passwordToUse = formatted;

	InitAuthElement(authElement, FVE_LIB_SECRET_TYPE_RECOVERY_PASSWORD);
	hr = gFve.AuthElementFromRecoveryPasswordW(passwordToUse, authElement);
	if (FAILED(hr))
		return hr;

	return S_OK;
}

static HRESULT CreateAuthElement(PCWSTR secret, DWORD secretType, FVE_AUTH_ELEMENT* authElement)
{
	if (secretType == FVE_LIB_SECRET_TYPE_RECOVERY_PASSWORD)
		return CreateRecoveryAuth(secret, authElement);
	return CreatePassphraseAuth(secret, authElement);
}

static HRESULT UnlockWithSecret(HANDLE volumeHandle, PCWSTR secret, DWORD secretType, BOOL closeAfterUnlock, BOOL* closed)
{
	FVE_AUTH_ELEMENT authElement;
	FVE_AUTH_ELEMENT* authElementPointer = &authElement;
	FVE_UNLOCK_SETTINGS unlockSettings;
	HRESULT hr;

	if (closed != NULL)
		*closed = FALSE;
	if (volumeHandle == NULL || secret == NULL || secret[0] == L'\0')
		return E_INVALIDARG;

	hr = CreateAuthElement(secret, secretType, &authElement);
	if (FAILED(hr))
		return hr;

	if (gFve.UnlockVolumeWithAccessMode != NULL)
	{
		InitUnlockSettings(&unlockSettings, secretType, &authElementPointer);
		hr = gFve.UnlockVolumeWithAccessMode(volumeHandle, &unlockSettings, 0);
		if (FAILED(hr))
			return hr;

		if (closeAfterUnlock)
		{
			hr = gFve.CloseVolumeForUnlock(volumeHandle, &unlockSettings, 0, secretType);
			if (SUCCEEDED(hr) && closed != NULL)
				*closed = TRUE;
			return hr;
		}

		return S_OK;
	}

	return gFve.UnlockVolume(volumeHandle, &authElement);
}

static HRESULT OpenUnlockClose(PCWSTR volumePath, PCWSTR secret, DWORD secretType)
{
	HANDLE volumeHandle = NULL;
	BOOL closed = FALSE;
	HRESULT hr = FveLibOpenVolume(volumePath, FVE_LIB_ACCESS_READ_ONLY, &volumeHandle);
	HRESULT closeHr;

	if (FVE_LIB_FAILED(hr))
		return hr;
	if (volumeHandle == NULL || volumeHandle == INVALID_HANDLE_VALUE)
		return E_HANDLE;

	hr = UnlockWithSecret(volumeHandle, secret, secretType, TRUE, &closed);
	if (closed)
		return hr;

	closeHr = FveLibCloseVolume(volumeHandle);
	if (FVE_LIB_FAILED(hr))
		return hr;
	return closeHr;
}

static HRESULT OpenRunClose(PCWSTR volumePath, FVE_LIB_ACCESS_MODE accessMode, HRESULT(WINAPI* callback)(HANDLE, void*), void* context)
{
	HANDLE volumeHandle = NULL;
	HRESULT hr = FveLibOpenVolume(volumePath, accessMode, &volumeHandle);
	HRESULT closeHr;

	if (FVE_LIB_FAILED(hr))
		return hr;
	if (volumeHandle == NULL || volumeHandle == INVALID_HANDLE_VALUE)
		return E_HANDLE;

	hr = callback(volumeHandle, context);
	closeHr = FveLibCloseVolume(volumeHandle);
	if (FVE_LIB_FAILED(hr))
		return hr;
	return closeHr;
}

static HRESULT WINAPI LockCallback(HANDLE volumeHandle, void* context)
{
	BOOL dismountFirst = context != NULL ? TRUE : FALSE;
	return FveLibLockVolume(volumeHandle, dismountFirst);
}

static HRESULT WINAPI DecryptCallback(HANDLE volumeHandle, void* context)
{
	DWORD* flags = (DWORD*)context;

	if (flags != NULL)
		return FveLibStartDecryptionEx(volumeHandle, *flags);
	return FveLibStartDecryption(volumeHandle);
}

static HRESULT WINAPI EncryptCallback(HANDLE volumeHandle, void* context)
{
	DWORD* flags = (DWORD*)context;

	if (flags != NULL)
		return FveLibStartEncryptionEx(volumeHandle, *flags);
	return FveLibStartEncryption(volumeHandle);
}

HRESULT FveLibInit(void)
{
	HRESULT hr = HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
	FARPROC closeProc;

	if (gFve.ApiDll != NULL)
		return S_OK;

	memset(&gFve, 0, sizeof(gFve));
	gFve.ApiDll = LoadLibraryW(L"fveapi.dll");
	if (gFve.ApiDll == NULL)
		goto fail;

	gFve.OpenVolumeW = (PFN_FveOpenVolumeW)GetProcAddress(gFve.ApiDll, "FveOpenVolumeW");
	if (gFve.OpenVolumeW == NULL)
		goto fail;

	closeProc = GetProcAddress(gFve.ApiDll, "FveCloseVolume");
	if (closeProc == NULL)
		goto fail;

	gFve.CloseVolume = (PFN_FveCloseVolume)closeProc;
	gFve.CloseVolumeForUnlock = (PFN_FveCloseVolumeForUnlock)closeProc;
	gFve.GetStatusW = (PFN_FveGetStatusW)GetProcAddress(gFve.ApiDll, "FveGetStatusW");
	if (gFve.GetStatusW == NULL)
		goto fail;

	gFve.GetStatus = (PFN_FveGetStatus)GetProcAddress(gFve.ApiDll, "FveGetStatus");
	if (gFve.GetStatus == NULL)
		goto fail;

	gFve.UnlockVolume = (PFN_FveUnlockVolume)GetProcAddress(gFve.ApiDll, "FveUnlockVolume");
	if (gFve.UnlockVolume == NULL)
		goto fail;

	gFve.LockVolume = (PFN_FveLockVolume)GetProcAddress(gFve.ApiDll, "FveLockVolume");
	if (gFve.LockVolume == NULL)
		goto fail;

	gFve.ConversionDecrypt = (PFN_FveConversionDecrypt)GetProcAddress(gFve.ApiDll, "FveConversionDecrypt");
	if (gFve.ConversionDecrypt == NULL)
		goto fail;

	gFve.ConversionDecryptEx = (PFN_FveConversionDecryptEx)GetProcAddress(gFve.ApiDll, "FveConversionDecryptEx");
	if (gFve.ConversionDecryptEx == NULL)
		goto fail;

	gFve.AuthElementFromPassPhraseW = (PFN_FveAuthElementFromPassPhraseW)GetProcAddress(gFve.ApiDll, "FveAuthElementFromPassPhraseW");
	if (gFve.AuthElementFromPassPhraseW == NULL)
		goto fail;

	gFve.AuthElementFromRecoveryPasswordW = (PFN_FveAuthElementFromRecoveryPasswordW)GetProcAddress(gFve.ApiDll, "FveAuthElementFromRecoveryPasswordW");
	if (gFve.AuthElementFromRecoveryPasswordW == NULL)
		goto fail;

	gFve.UnlockVolumeWithAccessMode = (PFN_FveUnlockVolumeWithAccessMode)GetProcAddress(gFve.ApiDll, "FveUnlockVolumeWithAccessMode");
	gFve.ConversionEncrypt = (PFN_FveConversionEncrypt)GetProcAddress(gFve.ApiDll, "FveConversionEncrypt");
	gFve.ConversionEncryptEx = (PFN_FveConversionEncryptEx)GetProcAddress(gFve.ApiDll, "FveConversionEncryptEx");
	gFve.InternalFveIsVolumeEncrypted = (PFN_InternalFveIsVolumeEncrypted)GetProcAddress(gFve.ApiDll, "InternalFveIsVolumeEncrypted");

	return S_OK;
fail:
	FveLibFini();
	return hr;
}

void FveLibFini(void)
{
	HMODULE dll = gFve.ApiDll;

	memset(&gFve, 0, sizeof(gFve));

	if (dll != NULL)
		FreeLibrary(dll);
}

HRESULT FveLibNormalizeVolumePath(PCWSTR volumePath, PWSTR normalizedPath, size_t cchNormalizedPath)
{
	WCHAR trimmed[MAX_PATH];
	HRESULT hr;

	hr = CopyTrimmed(volumePath, trimmed, ARRAYSIZE(trimmed));
	if (FAILED(hr))
		return hr;

	if (trimmed[0] == L'\0')
		return E_INVALIDARG;

	if (wcsstr(trimmed, L"Volume{") != NULL)
		return CopyTrimmed(trimmed, normalizedPath, cchNormalizedPath);

	if (IsRawGuidString(trimmed))
		return SetVolumeGuidPath(trimmed, normalizedPath, cchNormalizedPath);

	if (trimmed[0] == L'\\' && trimmed[1] == L'\\' &&
		(trimmed[2] == L'.' || trimmed[2] == L'?') && trimmed[3] == L'\\' &&
		iswalpha(trimmed[4]) && trimmed[5] == L':')
		return SetDrivePath(trimmed[4], normalizedPath, cchNormalizedPath);

	if (iswalpha(trimmed[0]))
	{
		if (trimmed[1] == L':' || trimmed[1] == L'\0')
			return SetDrivePath(trimmed[0], normalizedPath, cchNormalizedPath);
	}

	return CopyTrimmed(trimmed, normalizedPath, cchNormalizedPath);
}

HRESULT FveLibGetStatusByPath(PCWSTR volumePath, FVE_LIB_VOLUME_INFO* volumeInfo)
{
	WCHAR normalized[MAX_PATH];
	WCHAR volumeGuidPath[MAX_PATH];
	HRESULT hr;
	HRESULT firstHr;

	if (volumeInfo == NULL)
		return E_INVALIDARG;

	hr = FveLibNormalizeVolumePath(volumePath, normalized, ARRAYSIZE(normalized));
	if (FAILED(hr))
		return hr;

	hr = QueryStatusByPath(normalized, volumeInfo);
	if (SUCCEEDED(hr))
		return S_OK;
	firstHr = hr;

	if (TryGetVolumeGuidPathForDrive(normalized, volumeGuidPath, ARRAYSIZE(volumeGuidPath)))
	{
		hr = QueryStatusByPath(volumeGuidPath, volumeInfo);
		if (SUCCEEDED(hr))
			return S_OK;
	}

	hr = QueryStatusByOpenVolume(normalized, volumeInfo);
	if (SUCCEEDED(hr))
		return S_OK;

	return firstHr;
}

HRESULT FveLibOpenVolume(PCWSTR volumePath, FVE_LIB_ACCESS_MODE accessMode, HANDLE* volumeHandle)
{
	WCHAR normalized[MAX_PATH];
	WCHAR volumeGuidPath[MAX_PATH];
	HRESULT hr;

	if (volumeHandle == NULL)
		return E_INVALIDARG;
	*volumeHandle = NULL;

	hr = FveLibNormalizeVolumePath(volumePath, normalized, ARRAYSIZE(normalized));
	if (FAILED(hr))
		return hr;

	if (TryGetVolumeGuidPathForDrive(normalized, volumeGuidPath, ARRAYSIZE(volumeGuidPath)))
		return gFve.OpenVolumeW(volumeGuidPath, (DWORD)accessMode, volumeHandle);

	return gFve.OpenVolumeW(normalized, (DWORD)accessMode, volumeHandle);
}

HRESULT FveLibCloseVolume(HANDLE volumeHandle)
{
	if (volumeHandle == NULL || volumeHandle == INVALID_HANDLE_VALUE)
		return S_OK;

	return gFve.CloseVolume(volumeHandle);
}

HRESULT FveLibUnlockWithPassword(HANDLE volumeHandle, PCWSTR password)
{
	if (volumeHandle == NULL || password == NULL || password[0] == L'\0')
		return E_INVALIDARG;

	return UnlockWithSecret(volumeHandle, password, FVE_LIB_SECRET_TYPE_PASSPHRASE, FALSE, NULL);
}

HRESULT FveLibUnlockWithRecoveryPassword(HANDLE volumeHandle, PCWSTR recoveryPassword)
{
	if (volumeHandle == NULL || recoveryPassword == NULL || recoveryPassword[0] == L'\0')
		return E_INVALIDARG;

	return UnlockWithSecret(volumeHandle, recoveryPassword, FVE_LIB_SECRET_TYPE_RECOVERY_PASSWORD, FALSE, NULL);
}

HRESULT FveLibUnlockWithPasswordByPath(PCWSTR volumePath, PCWSTR password)
{
	return OpenUnlockClose(volumePath, password, FVE_LIB_SECRET_TYPE_PASSPHRASE);
}

HRESULT FveLibUnlockWithRecoveryPasswordByPath(PCWSTR volumePath, PCWSTR recoveryPassword)
{
	return OpenUnlockClose(volumePath, recoveryPassword, FVE_LIB_SECRET_TYPE_RECOVERY_PASSWORD);
}

HRESULT FveLibLockVolume(HANDLE volumeHandle, BOOL dismountFirst)
{
	if (volumeHandle == NULL)
		return E_INVALIDARG;

	return gFve.LockVolume(volumeHandle, dismountFirst ? 1u : 0u);
}

HRESULT FveLibLockVolumeByPath(PCWSTR volumePath, BOOL dismountFirst)
{
	return OpenRunClose(volumePath, FVE_LIB_ACCESS_READ_WRITE, LockCallback, dismountFirst ? (void*)1 : NULL);
}

HRESULT FveLibStartDecryption(HANDLE volumeHandle)
{
	if (volumeHandle == NULL)
		return E_INVALIDARG;

	return gFve.ConversionDecrypt(volumeHandle);
}

HRESULT FveLibStartDecryptionEx(HANDLE volumeHandle, DWORD flags)
{
	if (volumeHandle == NULL)
		return E_INVALIDARG;

	return gFve.ConversionDecryptEx(volumeHandle, flags);
}

HRESULT FveLibStartDecryptionByPath(PCWSTR volumePath)
{
	return OpenRunClose(volumePath, FVE_LIB_ACCESS_READ_WRITE, DecryptCallback, NULL);
}

HRESULT FveLibStartDecryptionExByPath(PCWSTR volumePath, DWORD flags)
{
	return OpenRunClose(volumePath, FVE_LIB_ACCESS_READ_WRITE, DecryptCallback, &flags);
}

HRESULT FveLibStartEncryption(HANDLE volumeHandle)
{
	if (volumeHandle == NULL)
		return E_INVALIDARG;

	if (gFve.ConversionEncrypt == NULL)
		return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

	return gFve.ConversionEncrypt(volumeHandle);
}

HRESULT FveLibStartEncryptionEx(HANDLE volumeHandle, DWORD flags)
{
	if (volumeHandle == NULL)
		return E_INVALIDARG;

	if (gFve.ConversionEncryptEx == NULL)
		return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

	return gFve.ConversionEncryptEx(volumeHandle, flags);
}

HRESULT FveLibStartEncryptionByPath(PCWSTR volumePath)
{
	return OpenRunClose(volumePath, FVE_LIB_ACCESS_READ_WRITE, EncryptCallback, NULL);
}

HRESULT FveLibStartEncryptionExByPath(PCWSTR volumePath, DWORD flags)
{
	return OpenRunClose(volumePath, FVE_LIB_ACCESS_READ_WRITE, EncryptCallback, &flags);
}

HRESULT FveLibFormatRecoveryPassword(PCWSTR input, PWSTR output, size_t cchOutput)
{
	WCHAR digits[49];
	size_t digitCount = 0;

	if (input == NULL || output == NULL || cchOutput < 56)
		return E_INVALIDARG;

	for (size_t i = 0; input[i] != L'\0'; ++i)
	{
		if (input[i] >= L'0' && input[i] <= L'9')
		{
			if (digitCount >= 48)
				return E_INVALIDARG;
			digits[digitCount++] = input[i];
		}
	}
	if (digitCount != 48)
		return E_INVALIDARG;
	digits[48] = L'\0';

	for (size_t i = 0, j = 0; i < 48; ++i)
	{
		if (i > 0 && (i % 6) == 0)
			output[j++] = L'-';
		output[j++] = digits[i];
		if (i == 47)
			output[j] = L'\0';
	}

	return S_OK;
}

const char* FveLibErrorName(HRESULT hr)
{
	switch ((DWORD)hr)
	{
	case 0x00000000u:
		return "Success";
	case 0x80070005u:
		return "AccessDenied";
	case 0x80070057u:
		return "InvalidParameter";
	case 0x8007007Fu:
		return "ProcNotFound";
	case 0x80310000u:
		return "VolumeLocked";
	case 0x80310001u:
		return "NotSupported";
	case 0x80310008u:
		return "NotEncrypted";
	case 0x8031000Du:
		return "AuthenticationFailed";
	case 0x80310023u:
		return "VolumeUnlocked";
	case 0x80310027u:
		return "BadPassword";
	case 0x80310028u:
		return "BadRecoveryPassword";
	case 0x80310044u:
		return "KeyRequired";
	case 0x80310049u:
		return "NotBitLockerVolume";
	case 0x8031004Au:
		return "VolumeRemoved";
	default:
		return "Unknown";
	}
}

const char* FveLibVolumeStatusName(FVE_LIB_VOLUME_STATUS status)
{
	switch (status)
	{
	case FVE_LIB_VOLUME_FULLY_DECRYPTED:
		return "FullyDecrypted";
	case FVE_LIB_VOLUME_FULLY_ENCRYPTED:
		return "FullyEncrypted";
	case FVE_LIB_VOLUME_ENCRYPTION_IN_PROGRESS:
		return "EncryptionInProgress";
	case FVE_LIB_VOLUME_DECRYPTION_IN_PROGRESS:
		return "DecryptionInProgress";
	case FVE_LIB_VOLUME_ENCRYPTION_PAUSED:
		return "EncryptionPaused";
	case FVE_LIB_VOLUME_DECRYPTION_PAUSED:
		return "DecryptionPaused";
	default:
		return "Unknown";
	}
}

const char* FveLibProtectionStatusName(FVE_LIB_PROTECTION_STATUS status)
{
	switch (status)
	{
	case FVE_LIB_PROTECTION_OFF:
		return "Off";
	case FVE_LIB_PROTECTION_ON:
		return "On";
	case FVE_LIB_PROTECTION_UNKNOWN:
		return "Unknown";
	default:
		return "Unknown";
	}
}

const char* FveLibLockStatusName(FVE_LIB_LOCK_STATUS status)
{
	switch (status)
	{
	case FVE_LIB_LOCK_UNLOCKED:
		return "Unlocked";
	case FVE_LIB_LOCK_LOCKED:
		return "Locked";
	default:
		return "Unknown";
	}
}
