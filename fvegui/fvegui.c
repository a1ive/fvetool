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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <strsafe.h>

#include "../fvelib/fvelib.h"
#include "resource.h"

#define FVE_GUI_CLASS_NAME L"FveToolNativeGuiWindow"
#define FVE_GUI_MAX_VOLUMES 32
#define FVE_GUI_MAX_SECRET 2048
#define FVE_GUI_TEMP_STRING_BUFFERS 16
#define FVE_GUI_TEMP_STRING_CCH 512

#define IDC_VOLUME_COMBO 1001
#define IDC_REFRESH_BUTTON 1002
#define IDC_STATUS_EDIT 1003
#define IDC_SECRET_EDIT 1004
#define IDC_RECOVERY_CHECK 1005
#define IDC_UNLOCK_BUTTON 1006
#define IDC_LOCK_BUTTON 1007
#define IDC_DECRYPT_BUTTON 1008
#define IDC_MESSAGE_STATIC 1009

typedef struct FVE_GUI_VOLUME_ENTRY
{
	WCHAR Path[4];
	WCHAR RootPath[4];
	WCHAR DisplayName[256];
	WCHAR VolumeLabel[128];
	WCHAR FileSystem[32];
	UINT DriveType;
	FVE_LIB_VOLUME_INFO Info;
	HRESULT StatusHr;
	BOOL HasStatus;
} FVE_GUI_VOLUME_ENTRY;

typedef struct FVE_GUI_TEXT
{
	WCHAR AppTitle[128];
	WCHAR VolumeLabel[64];
	WCHAR RefreshButton[64];
	WCHAR SecretLabel[64];
	WCHAR RecoveryCheck[64];
	WCHAR UnlockButton[64];
	WCHAR LockButton[64];
	WCHAR TurnOffButton[96];
} FVE_GUI_TEXT;

typedef struct FVE_GUI_APP
{
	HINSTANCE Instance;
	HWND MainWindow;
	HWND VolumeLabel;
	HWND VolumeCombo;
	HWND RefreshButton;
	HWND StatusEdit;
	HWND SecretLabel;
	HWND SecretEdit;
	HWND RecoveryCheck;
	HWND UnlockButton;
	HWND LockButton;
	HWND DecryptButton;
	HWND MessageStatic;
	HFONT Font;
	BOOL Busy;
	BOOL Elevated;
	FVE_GUI_TEXT Text;
	FVE_GUI_VOLUME_ENTRY Volumes[FVE_GUI_MAX_VOLUMES];
	int VolumeCount;
} FVE_GUI_APP;

static FVE_GUI_APP gApp;

static void LoadResourceStringOrFallback(UINT stringId, PWSTR output, size_t cchOutput, PCWSTR fallback)
{
	int chars = 0;

	if (output == NULL || cchOutput == 0)
		return;

	if (gApp.Instance != NULL)
		chars = LoadStringW(gApp.Instance, stringId, output, (int)cchOutput);

	if (chars <= 0)
		StringCchCopyW(output, cchOutput, fallback != NULL ? fallback : L"");
}

static PCWSTR LoadTempString(UINT stringId, PCWSTR fallback)
{
	static WCHAR buffers[FVE_GUI_TEMP_STRING_BUFFERS][FVE_GUI_TEMP_STRING_CCH];
	static UINT nextBuffer;
	PWSTR buffer = buffers[nextBuffer % FVE_GUI_TEMP_STRING_BUFFERS];

	++nextBuffer;
	LoadResourceStringOrFallback(stringId, buffer, ARRAYSIZE(buffers[0]), fallback);
	return buffer;
}

static void LoadGuiText(FVE_GUI_TEXT* text)
{
#define LOAD_TEXT(field, stringId, fallback) \
	LoadResourceStringOrFallback((stringId), (text)->field, ARRAYSIZE((text)->field), (fallback))

	LOAD_TEXT(AppTitle, IDS_APP_TITLE, L"BitLocker Native GUI");
	LOAD_TEXT(VolumeLabel, IDS_VOLUME_LABEL, L"Volume:");
	LOAD_TEXT(RefreshButton, IDS_REFRESH_BUTTON, L"Refresh");
	LOAD_TEXT(SecretLabel, IDS_SECRET_LABEL, L"Unlock key:");
	LOAD_TEXT(RecoveryCheck, IDS_RECOVERY_CHECK, L"Recovery key");
	LOAD_TEXT(UnlockButton, IDS_UNLOCK_BUTTON, L"Unlock");
	LOAD_TEXT(LockButton, IDS_LOCK_BUTTON, L"Lock");
	LOAD_TEXT(TurnOffButton, IDS_TURN_OFF_BUTTON, L"Turn off BitLocker");

#undef LOAD_TEXT
}

static int ScaleForWindow(HWND window, int value)
{
	UINT dpi = 96;

	if (window != NULL)
		dpi = GetDpiForWindow(window);

	return MulDiv(value, (int)dpi, 96);
}

static void AsciiToWide(const char* input, PWSTR output, size_t cchOutput)
{
	size_t index = 0;

	if (output == NULL || cchOutput == 0)
		return;

	if (input != NULL)
	{
		while (input[index] != '\0' && index + 1 < cchOutput)
		{
			output[index] = (WCHAR)(unsigned char)input[index];
			++index;
		}
	}

	output[index] = L'\0';
}

