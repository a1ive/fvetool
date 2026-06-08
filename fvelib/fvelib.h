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

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stddef.h>

#define FVE_LIB_HRESULT_VOLUME_LOCKED ((HRESULT)0x80310000u)
#define FVE_LIB_HRESULT_IS_VOLUME_LOCKED(hr) ((DWORD)(HRESULT)(hr) == (DWORD)FVE_LIB_HRESULT_VOLUME_LOCKED)
#define FVE_LIB_FAILED(hr) (FAILED(hr) && !FVE_LIB_HRESULT_IS_VOLUME_LOCKED(hr))
#define FVE_LIB_RECOVERY_PASSWORD_CCH 56u
#define FVE_LIB_MAX_KEY_PROTECTOR_ELEMENTS 16u

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FVE_LIB_KEY_PROTECTOR_TYPE
{
	FVE_LIB_KEY_PROTECTOR_UNKNOWN = 0,
	FVE_LIB_KEY_PROTECTOR_RECOVERY_PASSWORD = 1,
	FVE_LIB_KEY_PROTECTOR_PIN = 2,
	FVE_LIB_KEY_PROTECTOR_TPM = 3,
	FVE_LIB_KEY_PROTECTOR_EXTERNAL_KEY = 4,
	FVE_LIB_KEY_PROTECTOR_PASSPHRASE = 8,
	FVE_LIB_KEY_PROTECTOR_CLEAR_KEY = 9,
	FVE_LIB_KEY_PROTECTOR_DPAPI_NG = 10,
	FVE_LIB_KEY_PROTECTOR_NETWORK = 11
} FVE_LIB_KEY_PROTECTOR_TYPE;

typedef struct FVE_LIB_KEY_PROTECTOR_INFO
{
	GUID ProtectorId;
	DWORD ElementCount;
	FVE_LIB_KEY_PROTECTOR_TYPE ElementTypes[FVE_LIB_MAX_KEY_PROTECTOR_ELEMENTS];
	WCHAR RecoveryPassword[FVE_LIB_RECOVERY_PASSWORD_CCH];
	BOOL HasRecoveryPassword;
} FVE_LIB_KEY_PROTECTOR_INFO;

typedef struct FVE_LIB_KEY_PROTECTORS
{
	DWORD Count;
	FVE_LIB_KEY_PROTECTOR_INFO* Protectors;
} FVE_LIB_KEY_PROTECTORS;

typedef enum FVE_LIB_ACCESS_MODE
{
	FVE_LIB_ACCESS_READ_ONLY = 0,
	FVE_LIB_ACCESS_READ_WRITE = 1
} FVE_LIB_ACCESS_MODE;

typedef enum FVE_LIB_VOLUME_STATUS
{
	FVE_LIB_VOLUME_FULLY_DECRYPTED = 0,
	FVE_LIB_VOLUME_FULLY_ENCRYPTED = 1,
	FVE_LIB_VOLUME_ENCRYPTION_IN_PROGRESS = 2,
	FVE_LIB_VOLUME_DECRYPTION_IN_PROGRESS = 3,
	FVE_LIB_VOLUME_ENCRYPTION_PAUSED = 4,
	FVE_LIB_VOLUME_DECRYPTION_PAUSED = 5
} FVE_LIB_VOLUME_STATUS;

typedef enum FVE_LIB_PROTECTION_STATUS
{
	FVE_LIB_PROTECTION_OFF = 0,
	FVE_LIB_PROTECTION_ON = 1,
	FVE_LIB_PROTECTION_UNKNOWN = 2
} FVE_LIB_PROTECTION_STATUS;

typedef enum FVE_LIB_LOCK_STATUS
{
	FVE_LIB_LOCK_UNLOCKED = 0,
	FVE_LIB_LOCK_LOCKED = 1
} FVE_LIB_LOCK_STATUS;

typedef struct FVE_LIB_VOLUME_INFO
{
	FVE_LIB_VOLUME_STATUS VolumeStatus;
	FVE_LIB_PROTECTION_STATUS ProtectionStatus;
	FVE_LIB_LOCK_STATUS LockStatus;
} FVE_LIB_VOLUME_INFO;