static PCWSTR VolumeStatusText(FVE_LIB_VOLUME_STATUS status)
{
	switch (status)
	{
	case FVE_LIB_VOLUME_FULLY_DECRYPTED:
		return LoadTempString(IDS_VOLUME_FULLY_DECRYPTED, L"Fully decrypted");
	case FVE_LIB_VOLUME_FULLY_ENCRYPTED:
		return LoadTempString(IDS_VOLUME_FULLY_ENCRYPTED, L"Fully encrypted");
	case FVE_LIB_VOLUME_ENCRYPTION_IN_PROGRESS:
		return LoadTempString(IDS_VOLUME_ENCRYPTION_IN_PROGRESS, L"Encryption in progress");
	case FVE_LIB_VOLUME_DECRYPTION_IN_PROGRESS:
		return LoadTempString(IDS_VOLUME_DECRYPTION_IN_PROGRESS, L"Decryption in progress");
	case FVE_LIB_VOLUME_ENCRYPTION_PAUSED:
		return LoadTempString(IDS_VOLUME_ENCRYPTION_PAUSED, L"Encryption paused");
	case FVE_LIB_VOLUME_DECRYPTION_PAUSED:
		return LoadTempString(IDS_VOLUME_DECRYPTION_PAUSED, L"Decryption paused");
	default:
		return LoadTempString(IDS_UNKNOWN, L"Unknown");
	}
}

static PCWSTR ProtectionStatusText(FVE_LIB_PROTECTION_STATUS status)
{
	switch (status)
	{
	case FVE_LIB_PROTECTION_OFF:
		return LoadTempString(IDS_PROTECTION_OFF, L"Off");
	case FVE_LIB_PROTECTION_ON:
		return LoadTempString(IDS_PROTECTION_ON, L"On");
	case FVE_LIB_PROTECTION_UNKNOWN:
		return LoadTempString(IDS_UNKNOWN, L"Unknown");
	default:
		return LoadTempString(IDS_UNKNOWN, L"Unknown");
	}
}

static PCWSTR LockStatusText(FVE_LIB_LOCK_STATUS status)
{
	switch (status)
	{
	case FVE_LIB_LOCK_UNLOCKED:
		return LoadTempString(IDS_LOCK_UNLOCKED, L"Unlocked");
	case FVE_LIB_LOCK_LOCKED:
		return LoadTempString(IDS_LOCK_LOCKED, L"Locked");
	default:
		return LoadTempString(IDS_UNKNOWN, L"Unknown");
	}
}

static PCWSTR DriveTypeText(UINT driveType)
{
	switch (driveType)
	{
	case DRIVE_REMOVABLE:
		return LoadTempString(IDS_DRIVE_REMOVABLE, L"Removable");
	case DRIVE_FIXED:
		return LoadTempString(IDS_DRIVE_FIXED, L"Fixed");
	case DRIVE_REMOTE:
		return LoadTempString(IDS_DRIVE_NETWORK, L"Network");
	case DRIVE_CDROM:
		return LoadTempString(IDS_DRIVE_CDROM, L"CD-ROM");
	case DRIVE_RAMDISK:
		return LoadTempString(IDS_DRIVE_RAMDISK, L"RAM disk");
	case DRIVE_NO_ROOT_DIR:
		return LoadTempString(IDS_DRIVE_NO_ROOT, L"No root");
	default:
		return LoadTempString(IDS_UNKNOWN, L"Unknown");
	}
}

static void FormatHRESULT(HRESULT hr, PWSTR output, size_t cchOutput)
{
	WCHAR errorName[64];
	WCHAR systemMessage[512];
	WCHAR format[128];
	DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
	DWORD chars;

	AsciiToWide(FveLibErrorName(hr), errorName, ARRAYSIZE(errorName));
	chars = FormatMessageW(flags, NULL, (DWORD)hr, 0, systemMessage, ARRAYSIZE(systemMessage), NULL);
	if (chars == 0)
	{
		LoadResourceStringOrFallback(IDS_HRESULT_FORMAT, format, ARRAYSIZE(format), L"hr=0x%08lX (%s)");
		StringCchPrintfW(output, cchOutput, format, (unsigned long)(DWORD)hr, errorName);
		return;
	}

	while (chars > 0 && (systemMessage[chars - 1] == L'\r' ||
		systemMessage[chars - 1] == L'\n' || systemMessage[chars - 1] == L' '))
	{
		systemMessage[chars - 1] = L'\0';
		--chars;
	}

	LoadResourceStringOrFallback(IDS_HRESULT_WITH_MESSAGE_FORMAT, format, ARRAYSIZE(format), L"hr=0x%08lX (%s): %s");
	StringCchPrintfW(output, cchOutput, format, (unsigned long)(DWORD)hr, errorName, systemMessage);
}