HRESULT FveLibInit(void);
void FveLibFini(void);

HRESULT FveLibNormalizeVolumePath(PCWSTR volumePath, PWSTR normalizedPath, size_t cchNormalizedPath);

HRESULT FveLibGetStatusByPath(PCWSTR volumePath, FVE_LIB_VOLUME_INFO* volumeInfo);

HRESULT FveLibOpenVolume(PCWSTR volumePath, FVE_LIB_ACCESS_MODE accessMode, HANDLE* volumeHandle);
HRESULT FveLibCloseVolume(HANDLE volumeHandle);

HRESULT FveLibUnlockWithPassword(HANDLE volumeHandle, PCWSTR password);
HRESULT FveLibUnlockWithRecoveryPassword(HANDLE volumeHandle, PCWSTR recoveryPassword);
HRESULT FveLibUnlockWithPasswordByPath(PCWSTR volumePath, PCWSTR password);
HRESULT FveLibUnlockWithRecoveryPasswordByPath(PCWSTR volumePath, PCWSTR recoveryPassword);

HRESULT FveLibLockVolume(HANDLE volumeHandle, BOOL dismountFirst);
HRESULT FveLibLockVolumeByPath(PCWSTR volumePath, BOOL dismountFirst);

HRESULT FveLibStartDecryption(HANDLE volumeHandle);
HRESULT FveLibStartDecryptionEx(HANDLE volumeHandle, DWORD flags);
HRESULT FveLibStartDecryptionByPath(PCWSTR volumePath);
HRESULT FveLibStartDecryptionExByPath(PCWSTR volumePath, DWORD flags);

HRESULT FveLibStartEncryption(HANDLE volumeHandle);
HRESULT FveLibStartEncryptionEx(HANDLE volumeHandle, DWORD flags);
HRESULT FveLibStartEncryptionByPath(PCWSTR volumePath);
HRESULT FveLibStartEncryptionExByPath(PCWSTR volumePath, DWORD flags);

HRESULT FveLibInitVolumeForEncryption(HANDLE volumeHandle);
HRESULT FveLibAddPasswordProtector(HANDLE volumeHandle, PCWSTR password, GUID* protectorId);
HRESULT FveLibAddRecoveryPasswordProtector(HANDLE volumeHandle, PCWSTR recoveryPassword, GUID* protectorId);
HRESULT FveLibGenerateRecoveryPassword(PWSTR output, size_t cchOutput);
HRESULT FveLibEncryptWithPassword(HANDLE volumeHandle, PCWSTR password, PWSTR recoveryPassword, size_t cchRecoveryPassword);
HRESULT FveLibEncryptWithPasswordEx(HANDLE volumeHandle, PCWSTR password, DWORD flags, PWSTR recoveryPassword, size_t cchRecoveryPassword);
HRESULT FveLibEncryptWithPasswordByPath(PCWSTR volumePath, PCWSTR password, PWSTR recoveryPassword, size_t cchRecoveryPassword);
HRESULT FveLibEncryptWithPasswordExByPath(PCWSTR volumePath, PCWSTR password, DWORD flags, PWSTR recoveryPassword, size_t cchRecoveryPassword);

HRESULT FveLibFormatRecoveryPassword(PCWSTR input, PWSTR output, size_t cchOutput);

const char* FveLibErrorName(HRESULT hr);
const char* FveLibVolumeStatusName(FVE_LIB_VOLUME_STATUS status);
const char* FveLibProtectionStatusName(FVE_LIB_PROTECTION_STATUS status);
const char* FveLibLockStatusName(FVE_LIB_LOCK_STATUS status);
const char* FveLibKeyProtectorTypeName(FVE_LIB_KEY_PROTECTOR_TYPE type);

HRESULT FveLibGetKeyProtectors(PCWSTR volumePath, FVE_LIB_KEY_PROTECTORS* protectors);
HRESULT FveLibGetKeyProtectorsByHandle(HANDLE volumeHandle, FVE_LIB_KEY_PROTECTORS* protectors);
void FveLibFreeKeyProtectors(FVE_LIB_KEY_PROTECTORS* protectors);

#ifdef __cplusplus
}
#endif