static void ShowHrError(HWND owner, PCWSTR operation, HRESULT hr)
{
	WCHAR detail[768];
	WCHAR message[1024];
	WCHAR format[128];
	WCHAR title[128];

	FormatHRESULT(hr, detail, ARRAYSIZE(detail));
	LoadResourceStringOrFallback(IDS_OPERATION_FAILED_FORMAT, format, ARRAYSIZE(format), L"%s failed.\r\n\r\n%s");
	LoadResourceStringOrFallback(IDS_OPERATION_FAILED_TITLE, title, ARRAYSIZE(title), L"BitLocker operation failed");
	StringCchPrintfW(message, ARRAYSIZE(message), format, operation, detail);
	MessageBoxW(owner, message, title, MB_OK | MB_ICONERROR);
}

static BOOL IsProcessElevated(void)
{
	HANDLE token = NULL;
	TOKEN_ELEVATION elevation;
	DWORD returned = 0;
	BOOL elevated = FALSE;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
		return FALSE;

	if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returned))
		elevated = elevation.TokenIsElevated != 0;

	CloseHandle(token);
	return elevated;
}

static void SetControlFont(HWND control, HFONT font)
{
	if (control != NULL)
		SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
}

static HWND CreateChildWindow(
	FVE_GUI_APP* app,
	PCWSTR className,
	PCWSTR text,
	DWORD style,
	DWORD exStyle,
	int controlId)
{
	HWND child = CreateWindowExW(
		exStyle,
		className,
		text,
		WS_CHILD | WS_VISIBLE | style,
		0,
		0,
		0,
		0,
		app->MainWindow,
		(HMENU)(INT_PTR)controlId,
		app->Instance,
		NULL);

	SetControlFont(child, app->Font);
	return child;
}

static int GetSelectedVolumeIndex(const FVE_GUI_APP* app)
{
	LRESULT selection;
	LRESULT itemData;

	if (app == NULL || app->VolumeCombo == NULL)
		return -1;

	selection = SendMessageW(app->VolumeCombo, CB_GETCURSEL, 0, 0);
	if (selection == CB_ERR)
		return -1;

	itemData = SendMessageW(app->VolumeCombo, CB_GETITEMDATA, (WPARAM)selection, 0);
	if (itemData == CB_ERR || itemData < 0 || itemData >= app->VolumeCount)
		return -1;

	return (int)itemData;
}

static const FVE_GUI_VOLUME_ENTRY* GetSelectedVolume(const FVE_GUI_APP* app)
{
	int index = GetSelectedVolumeIndex(app);

	if (index < 0)
		return NULL;

	return &app->Volumes[index];
}

static void SetMessage(FVE_GUI_APP* app, PCWSTR message)
{
	if (app != NULL && app->MessageStatic != NULL)
		SetWindowTextW(app->MessageStatic, message != NULL ? message : L"");
}

static HBRUSH PrepareTransparentControlBackground(HDC dc)
{
	SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
	SetBkColor(dc, GetSysColor(COLOR_WINDOW));
	SetBkMode(dc, TRANSPARENT);
	return GetSysColorBrush(COLOR_WINDOW);
}

static void UpdateButtons(FVE_GUI_APP* app)
{
	const FVE_GUI_VOLUME_ENTRY* volume = GetSelectedVolume(app);
	BOOL hasVolume = volume != NULL;
	BOOL hasStatus = hasVolume && volume->HasStatus && SUCCEEDED(volume->StatusHr);
	BOOL locked = hasStatus && volume->Info.LockStatus == FVE_LIB_LOCK_LOCKED;
	BOOL unlocked = hasStatus && volume->Info.LockStatus == FVE_LIB_LOCK_UNLOCKED;
	BOOL decrypted = hasStatus && volume->Info.VolumeStatus == FVE_LIB_VOLUME_FULLY_DECRYPTED;
	BOOL keyPresent = FALSE;

	if (app->SecretEdit != NULL)
		keyPresent = GetWindowTextLengthW(app->SecretEdit) > 0;

	EnableWindow(app->VolumeCombo, !app->Busy);
	EnableWindow(app->RefreshButton, !app->Busy);
	EnableWindow(app->SecretEdit, !app->Busy);
	EnableWindow(app->RecoveryCheck, !app->Busy);
	EnableWindow(app->UnlockButton, !app->Busy && hasVolume && locked && keyPresent);
	EnableWindow(app->LockButton, !app->Busy && hasVolume && unlocked && !decrypted);
	EnableWindow(app->DecryptButton, !app->Busy && hasVolume && unlocked && !decrypted);
}

static void SetBusy(FVE_GUI_APP* app, BOOL busy, PCWSTR message)
{
	app->Busy = busy;
	SetMessage(app, message);
	UpdateButtons(app);
	SetCursor(LoadCursorW(NULL, busy ? IDC_WAIT : IDC_ARROW));
	UpdateWindow(app->MainWindow);
}

static BOOL ShouldIncludeDrive(UINT driveType)
{
	return driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE;
}

static BOOL ShouldIncludeVolumeStatus(const FVE_GUI_VOLUME_ENTRY* volume)
{
	if (volume == NULL || !volume->HasStatus || FAILED(volume->StatusHr))
		return TRUE;

	return volume->Info.VolumeStatus != FVE_LIB_VOLUME_FULLY_DECRYPTED ||
		volume->Info.LockStatus != FVE_LIB_LOCK_UNLOCKED;
}

static void BuildVolumeDisplayName(FVE_GUI_VOLUME_ENTRY* volume)
{
	WCHAR format[256];
	PCWSTR label = volume->VolumeLabel[0] != L'\0' ?
		volume->VolumeLabel : LoadTempString(IDS_NO_LABEL, L"(no label)");
	PCWSTR fs = volume->FileSystem[0] != L'\0' ?
		volume->FileSystem : LoadTempString(IDS_UNKNOWN_FS, L"unknown fs");

	if (volume->HasStatus && SUCCEEDED(volume->StatusHr))
	{
		LoadResourceStringOrFallback(IDS_VOLUME_DISPLAY_FORMAT, format, ARRAYSIZE(format), L"%s  %s  %s  %s");
		StringCchPrintfW(
			volume->DisplayName,
			ARRAYSIZE(volume->DisplayName),
			format,
			volume->Path,
			label,
			VolumeStatusText(volume->Info.VolumeStatus),
			LockStatusText(volume->Info.LockStatus));
	} else {
		LoadResourceStringOrFallback(IDS_VOLUME_DISPLAY_UNAVAILABLE_FORMAT, format, ARRAYSIZE(format), L"%s  %s  %s  status unavailable");
		StringCchPrintfW(
			volume->DisplayName,
			ARRAYSIZE(volume->DisplayName),
			format,
			volume->Path,
			label,
			fs);
	}
}

static void FillVolumeMetadata(FVE_GUI_VOLUME_ENTRY* volume)
{
	DWORD serial = 0;
	DWORD maxComponent = 0;
	DWORD flags = 0;

	volume->DriveType = GetDriveTypeW(volume->RootPath);
	volume->VolumeLabel[0] = L'\0';
	volume->FileSystem[0] = L'\0';

	GetVolumeInformationW(
		volume->RootPath,
		volume->VolumeLabel,
		ARRAYSIZE(volume->VolumeLabel),
		&serial,
		&maxComponent,
		&flags,
		volume->FileSystem,
		ARRAYSIZE(volume->FileSystem));
}

static void AddDriveVolume(FVE_GUI_APP* app, WCHAR driveLetter)
{
	FVE_GUI_VOLUME_ENTRY* volume;
	HRESULT hr;

	if (app->VolumeCount >= FVE_GUI_MAX_VOLUMES)
		return;

	volume = &app->Volumes[app->VolumeCount];
	ZeroMemory(volume, sizeof(*volume));
	volume->Path[0] = driveLetter;
	volume->Path[1] = L':';
	volume->Path[2] = L'\0';
	volume->RootPath[0] = driveLetter;
	volume->RootPath[1] = L':';
	volume->RootPath[2] = L'\\';
	volume->RootPath[3] = L'\0';

	FillVolumeMetadata(volume);
	if (!ShouldIncludeDrive(volume->DriveType))
		return;

	hr = FveLibGetStatusByPath(volume->Path, &volume->Info);
	volume->StatusHr = hr;
	volume->HasStatus = TRUE;
	if (!ShouldIncludeVolumeStatus(volume))
		return;

	BuildVolumeDisplayName(volume);
	++app->VolumeCount;
}

static void FormatSelectedVolumeStatus(const FVE_GUI_VOLUME_ENTRY* volume, PWSTR output, size_t cchOutput)
{
	WCHAR errorText[256];
	WCHAR format[1024];
	PCWSTR label;

	if (volume == NULL)
	{
		LoadResourceStringOrFallback(IDS_NO_VOLUME_SELECTED, output, cchOutput, L"No volume selected.");
		return;
	}

	label = volume->VolumeLabel[0] != L'\0' ?
		volume->VolumeLabel : LoadTempString(IDS_NO_LABEL, L"(no label)");

	if (!volume->HasStatus || FAILED(volume->StatusHr))
	{
		FormatHRESULT(volume->StatusHr, errorText, ARRAYSIZE(errorText));
		LoadResourceStringOrFallback(
			IDS_STATUS_UNAVAILABLE_FORMAT,
			format,
			ARRAYSIZE(format),
			L"Volume:       %s\r\n"
			L"Label:        %s\r\n"
			L"Drive type:   %s\r\n"
			L"Status:       unavailable\r\n"
			L"Error:        %s\r\n");
		StringCchPrintfW(
			output,
			cchOutput,
			format,
			volume->Path,
			label,
			DriveTypeText(volume->DriveType),
			errorText);
		return;
	}

	LoadResourceStringOrFallback(
		IDS_STATUS_AVAILABLE_FORMAT,
		format,
		ARRAYSIZE(format),
		L"Volume:       %s\r\n"
		L"Label:        %s\r\n"
		L"Drive type:   %s\r\n"
		L"\r\n"
		L"Volume state: %s\r\n"
		L"Protection:   %s\r\n"
		L"Lock state:   %s\r\n");
	StringCchPrintfW(
		output,
		cchOutput,
		format,
		volume->Path,
		label,
		DriveTypeText(volume->DriveType),
		VolumeStatusText(volume->Info.VolumeStatus),
		ProtectionStatusText(volume->Info.ProtectionStatus),
		LockStatusText(volume->Info.LockStatus));
}

static void RefreshStatusText(FVE_GUI_APP* app)
{
	WCHAR text[2048];

	FormatSelectedVolumeStatus(GetSelectedVolume(app), text, ARRAYSIZE(text));
	SetWindowTextW(app->StatusEdit, text);
	UpdateButtons(app);
}

static BOOL GetSelectedPath(FVE_GUI_APP* app, PWSTR output, size_t cchOutput)
{
	const FVE_GUI_VOLUME_ENTRY* volume = GetSelectedVolume(app);

	if (volume == NULL || output == NULL || cchOutput < ARRAYSIZE(volume->Path))
		return FALSE;

	StringCchCopyW(output, cchOutput, volume->Path);
	return TRUE;
}

static void RepopulateVolumeCombo(FVE_GUI_APP* app, PCWSTR preferredPath)
{
	int selectedIndex = -1;

	SendMessageW(app->VolumeCombo, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < app->VolumeCount; ++i)
	{
		LRESULT item = SendMessageW(app->VolumeCombo, CB_ADDSTRING, 0, (LPARAM)app->Volumes[i].DisplayName);
		if (item != CB_ERR && item != CB_ERRSPACE)
			SendMessageW(app->VolumeCombo, CB_SETITEMDATA, (WPARAM)item, (LPARAM)i);

		if (preferredPath != NULL && lstrcmpiW(preferredPath, app->Volumes[i].Path) == 0)
			selectedIndex = i;
	}

	if (selectedIndex < 0 && app->VolumeCount > 0)
		selectedIndex = 0;

	if (selectedIndex >= 0)
	{
		for (int comboIndex = 0; comboIndex < app->VolumeCount; ++comboIndex)
		{
			LRESULT itemData = SendMessageW(app->VolumeCombo, CB_GETITEMDATA, (WPARAM)comboIndex, 0);
			if (itemData == selectedIndex)
			{
				SendMessageW(app->VolumeCombo, CB_SETCURSEL, (WPARAM)comboIndex, 0);
				break;
			}
		}
	}
}

static void RefreshVolumes(FVE_GUI_APP* app, BOOL preserveSelection)
{
	WCHAR previousPath[4] = L"";
	DWORD driveMask;

	if (preserveSelection)
		GetSelectedPath(app, previousPath, ARRAYSIZE(previousPath));

	SetBusy(app, TRUE, LoadTempString(IDS_REFRESHING_VOLUMES, L"Refreshing volumes..."));
	app->VolumeCount = 0;

	driveMask = GetLogicalDrives();
	for (WCHAR letter = L'A'; letter <= L'Z'; ++letter)
	{
		DWORD bit = 1u << (letter - L'A');
		if ((driveMask & bit) != 0)
			AddDriveVolume(app, letter);
	}

	RepopulateVolumeCombo(app, previousPath[0] != L'\0' ? previousPath : NULL);
	RefreshStatusText(app);

	if (app->VolumeCount == 0)
		SetMessage(app, LoadTempString(IDS_NO_VOLUMES_FOUND, L"No actionable BitLocker volumes were found."));
	else if (!app->Elevated)
		SetMessage(app, LoadTempString(IDS_NOT_ELEVATED, L"Not elevated. Lock and turn-off operations may require administrator rights."));
	else
		SetMessage(app, LoadTempString(IDS_READY, L"Ready."));

	SetBusy(app, FALSE, app->VolumeCount == 0 ?
		LoadTempString(IDS_NO_VOLUMES_FOUND, L"No actionable BitLocker volumes were found.") : NULL);
	if (app->VolumeCount > 0)
	{
		if (!app->Elevated)
			SetMessage(app, LoadTempString(IDS_NOT_ELEVATED, L"Not elevated. Lock and turn-off operations may require administrator rights."));
		else
			SetMessage(app, LoadTempString(IDS_READY, L"Ready."));
	}
}

static void ClearSecretEdit(FVE_GUI_APP* app)
{
	WCHAR secret[FVE_GUI_MAX_SECRET];
	int length;

	length = GetWindowTextW(app->SecretEdit, secret, ARRAYSIZE(secret));
	if (length > 0)
		SecureZeroMemory(secret, sizeof(secret));
	SetWindowTextW(app->SecretEdit, L"");
}

static BOOL ReadSecret(FVE_GUI_APP* app, PWSTR output, size_t cchOutput)
{
	if (GetWindowTextW(app->SecretEdit, output, (int)cchOutput) <= 0)
	{
		MessageBoxW(
			app->MainWindow,
			LoadTempString(IDS_MISSING_KEY_MESSAGE, L"Enter a password or recovery key first."),
			LoadTempString(IDS_MISSING_KEY_TITLE, L"Missing key"),
			MB_OK | MB_ICONWARNING);
		return FALSE;
	}

	return TRUE;
}

static void RunUnlock(FVE_GUI_APP* app)
{
	WCHAR volumePath[4];
	WCHAR secret[FVE_GUI_MAX_SECRET];
	BOOL recovery;
	HRESULT hr;

	if (!GetSelectedPath(app, volumePath, ARRAYSIZE(volumePath)) ||
		!ReadSecret(app, secret, ARRAYSIZE(secret)))
		return;

	recovery = SendMessageW(app->RecoveryCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
	SetBusy(app, TRUE, LoadTempString(IDS_UNLOCKING_VOLUME, L"Unlocking volume..."));

	if (recovery)
		hr = FveLibUnlockWithRecoveryPasswordByPath(volumePath, secret);
	else
		hr = FveLibUnlockWithPasswordByPath(volumePath, secret);

	SecureZeroMemory(secret, sizeof(secret));
	ClearSecretEdit(app);

	if (FAILED(hr))
	{
		SetBusy(app, FALSE, LoadTempString(IDS_UNLOCK_FAILED, L"Unlock failed."));
		ShowHrError(app->MainWindow, LoadTempString(IDS_OPERATION_UNLOCK, L"Unlock"), hr);
		RefreshVolumes(app, TRUE);
		return;
	}

	SetBusy(app, FALSE, LoadTempString(IDS_VOLUME_UNLOCKED, L"Volume unlocked."));
	RefreshVolumes(app, TRUE);
	SetMessage(app, LoadTempString(IDS_VOLUME_UNLOCKED, L"Volume unlocked."));
}

static void RunLock(FVE_GUI_APP* app)
{
	WCHAR volumePath[4];
	HRESULT hr;

	if (!GetSelectedPath(app, volumePath, ARRAYSIZE(volumePath)))
		return;

	if (MessageBoxW(
		app->MainWindow,
		LoadTempString(IDS_CONFIRM_LOCK_MESSAGE, L"Lock the selected BitLocker volume?"),
		LoadTempString(IDS_CONFIRM_LOCK_TITLE, L"Confirm lock"),
		MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
		return;

	SetBusy(app, TRUE, LoadTempString(IDS_LOCKING_VOLUME, L"Locking volume..."));
	hr = FveLibLockVolumeByPath(volumePath, FALSE);
	if (FAILED(hr))
	{
		SetBusy(app, FALSE, LoadTempString(IDS_LOCK_FAILED, L"Lock failed."));
		ShowHrError(app->MainWindow, LoadTempString(IDS_OPERATION_LOCK, L"Lock"), hr);
		RefreshVolumes(app, TRUE);
		return;
	}

	SetBusy(app, FALSE, LoadTempString(IDS_VOLUME_LOCKED, L"Volume locked."));
	RefreshVolumes(app, TRUE);
	SetMessage(app, LoadTempString(IDS_VOLUME_LOCKED, L"Volume locked."));
}

static void RunTurnOffBitLocker(FVE_GUI_APP* app)
{
	const FVE_GUI_VOLUME_ENTRY* volume = GetSelectedVolume(app);
	WCHAR volumePath[4];
	HRESULT hr;

	if (volume == NULL || !GetSelectedPath(app, volumePath, ARRAYSIZE(volumePath)))
		return;

	if (volume->HasStatus && SUCCEEDED(volume->StatusHr))
	{
		if (volume->Info.LockStatus == FVE_LIB_LOCK_LOCKED)
		{
			MessageBoxW(
				app->MainWindow,
				LoadTempString(IDS_UNLOCK_BEFORE_TURN_OFF_MESSAGE, L"Unlock the volume before turning off BitLocker."),
				LoadTempString(IDS_VOLUME_LOCKED_TITLE, L"Volume locked"),
				MB_OK | MB_ICONWARNING);
			return;
		}
		if (volume->Info.VolumeStatus == FVE_LIB_VOLUME_FULLY_DECRYPTED)
		{
			MessageBoxW(
				app->MainWindow,
				LoadTempString(IDS_ALREADY_DECRYPTED_MESSAGE, L"The selected volume is already fully decrypted."),
				LoadTempString(IDS_BITLOCKER_TITLE, L"BitLocker"),
				MB_OK | MB_ICONINFORMATION);
			return;
		}
		if (volume->Info.VolumeStatus == FVE_LIB_VOLUME_DECRYPTION_IN_PROGRESS)
		{
			MessageBoxW(
				app->MainWindow,
				LoadTempString(IDS_DECRYPTION_IN_PROGRESS_MESSAGE, L"BitLocker decryption is already in progress."),
				LoadTempString(IDS_BITLOCKER_TITLE, L"BitLocker"),
				MB_OK | MB_ICONINFORMATION);
			return;
		}
	}

	if (MessageBoxW(
		app->MainWindow,
		LoadTempString(IDS_CONFIRM_TURN_OFF_MESSAGE, L"Turn off BitLocker for the selected volume?\r\n\r\nThis starts full-volume decryption and may take a long time."),
		LoadTempString(IDS_CONFIRM_TURN_OFF_TITLE, L"Confirm BitLocker turn off"),
		MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
		return;

	SetBusy(app, TRUE, LoadTempString(IDS_STARTING_DECRYPTION, L"Starting decryption..."));
	hr = FveLibStartDecryptionByPath(volumePath);
	if (FAILED(hr))
	{
		SetBusy(app, FALSE, LoadTempString(IDS_TURN_OFF_FAILED, L"Turn off failed."));
		ShowHrError(app->MainWindow, LoadTempString(IDS_OPERATION_TURN_OFF, L"Turn off BitLocker"), hr);
		RefreshVolumes(app, TRUE);
		return;
	}

	SetBusy(app, FALSE, LoadTempString(IDS_DECRYPTION_STARTED, L"BitLocker decryption started."));
	RefreshVolumes(app, TRUE);
	SetMessage(app, LoadTempString(IDS_DECRYPTION_STARTED, L"BitLocker decryption started."));
}

static void LayoutControls(HWND window, FVE_GUI_APP* app)
{
	RECT client;
	int margin;
	int labelWidth;
	int buttonWidth;
	int buttonHeight;
	int rowHeight;
	int top;
	int keyTop;
	int buttonsTop;
	int messageTop;
	int statusTop;
	int statusHeight;
	int clientWidth;
	int clientHeight;
	int comboWidth;
	int secretWidth;
	int recoveryWidth;

	GetClientRect(window, &client);
	clientWidth = client.right - client.left;
	clientHeight = client.bottom - client.top;

	margin = ScaleForWindow(window, 12);
	labelWidth = ScaleForWindow(window, 74);
	buttonWidth = ScaleForWindow(window, 106);
	buttonHeight = ScaleForWindow(window, 28);
	rowHeight = ScaleForWindow(window, 28);
	recoveryWidth = ScaleForWindow(window, 112);

	top = margin;
	comboWidth = clientWidth - margin * 3 - labelWidth - buttonWidth;
	if (comboWidth < ScaleForWindow(window, 180))
		comboWidth = ScaleForWindow(window, 180);

	MoveWindow(app->VolumeLabel, margin, top + ScaleForWindow(window, 4), labelWidth, rowHeight, TRUE);
	MoveWindow(app->VolumeCombo, margin + labelWidth, top, comboWidth, ScaleForWindow(window, 240), TRUE);
	MoveWindow(app->RefreshButton, margin + labelWidth + comboWidth + margin, top, buttonWidth, buttonHeight, TRUE);

	statusTop = top + rowHeight + margin;
	messageTop = clientHeight - margin - ScaleForWindow(window, 20);
	buttonsTop = messageTop - margin - buttonHeight;
	keyTop = buttonsTop - margin - rowHeight;
	statusHeight = keyTop - margin - statusTop;
	if (statusHeight < ScaleForWindow(window, 120))
		statusHeight = ScaleForWindow(window, 120);

	MoveWindow(app->StatusEdit, margin, statusTop, clientWidth - margin * 2, statusHeight, TRUE);

	MoveWindow(app->SecretLabel, margin, keyTop + ScaleForWindow(window, 4), labelWidth, rowHeight, TRUE);
	secretWidth = clientWidth - margin * 4 - labelWidth - recoveryWidth;
	if (secretWidth < ScaleForWindow(window, 180))
		secretWidth = ScaleForWindow(window, 180);
	MoveWindow(app->SecretEdit, margin + labelWidth, keyTop, secretWidth, buttonHeight, TRUE);
	MoveWindow(app->RecoveryCheck, margin + labelWidth + secretWidth + margin, keyTop + ScaleForWindow(window, 2), recoveryWidth, rowHeight, TRUE);

	MoveWindow(app->UnlockButton, margin + labelWidth, buttonsTop, buttonWidth, buttonHeight, TRUE);
	MoveWindow(app->LockButton, margin + labelWidth + buttonWidth + margin, buttonsTop, buttonWidth, buttonHeight, TRUE);
	MoveWindow(app->DecryptButton, margin + labelWidth + (buttonWidth + margin) * 2, buttonsTop, ScaleForWindow(window, 154), buttonHeight, TRUE);
	MoveWindow(app->MessageStatic, margin, messageTop, clientWidth - margin * 2, ScaleForWindow(window, 22), TRUE);
}

static BOOL CreateControls(FVE_GUI_APP* app)
{
	app->Font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	app->VolumeLabel = CreateChildWindow(app, L"STATIC", app->Text.VolumeLabel, 0, 0, -1);
	app->VolumeCombo = CreateChildWindow(app, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 0, IDC_VOLUME_COMBO);
	app->RefreshButton = CreateChildWindow(app, L"BUTTON", app->Text.RefreshButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_REFRESH_BUTTON);
	app->StatusEdit = CreateChildWindow(app, L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_STATUS_EDIT);
	app->SecretLabel = CreateChildWindow(app, L"STATIC", app->Text.SecretLabel, 0, 0, -1);
	app->SecretEdit = CreateChildWindow(app, L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_SECRET_EDIT);
	app->RecoveryCheck = CreateChildWindow(app, L"BUTTON", app->Text.RecoveryCheck, BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_RECOVERY_CHECK);
	app->UnlockButton = CreateChildWindow(app, L"BUTTON", app->Text.UnlockButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_UNLOCK_BUTTON);
	app->LockButton = CreateChildWindow(app, L"BUTTON", app->Text.LockButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_LOCK_BUTTON);
	app->DecryptButton = CreateChildWindow(app, L"BUTTON", app->Text.TurnOffButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_DECRYPT_BUTTON);
	app->MessageStatic = CreateChildWindow(app, L"STATIC", L"", 0, 0, IDC_MESSAGE_STATIC);

	return app->VolumeLabel != NULL &&
		app->VolumeCombo != NULL &&
		app->RefreshButton != NULL &&
		app->StatusEdit != NULL &&
		app->SecretLabel != NULL &&
		app->SecretEdit != NULL &&
		app->RecoveryCheck != NULL &&
		app->UnlockButton != NULL &&
		app->LockButton != NULL &&
		app->DecryptButton != NULL &&
		app->MessageStatic != NULL;
}

static LRESULT CALLBACK MainWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	FVE_GUI_APP* app = (FVE_GUI_APP*)GetWindowLongPtrW(window, GWLP_USERDATA);

	switch (message)
	{
	case WM_CREATE:
	{
		CREATESTRUCTW* create = (CREATESTRUCTW*)lParam;
		app = (FVE_GUI_APP*)create->lpCreateParams;
		app->MainWindow = window;
		SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)app);
		if (!CreateControls(app))
			return -1;
		LayoutControls(window, app);
		RefreshVolumes(app, FALSE);
		return 0;
	}
	case WM_SIZE:
		if (app != NULL)
			LayoutControls(window, app);
		return 0;
	case WM_GETMINMAXINFO:
	{
		MINMAXINFO* minMax = (MINMAXINFO*)lParam;
		minMax->ptMinTrackSize.x = ScaleForWindow(window, 620);
		minMax->ptMinTrackSize.y = ScaleForWindow(window, 420);
		return 0;
	}
	case WM_SETCURSOR:
		if (app != NULL && app->Busy)
		{
			SetCursor(LoadCursorW(NULL, IDC_WAIT));
			return TRUE;
		}
		break;
	case WM_CTLCOLORSTATIC:
		if (app != NULL)
		{
			HWND control = (HWND)lParam;
			if (control == app->VolumeLabel ||
				control == app->SecretLabel ||
				control == app->MessageStatic ||
				control == app->RecoveryCheck)
			{
				return (LRESULT)PrepareTransparentControlBackground((HDC)wParam);
			}
		}
		break;
	case WM_CTLCOLORBTN:
		if (app != NULL && (HWND)lParam == app->RecoveryCheck)
			return (LRESULT)PrepareTransparentControlBackground((HDC)wParam);
		break;
	case WM_COMMAND:
		if (app == NULL)
			break;

		switch (LOWORD(wParam))
		{
		case IDC_VOLUME_COMBO:
			if (HIWORD(wParam) == CBN_SELCHANGE)
				RefreshStatusText(app);
			return 0;
		case IDC_SECRET_EDIT:
			if (HIWORD(wParam) == EN_CHANGE)
				UpdateButtons(app);
			return 0;
		case IDC_REFRESH_BUTTON:
			RefreshVolumes(app, TRUE);
			return 0;
		case IDC_UNLOCK_BUTTON:
			RunUnlock(app);
			return 0;
		case IDC_LOCK_BUTTON:
			RunLock(app);
			return 0;
		case IDC_DECRYPT_BUTTON:
			RunTurnOffBitLocker(app);
			return 0;
		default:
			break;
		}
		break;
	case WM_DESTROY:
		ClearSecretEdit(&gApp);
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}

	return DefWindowProcW(window, message, wParam, lParam);
}

static BOOL RegisterMainWindowClass(HINSTANCE instance)
{
	WNDCLASSEXW wc;
	HICON icon;
	HICON smallIcon;

	icon = (HICON)LoadImageW(
		instance,
		MAKEINTRESOURCEW(IDI_FVEGUI_APP),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXICON),
		GetSystemMetrics(SM_CYICON),
		LR_DEFAULTCOLOR);
	smallIcon = (HICON)LoadImageW(
		instance,
		MAKEINTRESOURCEW(IDI_FVEGUI_APP),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR);

	if (icon == NULL)
		icon = LoadIconW(NULL, IDI_APPLICATION);
	if (smallIcon == NULL)
		smallIcon = LoadIconW(NULL, IDI_APPLICATION);

	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWindowProc;
	wc.hInstance = instance;
	wc.hIcon = icon;
	wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszClassName = FVE_GUI_CLASS_NAME;
	wc.hIconSm = smallIcon;

	return RegisterClassExW(&wc) != 0;
}

int APIENTRY
wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	HRESULT hr;
	HWND window;
	MSG message;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	ZeroMemory(&gApp, sizeof(gApp));
	gApp.Instance = hInstance;
	gApp.Elevated = IsProcessElevated();
	LoadGuiText(&gApp.Text);

	hr = FveLibInit();
	if (FAILED(hr))
	{
		ShowHrError(NULL, LoadTempString(IDS_OPERATION_INIT_API, L"Initialize BitLocker API"), hr);
		return 1;
	}

	if (!RegisterMainWindowClass(hInstance))
	{
		ShowHrError(NULL, LoadTempString(IDS_OPERATION_REGISTER_CLASS, L"Register window class"), HRESULT_FROM_WIN32(GetLastError()));
		FveLibFini();
		return 1;
	}

	window = CreateWindowExW(
		0,
		FVE_GUI_CLASS_NAME,
		gApp.Text.AppTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		ScaleForWindow(NULL, 760),
		ScaleForWindow(NULL, 520),
		NULL,
		NULL,
		hInstance,
		&gApp);

	if (window == NULL)
	{
		ShowHrError(NULL, LoadTempString(IDS_OPERATION_CREATE_WINDOW, L"Create main window"), HRESULT_FROM_WIN32(GetLastError()));
		FveLibFini();
		return 1;
	}

	ShowWindow(window, nCmdShow);
	UpdateWindow(window);

	while (GetMessageW(&message, NULL, 0, 0) > 0)
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}

	FveLibFini();
	return (int)message.wParam;
}
