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

#include <commctrl.h>
#include <shellapi.h>
#include <strsafe.h>

#include "../fvelib/fvelib.h"
#include "resource.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#define FVE_GUI_CLASS_NAME L"FveToolNativeGuiWindow"
#define FVE_GUI_MAX_VOLUMES 32
#define FVE_GUI_MAX_SECRET 2048
#define FVE_GUI_TEMP_STRING_BUFFERS 16
#define FVE_GUI_TEMP_STRING_CCH 512

#define FVE_GUI_TAB_DECRYPT 0
#define FVE_GUI_TAB_ENCRYPT 1

#define IDC_TAB_CONTROL 1000
#define IDC_VOLUME_COMBO 1001
#define IDC_REFRESH_BUTTON 1002
#define IDC_SECRET_EDIT 1004
#define IDC_RECOVERY_CHECK 1005
#define IDC_UNLOCK_BUTTON 1006
#define IDC_LOCK_BUTTON 1007
#define IDC_DECRYPT_BUTTON 1008
#define IDC_MESSAGE_STATIC 1009
#define IDC_STATUS_LABEL 1011
#define IDC_DECRYPT_STATUS_STATIC 1012
#define IDC_ENCRYPT_VOLUME_COMBO 1020
#define IDC_ENCRYPT_REFRESH_BUTTON 1021
#define IDC_ENCRYPT_PASSWORD_EDIT 1022
#define IDC_ENCRYPT_CONFIRM_EDIT 1023
#define IDC_ENCRYPT_BUTTON 1024
#define IDC_RECOVERY_KEY_EDIT 1025
#define IDC_COPY_RECOVERY_BUTTON 1026

typedef struct FVE_GUI_VOLUME_ENTRY
{
	WCHAR Path[4];
	WCHAR RootPath[4];
	WCHAR DisplayName[256];
	WCHAR VolumeLabel[128];
	UINT DriveType;
	FVE_LIB_VOLUME_INFO Info;
	HRESULT StatusHr;
	BOOL HasStatus;
} FVE_GUI_VOLUME_ENTRY;

typedef struct FVE_GUI_TEXT
{
	WCHAR AppTitle[128];
	WCHAR DecryptTab[64];
	WCHAR EncryptTab[64];
	WCHAR VolumeLabel[64];
	WCHAR RefreshButton[64];
	WCHAR SecretLabel[64];
	WCHAR RecoveryCheck[64];
	WCHAR UnlockButton[64];
	WCHAR LockButton[64];
	WCHAR TurnOffButton[96];
	WCHAR EncryptPasswordLabel[64];
	WCHAR ConfirmPasswordLabel[64];
	WCHAR EncryptButton[64];
	WCHAR RecoveryKeyLabel[64];
	WCHAR CopyButton[64];
	WCHAR StatusLabel[64];
} FVE_GUI_TEXT;

typedef struct FVE_GUI_APP
{
	HINSTANCE Instance;
	HWND MainWindow;
	HWND TabControl;
	HWND VolumeLabel;
	HWND VolumeCombo;
	HWND RefreshButton;
	HWND SecretLabel;
	HWND SecretEdit;
	HWND RecoveryCheck;
	HWND UnlockButton;
	HWND LockButton;
	HWND DecryptButton;
	HWND DecryptStatusStatic;
	HWND EncryptVolumeLabel;
	HWND EncryptVolumeCombo;
	HWND EncryptRefreshButton;
	HWND EncryptPasswordLabel;
	HWND EncryptPasswordEdit;
	HWND EncryptConfirmLabel;
	HWND EncryptConfirmEdit;
	HWND EncryptButton;
	HWND RecoveryKeyLabel;
	HWND RecoveryKeyEdit;
	HWND CopyRecoveryButton;
	HWND StatusLabel;
	HWND MessageStatic;
	HFONT Font;
	HBRUSH TabBodyBrush;
	BOOL Busy;
	BOOL Elevated;
	int ActiveTab;
	int StartupTab;
	WCHAR StartupVolumePath[4];
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
	LOAD_TEXT(DecryptTab, IDS_DECRYPT_TAB, L"Decrypt");
	LOAD_TEXT(EncryptTab, IDS_ENCRYPT_TAB, L"Encrypt");
	LOAD_TEXT(VolumeLabel, IDS_VOLUME_LABEL, L"Volume:");
	LOAD_TEXT(RefreshButton, IDS_REFRESH_BUTTON, L"Refresh");
	LOAD_TEXT(SecretLabel, IDS_SECRET_LABEL, L"Unlock key:");
	LOAD_TEXT(RecoveryCheck, IDS_RECOVERY_CHECK, L"Recovery key");
	LOAD_TEXT(UnlockButton, IDS_UNLOCK_BUTTON, L"Unlock");
	LOAD_TEXT(LockButton, IDS_LOCK_BUTTON, L"Lock");
	LOAD_TEXT(TurnOffButton, IDS_TURN_OFF_BUTTON, L"Turn off BitLocker");
	LOAD_TEXT(EncryptPasswordLabel, IDS_ENCRYPT_PASSWORD_LABEL, L"Password:");
	LOAD_TEXT(ConfirmPasswordLabel, IDS_CONFIRM_PASSWORD_LABEL, L"Confirm:");
	LOAD_TEXT(EncryptButton, IDS_ENCRYPT_BUTTON, L"Turn on BitLocker");
	LOAD_TEXT(RecoveryKeyLabel, IDS_RECOVERY_KEY_LABEL, L"Recovery key:");
	LOAD_TEXT(CopyButton, IDS_COPY_BUTTON, L"Copy");
	LOAD_TEXT(StatusLabel, IDS_STATUS_LABEL, L"Status:");

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

static BOOL NormalizeCommandLineVolumePath(PCWSTR input, PWSTR output, size_t cchOutput)
{
	WCHAR letter;

	if (output == NULL || cchOutput < 3)
		return FALSE;

	output[0] = L'\0';
	if (input == NULL || input[0] == L'\0')
		return FALSE;

	letter = input[0];
	if (letter >= L'a' && letter <= L'z')
		letter = (WCHAR)(letter - L'a' + L'A');

	if (letter < L'A' || letter > L'Z')
		return FALSE;
	if (input[1] != L'\0' &&
		(input[1] != L':' ||
			(input[2] != L'\0' && (input[2] != L'\\' || input[3] != L'\0'))))
		return FALSE;

	output[0] = letter;
	output[1] = L':';
	output[2] = L'\0';
	return TRUE;
}

static void ParseStartupOptions(FVE_GUI_APP* app)
{
	int argc;
	LPWSTR* argv;

	if (app == NULL)
		return;

	app->StartupTab = FVE_GUI_TAB_DECRYPT;
	app->StartupVolumePath[0] = L'\0';

	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv == NULL)
		return;

	if (argc < 2)
	{
		LocalFree(argv);
		return;
	}

	if (lstrcmpiW(argv[1], L"/encrypt") == 0)
		app->StartupTab = FVE_GUI_TAB_ENCRYPT;
	else if (lstrcmpiW(argv[1], L"/decrypt") == 0)
		app->StartupTab = FVE_GUI_TAB_DECRYPT;
	else {
		LocalFree(argv);
		return;
	}

	if (argc >= 3)
		NormalizeCommandLineVolumePath(argv[2], app->StartupVolumePath, ARRAYSIZE(app->StartupVolumePath));

	LocalFree(argv);
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

static int GetSelectedVolumeIndexFromCombo(const FVE_GUI_APP* app, HWND combo)
{
	LRESULT selection;
	LRESULT itemData;

	if (app == NULL || combo == NULL)
		return -1;

	selection = SendMessageW(combo, CB_GETCURSEL, 0, 0);
	if (selection == CB_ERR)
		return -1;

	itemData = SendMessageW(combo, CB_GETITEMDATA, (WPARAM)selection, 0);
	if (itemData == CB_ERR || itemData < 0 || itemData >= app->VolumeCount)
		return -1;

	return (int)itemData;
}

static const FVE_GUI_VOLUME_ENTRY* GetSelectedVolumeFromCombo(const FVE_GUI_APP* app, HWND combo)
{
	int index = GetSelectedVolumeIndexFromCombo(app, combo);

	if (index < 0)
		return NULL;

	return &app->Volumes[index];
}

static const FVE_GUI_VOLUME_ENTRY* GetSelectedDecryptVolume(const FVE_GUI_APP* app)
{
	return GetSelectedVolumeFromCombo(app, app != NULL ? app->VolumeCombo : NULL);
}

static const FVE_GUI_VOLUME_ENTRY* GetSelectedEncryptVolume(const FVE_GUI_APP* app)
{
	return GetSelectedVolumeFromCombo(app, app != NULL ? app->EncryptVolumeCombo : NULL);
}

static void SetMessage(FVE_GUI_APP* app, PCWSTR message)
{
	if (app != NULL && app->MessageStatic != NULL)
		SetWindowTextW(app->MessageStatic, message != NULL ? message : L"");
}

static void UpdateTabBodyBrush(FVE_GUI_APP* app)
{
	HDC tabDC;
	HDC memDC;
	HBITMAP bmp;
	HBITMAP oldBmp;
	RECT tabRect;
	RECT bodyRect;
	COLORREF color;

	if (app->TabBodyBrush != NULL)
	{
		DeleteObject(app->TabBodyBrush);
		app->TabBodyBrush = NULL;
	}

	if (app->TabControl == NULL)
		return;

	GetClientRect(app->TabControl, &tabRect);
	if (tabRect.right <= 0 || tabRect.bottom <= 0)
		return;

	bodyRect = tabRect;
	SendMessageW(app->TabControl, TCM_ADJUSTRECT, FALSE, (LPARAM)&bodyRect);

	tabDC = GetDC(app->TabControl);
	if (tabDC == NULL)
		return;

	memDC = CreateCompatibleDC(tabDC);
	bmp = CreateCompatibleBitmap(tabDC, tabRect.right, tabRect.bottom);
	oldBmp = (HBITMAP)SelectObject(memDC, bmp);

	SendMessageW(app->TabControl, WM_PRINTCLIENT, (WPARAM)memDC, PRF_CLIENT);
	color = GetPixel(memDC, (bodyRect.left + bodyRect.right) / 2, (bodyRect.top + bodyRect.bottom) / 2);

	SelectObject(memDC, oldBmp);
	DeleteObject(bmp);
	DeleteDC(memDC);
	ReleaseDC(app->TabControl, tabDC);

	if (color != CLR_INVALID)
		app->TabBodyBrush = CreateSolidBrush(color);
}

static HBRUSH PrepareTabControlBackground(HDC dc, HBRUSH tabBrush)
{
	SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
	SetBkMode(dc, TRANSPARENT);
	return tabBrush != NULL ? tabBrush : GetSysColorBrush(COLOR_BTNFACE);
}

static HBRUSH PrepareWindowControlBackground(HDC dc)
{
	SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
	SetBkColor(dc, GetSysColor(COLOR_WINDOW));
	SetBkMode(dc, OPAQUE);
	return GetSysColorBrush(COLOR_WINDOW);
}

static int GetComboItemCount(HWND combo)
{
	LRESULT count;

	if (combo == NULL)
		return 0;

	count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
	if (count == CB_ERR || count < 0)
		return 0;

	return (int)count;
}

static void UpdateButtons(FVE_GUI_APP* app)
{
	const FVE_GUI_VOLUME_ENTRY* volume = GetSelectedDecryptVolume(app);
	const FVE_GUI_VOLUME_ENTRY* encryptVolume = GetSelectedEncryptVolume(app);
	BOOL hasVolume = volume != NULL;
	BOOL hasStatus = hasVolume && volume->HasStatus && SUCCEEDED(volume->StatusHr);
	BOOL locked = hasStatus && volume->Info.LockStatus == FVE_LIB_LOCK_LOCKED;
	BOOL unlocked = hasStatus && volume->Info.LockStatus == FVE_LIB_LOCK_UNLOCKED;
	BOOL decrypted = hasStatus && volume->Info.VolumeStatus == FVE_LIB_VOLUME_FULLY_DECRYPTED;
	BOOL keyPresent = FALSE;
	BOOL passwordPresent = FALSE;
	BOOL confirmPresent = FALSE;
	BOOL recoveryPresent = FALSE;

	if (app->SecretEdit != NULL)
		keyPresent = GetWindowTextLengthW(app->SecretEdit) > 0;
	if (app->EncryptPasswordEdit != NULL)
		passwordPresent = GetWindowTextLengthW(app->EncryptPasswordEdit) > 0;
	if (app->EncryptConfirmEdit != NULL)
		confirmPresent = GetWindowTextLengthW(app->EncryptConfirmEdit) > 0;
	if (app->RecoveryKeyEdit != NULL)
		recoveryPresent = GetWindowTextLengthW(app->RecoveryKeyEdit) > 0;

	EnableWindow(app->VolumeCombo, !app->Busy);
	EnableWindow(app->RefreshButton, !app->Busy);
	EnableWindow(app->SecretEdit, !app->Busy);
	EnableWindow(app->RecoveryCheck, !app->Busy);
	EnableWindow(app->UnlockButton, !app->Busy && hasVolume && locked && keyPresent);
	EnableWindow(app->LockButton, !app->Busy && hasVolume && unlocked && !decrypted);
	EnableWindow(app->DecryptButton, !app->Busy && hasVolume && unlocked && !decrypted);
	EnableWindow(app->EncryptVolumeCombo, !app->Busy);
	EnableWindow(app->EncryptRefreshButton, !app->Busy);
	EnableWindow(app->EncryptPasswordEdit, !app->Busy);
	EnableWindow(app->EncryptConfirmEdit, !app->Busy);
	EnableWindow(app->EncryptButton, !app->Busy && encryptVolume != NULL && passwordPresent && confirmPresent);
	EnableWindow(app->CopyRecoveryButton, !app->Busy && recoveryPresent);
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

static BOOL ShouldIncludeDecryptVolume(const FVE_GUI_VOLUME_ENTRY* volume)
{
	if (volume == NULL)
		return FALSE;

	if (!ShouldIncludeDrive(volume->DriveType))
		return FALSE;

	if (!volume->HasStatus || FAILED(volume->StatusHr))
		return TRUE;

	return volume->Info.VolumeStatus != FVE_LIB_VOLUME_FULLY_DECRYPTED ||
		volume->Info.LockStatus != FVE_LIB_LOCK_UNLOCKED;
}

static BOOL ShouldIncludeEncryptVolume(const FVE_GUI_VOLUME_ENTRY* volume)
{
	return volume != NULL;
}

static void BuildVolumeDisplayName(FVE_GUI_VOLUME_ENTRY* volume)
{
	PCWSTR label = volume->VolumeLabel[0] != L'\0' ?
		volume->VolumeLabel : LoadTempString(IDS_NO_LABEL, L"(no label)");

	StringCchPrintfW(
		volume->DisplayName,
		ARRAYSIZE(volume->DisplayName),
		L"%s  %s",
		volume->Path,
		label);
}

static void FillVolumeMetadata(FVE_GUI_VOLUME_ENTRY* volume)
{
	DWORD serial = 0;
	DWORD maxComponent = 0;
	DWORD flags = 0;

	volume->DriveType = GetDriveTypeW(volume->RootPath);
	volume->VolumeLabel[0] = L'\0';

	GetVolumeInformationW(
		volume->RootPath,
		volume->VolumeLabel,
		ARRAYSIZE(volume->VolumeLabel),
		&serial,
		&maxComponent,
		&flags,
		NULL,
		0);
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
	hr = FveLibGetStatusByPath(volume->Path, &volume->Info);
	volume->StatusHr = hr;
	volume->HasStatus = TRUE;

	BuildVolumeDisplayName(volume);
	++app->VolumeCount;
}

static void FormatSelectedVolumeStatus(const FVE_GUI_VOLUME_ENTRY* volume, PWSTR output, size_t cchOutput)
{
	WCHAR errorText[256];
	WCHAR format[1024];

	if (volume == NULL)
	{
		LoadResourceStringOrFallback(IDS_NO_VOLUME_SELECTED, output, cchOutput, L"No volume selected.");
		return;
	}

	if (!volume->HasStatus || FAILED(volume->StatusHr))
	{
		FormatHRESULT(volume->StatusHr, errorText, ARRAYSIZE(errorText));
		LoadResourceStringOrFallback(
			IDS_DECRYPT_STATUS_UNAVAILABLE_FORMAT,
			format,
			ARRAYSIZE(format),
			L"Status:       unavailable\r\n"
			L"Error:        %s");
		StringCchPrintfW(
			output,
			cchOutput,
			format,
			errorText);
		return;
	}

	LoadResourceStringOrFallback(
		IDS_DECRYPT_STATUS_AVAILABLE_FORMAT,
		format,
		ARRAYSIZE(format),
		L"Volume state: %s\r\n"
		L"Protection:   %s\r\n"
		L"Lock state:   %s");
	StringCchPrintfW(
		output,
		cchOutput,
		format,
		VolumeStatusText(volume->Info.VolumeStatus),
		ProtectionStatusText(volume->Info.ProtectionStatus),
		LockStatusText(volume->Info.LockStatus));
}

static void RefreshStatusText(FVE_GUI_APP* app)
{
	WCHAR statusText[1024];

	if (app != NULL && app->DecryptStatusStatic != NULL)
	{
		FormatSelectedVolumeStatus(GetSelectedDecryptVolume(app), statusText, ARRAYSIZE(statusText));
		SetWindowTextW(app->DecryptStatusStatic, statusText);
	}

	UpdateButtons(app);
}

static BOOL GetSelectedPathFromCombo(FVE_GUI_APP* app, HWND combo, PWSTR output, size_t cchOutput)
{
	const FVE_GUI_VOLUME_ENTRY* volume = GetSelectedVolumeFromCombo(app, combo);

	if (volume == NULL || output == NULL || cchOutput < ARRAYSIZE(volume->Path))
		return FALSE;

	StringCchCopyW(output, cchOutput, volume->Path);
	return TRUE;
}

static BOOL GetSelectedDecryptPath(FVE_GUI_APP* app, PWSTR output, size_t cchOutput)
{
	return GetSelectedPathFromCombo(app, app != NULL ? app->VolumeCombo : NULL, output, cchOutput);
}

static BOOL GetSelectedEncryptPath(FVE_GUI_APP* app, PWSTR output, size_t cchOutput)
{
	return GetSelectedPathFromCombo(app, app != NULL ? app->EncryptVolumeCombo : NULL, output, cchOutput);
}

static int RepopulateVolumeCombo(
	FVE_GUI_APP* app,
	HWND combo,
	PCWSTR preferredPath,
	BOOL(*includeVolume)(const FVE_GUI_VOLUME_ENTRY*))
{
	int selectedIndex = -1;
	int itemCount = 0;

	if (app == NULL || combo == NULL)
		return 0;

	SendMessageW(combo, CB_RESETCONTENT, 0, 0);
	for (int i = 0; i < app->VolumeCount; ++i)
	{
		LRESULT item;

		if (includeVolume != NULL && !includeVolume(&app->Volumes[i]))
			continue;

		item = SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)app->Volumes[i].DisplayName);
		if (item != CB_ERR && item != CB_ERRSPACE)
		{
			SendMessageW(combo, CB_SETITEMDATA, (WPARAM)item, (LPARAM)i);
			++itemCount;
		}

		if (preferredPath != NULL && lstrcmpiW(preferredPath, app->Volumes[i].Path) == 0)
			selectedIndex = i;
	}

	if (selectedIndex < 0 && itemCount > 0)
	{
		LRESULT itemData = SendMessageW(combo, CB_GETITEMDATA, 0, 0);
		if (itemData != CB_ERR)
			selectedIndex = (int)itemData;
	}

	if (selectedIndex >= 0)
	{
		for (int comboIndex = 0; comboIndex < itemCount; ++comboIndex)
		{
			LRESULT itemData = SendMessageW(combo, CB_GETITEMDATA, (WPARAM)comboIndex, 0);
			if (itemData == selectedIndex)
			{
				SendMessageW(combo, CB_SETCURSEL, (WPARAM)comboIndex, 0);
				break;
			}
		}
	}

	return itemCount;
}

static void SetReadyMessage(FVE_GUI_APP* app)
{
	int visibleCount;

	if (app == NULL)
		return;

	visibleCount = app->ActiveTab == FVE_GUI_TAB_ENCRYPT ?
		GetComboItemCount(app->EncryptVolumeCombo) :
		GetComboItemCount(app->VolumeCombo);

	if (visibleCount == 0)
	{
		if (app->ActiveTab == FVE_GUI_TAB_ENCRYPT)
			SetMessage(app, LoadTempString(IDS_NO_DRIVES_FOUND, L"No volumes were found."));
		else
			SetMessage(app, LoadTempString(IDS_NO_VOLUMES_FOUND, L"No actionable BitLocker volumes were found."));
	} else if (!app->Elevated) {
		SetMessage(app, LoadTempString(IDS_NOT_ELEVATED, L"Not elevated. Lock, turn-off, and encryption operations may require administrator rights."));
	} else {
		SetMessage(app, LoadTempString(IDS_READY, L"Ready."));
	}
}

static void RefreshVolumes(FVE_GUI_APP* app, BOOL preserveSelection)
{
	WCHAR previousDecryptPath[4] = L"";
	WCHAR previousEncryptPath[4] = L"";
	DWORD driveMask;

	if (preserveSelection)
	{
		GetSelectedDecryptPath(app, previousDecryptPath, ARRAYSIZE(previousDecryptPath));
		GetSelectedEncryptPath(app, previousEncryptPath, ARRAYSIZE(previousEncryptPath));
	} else if (app != NULL && app->StartupVolumePath[0] != L'\0') {
		if (app->StartupTab == FVE_GUI_TAB_ENCRYPT)
			StringCchCopyW(previousEncryptPath, ARRAYSIZE(previousEncryptPath), app->StartupVolumePath);
		else
			StringCchCopyW(previousDecryptPath, ARRAYSIZE(previousDecryptPath), app->StartupVolumePath);
	}

	SetBusy(app, TRUE, LoadTempString(IDS_REFRESHING_VOLUMES, L"Refreshing volumes..."));
	app->VolumeCount = 0;

	driveMask = GetLogicalDrives();
	for (WCHAR letter = L'A'; letter <= L'Z'; ++letter)
	{
		DWORD bit = 1u << (letter - L'A');
		if ((driveMask & bit) != 0)
			AddDriveVolume(app, letter);
	}

	RepopulateVolumeCombo(
		app,
		app->VolumeCombo,
		previousDecryptPath[0] != L'\0' ? previousDecryptPath : NULL,
		ShouldIncludeDecryptVolume);
	RepopulateVolumeCombo(
		app,
		app->EncryptVolumeCombo,
		previousEncryptPath[0] != L'\0' ? previousEncryptPath : NULL,
		ShouldIncludeEncryptVolume);
	if (app != NULL)
		app->StartupVolumePath[0] = L'\0';
	RefreshStatusText(app);

	SetBusy(app, FALSE, NULL);
	SetReadyMessage(app);
}

static void ClearSecretEdit(FVE_GUI_APP* app)
{
	WCHAR secret[FVE_GUI_MAX_SECRET];
	int length;

	if (app == NULL || app->SecretEdit == NULL)
		return;

	length = GetWindowTextW(app->SecretEdit, secret, ARRAYSIZE(secret));
	if (length > 0)
		SecureZeroMemory(secret, sizeof(secret));
	SetWindowTextW(app->SecretEdit, L"");
}

static void ClearEncryptPasswordEdits(FVE_GUI_APP* app)
{
	WCHAR secret[FVE_GUI_MAX_SECRET];
	int length;

	if (app == NULL)
		return;

	if (app->EncryptPasswordEdit != NULL)
	{
		length = GetWindowTextW(app->EncryptPasswordEdit, secret, ARRAYSIZE(secret));
		if (length > 0)
			SecureZeroMemory(secret, sizeof(secret));
		SetWindowTextW(app->EncryptPasswordEdit, L"");
	}

	if (app->EncryptConfirmEdit != NULL)
	{
		length = GetWindowTextW(app->EncryptConfirmEdit, secret, ARRAYSIZE(secret));
		if (length > 0)
			SecureZeroMemory(secret, sizeof(secret));
		SetWindowTextW(app->EncryptConfirmEdit, L"");
	}
}

static void ClearRecoveryKeyEdit(FVE_GUI_APP* app)
{
	WCHAR recoveryPassword[FVE_LIB_RECOVERY_PASSWORD_CCH];
	int length;

	if (app == NULL || app->RecoveryKeyEdit == NULL)
		return;

	length = GetWindowTextW(app->RecoveryKeyEdit, recoveryPassword, ARRAYSIZE(recoveryPassword));
	if (length > 0)
		SecureZeroMemory(recoveryPassword, sizeof(recoveryPassword));
	SetWindowTextW(app->RecoveryKeyEdit, L"");
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

static BOOL ReadEncryptPasswordPair(FVE_GUI_APP* app, PWSTR password, size_t cchPassword)
{
	WCHAR confirm[FVE_GUI_MAX_SECRET];
	BOOL ok = FALSE;

	if (GetWindowTextW(app->EncryptPasswordEdit, password, (int)cchPassword) <= 0 ||
		GetWindowTextW(app->EncryptConfirmEdit, confirm, ARRAYSIZE(confirm)) <= 0)
	{
		MessageBoxW(
			app->MainWindow,
			LoadTempString(IDS_MISSING_PASSWORD_MESSAGE, L"Enter and confirm the password first."),
			LoadTempString(IDS_MISSING_KEY_TITLE, L"Missing key"),
			MB_OK | MB_ICONWARNING);
		goto cleanup;
	}

	if (lstrcmpW(password, confirm) != 0)
	{
		MessageBoxW(
			app->MainWindow,
			LoadTempString(IDS_PASSWORD_MISMATCH_MESSAGE, L"The two passwords do not match."),
			LoadTempString(IDS_PASSWORD_MISMATCH_TITLE, L"Password mismatch"),
			MB_OK | MB_ICONWARNING);
		goto cleanup;
	}

	ok = TRUE;

cleanup:
	SecureZeroMemory(confirm, sizeof(confirm));
	if (!ok && password != NULL)
		SecureZeroMemory(password, cchPassword * sizeof(password[0]));
	return ok;
}

static void RunUnlock(FVE_GUI_APP* app)
{
	WCHAR volumePath[4];
	WCHAR secret[FVE_GUI_MAX_SECRET];
	BOOL recovery;
	HRESULT hr;

	if (!GetSelectedDecryptPath(app, volumePath, ARRAYSIZE(volumePath)) ||
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

	if (!GetSelectedDecryptPath(app, volumePath, ARRAYSIZE(volumePath)))
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
	const FVE_GUI_VOLUME_ENTRY* volume = GetSelectedDecryptVolume(app);
	WCHAR volumePath[4];
	HRESULT hr;

	if (volume == NULL || !GetSelectedDecryptPath(app, volumePath, ARRAYSIZE(volumePath)))
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

static void RunEncrypt(FVE_GUI_APP* app)
{
	const FVE_GUI_VOLUME_ENTRY* volume = GetSelectedEncryptVolume(app);
	WCHAR volumePath[4];
	WCHAR password[FVE_GUI_MAX_SECRET];
	WCHAR recoveryPassword[FVE_LIB_RECOVERY_PASSWORD_CCH];
	HRESULT hr;

	if (volume == NULL || !GetSelectedEncryptPath(app, volumePath, ARRAYSIZE(volumePath)) ||
		!ReadEncryptPasswordPair(app, password, ARRAYSIZE(password)))
		return;

	if (volume->HasStatus && SUCCEEDED(volume->StatusHr))
	{
		if (volume->Info.LockStatus == FVE_LIB_LOCK_LOCKED)
		{
			MessageBoxW(
				app->MainWindow,
				LoadTempString(IDS_UNLOCK_BEFORE_ENCRYPT_MESSAGE, L"Unlock the volume before turning on BitLocker."),
				LoadTempString(IDS_VOLUME_LOCKED_TITLE, L"Volume locked"),
				MB_OK | MB_ICONWARNING);
			SecureZeroMemory(password, sizeof(password));
			return;
		}
		if (volume->Info.VolumeStatus == FVE_LIB_VOLUME_FULLY_ENCRYPTED)
		{
			MessageBoxW(
				app->MainWindow,
				LoadTempString(IDS_ALREADY_ENCRYPTED_MESSAGE, L"The selected volume is already fully encrypted."),
				LoadTempString(IDS_BITLOCKER_TITLE, L"BitLocker"),
				MB_OK | MB_ICONINFORMATION);
			SecureZeroMemory(password, sizeof(password));
			return;
		}
		if (volume->Info.VolumeStatus == FVE_LIB_VOLUME_ENCRYPTION_IN_PROGRESS)
		{
			MessageBoxW(
				app->MainWindow,
				LoadTempString(IDS_ENCRYPTION_IN_PROGRESS_MESSAGE, L"BitLocker encryption is already in progress."),
				LoadTempString(IDS_BITLOCKER_TITLE, L"BitLocker"),
				MB_OK | MB_ICONINFORMATION);
			SecureZeroMemory(password, sizeof(password));
			return;
		}
	}

	if (MessageBoxW(
		app->MainWindow,
		LoadTempString(IDS_CONFIRM_ENCRYPT_MESSAGE, L"Turn on BitLocker for the selected volume?\r\n\r\nThis starts encryption and generates a recovery key."),
		LoadTempString(IDS_CONFIRM_ENCRYPT_TITLE, L"Confirm BitLocker turn on"),
		MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
	{
		SecureZeroMemory(password, sizeof(password));
		return;
	}

	ClearRecoveryKeyEdit(app);
	SetBusy(app, TRUE, LoadTempString(IDS_STARTING_ENCRYPTION, L"Starting encryption..."));
	hr = FveLibEncryptWithPasswordByPath(
		volumePath,
		password,
		recoveryPassword,
		ARRAYSIZE(recoveryPassword));
	SecureZeroMemory(password, sizeof(password));
	ClearEncryptPasswordEdits(app);

	if (FAILED(hr))
	{
		SecureZeroMemory(recoveryPassword, sizeof(recoveryPassword));
		SetBusy(app, FALSE, LoadTempString(IDS_ENCRYPT_FAILED, L"Encryption failed."));
		ShowHrError(app->MainWindow, LoadTempString(IDS_OPERATION_ENCRYPT, L"Turn on BitLocker"), hr);
		RefreshVolumes(app, TRUE);
		return;
	}

	SetWindowTextW(app->RecoveryKeyEdit, recoveryPassword);
	SecureZeroMemory(recoveryPassword, sizeof(recoveryPassword));
	SetBusy(app, FALSE, LoadTempString(IDS_ENCRYPTION_STARTED, L"BitLocker encryption started. Save or copy the recovery key."));
	RefreshVolumes(app, TRUE);
	SetMessage(app, LoadTempString(IDS_ENCRYPTION_STARTED, L"BitLocker encryption started. Save or copy the recovery key."));
}

static void CopyRecoveryKey(FVE_GUI_APP* app)
{
	int length;
	HGLOBAL memory;
	PWSTR target;
	WCHAR recoveryPassword[FVE_LIB_RECOVERY_PASSWORD_CCH];

	if (app == NULL || app->RecoveryKeyEdit == NULL)
		return;

	length = GetWindowTextW(app->RecoveryKeyEdit, recoveryPassword, ARRAYSIZE(recoveryPassword));
	if (length <= 0)
		return;

	if (!OpenClipboard(app->MainWindow))
	{
		ShowHrError(app->MainWindow, LoadTempString(IDS_OPERATION_COPY_RECOVERY, L"Copy recovery key"), HRESULT_FROM_WIN32(GetLastError()));
		SecureZeroMemory(recoveryPassword, sizeof(recoveryPassword));
		return;
	}

	memory = GlobalAlloc(GMEM_MOVEABLE, ((SIZE_T)length + 1u) * sizeof(WCHAR));
	if (memory == NULL)
	{
		CloseClipboard();
		ShowHrError(app->MainWindow, LoadTempString(IDS_OPERATION_COPY_RECOVERY, L"Copy recovery key"), HRESULT_FROM_WIN32(GetLastError()));
		SecureZeroMemory(recoveryPassword, sizeof(recoveryPassword));
		return;
	}

	target = (PWSTR)GlobalLock(memory);
	if (target == NULL)
	{
		GlobalFree(memory);
		CloseClipboard();
		ShowHrError(app->MainWindow, LoadTempString(IDS_OPERATION_COPY_RECOVERY, L"Copy recovery key"), HRESULT_FROM_WIN32(GetLastError()));
		SecureZeroMemory(recoveryPassword, sizeof(recoveryPassword));
		return;
	}

	StringCchCopyW(target, (size_t)length + 1u, recoveryPassword);
	GlobalUnlock(memory);
	EmptyClipboard();
	if (SetClipboardData(CF_UNICODETEXT, memory) == NULL)
	{
		GlobalFree(memory);
		CloseClipboard();
		ShowHrError(app->MainWindow, LoadTempString(IDS_OPERATION_COPY_RECOVERY, L"Copy recovery key"), HRESULT_FROM_WIN32(GetLastError()));
		SecureZeroMemory(recoveryPassword, sizeof(recoveryPassword));
		return;
	}

	CloseClipboard();
	SecureZeroMemory(recoveryPassword, sizeof(recoveryPassword));
	SetMessage(app, LoadTempString(IDS_RECOVERY_KEY_COPIED, L"Recovery key copied."));
}

static void ShowControl(HWND control, BOOL visible)
{
	if (control != NULL)
		ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
}

static void UpdateTabVisibility(FVE_GUI_APP* app)
{
	BOOL decryptVisible;
	BOOL encryptVisible;

	if (app == NULL)
		return;

	decryptVisible = app->ActiveTab == FVE_GUI_TAB_DECRYPT;
	encryptVisible = app->ActiveTab == FVE_GUI_TAB_ENCRYPT;

	ShowControl(app->VolumeLabel, decryptVisible);
	ShowControl(app->VolumeCombo, decryptVisible);
	ShowControl(app->RefreshButton, decryptVisible);
	ShowControl(app->SecretLabel, decryptVisible);
	ShowControl(app->SecretEdit, decryptVisible);
	ShowControl(app->RecoveryCheck, decryptVisible);
	ShowControl(app->UnlockButton, decryptVisible);
	ShowControl(app->LockButton, decryptVisible);
	ShowControl(app->DecryptButton, decryptVisible);
	ShowControl(app->DecryptStatusStatic, decryptVisible);

	ShowControl(app->EncryptVolumeLabel, encryptVisible);
	ShowControl(app->EncryptVolumeCombo, encryptVisible);
	ShowControl(app->EncryptRefreshButton, encryptVisible);
	ShowControl(app->EncryptPasswordLabel, encryptVisible);
	ShowControl(app->EncryptPasswordEdit, encryptVisible);
	ShowControl(app->EncryptConfirmLabel, encryptVisible);
	ShowControl(app->EncryptConfirmEdit, encryptVisible);
	ShowControl(app->EncryptButton, encryptVisible);
	ShowControl(app->RecoveryKeyLabel, encryptVisible);
	ShowControl(app->RecoveryKeyEdit, encryptVisible);
	ShowControl(app->CopyRecoveryButton, encryptVisible);
}

static void SetActiveTab(FVE_GUI_APP* app, int activeTab)
{
	if (app == NULL)
		return;

	if (activeTab != FVE_GUI_TAB_ENCRYPT)
		activeTab = FVE_GUI_TAB_DECRYPT;

	app->ActiveTab = activeTab;
	if (app->TabControl != NULL)
		SendMessageW(app->TabControl, TCM_SETCURSEL, (WPARAM)activeTab, 0);
	UpdateTabVisibility(app);
	UpdateButtons(app);
	SetReadyMessage(app);
}

static void AddTabItem(HWND tabControl, int index, PWSTR text)
{
	TCITEMW item;

	ZeroMemory(&item, sizeof(item));
	item.mask = TCIF_TEXT;
	item.pszText = text;
	SendMessageW(tabControl, TCM_INSERTITEMW, (WPARAM)index, (LPARAM)&item);
}

static void LayoutControls(HWND window, FVE_GUI_APP* app)
{
	RECT client;
	RECT tabRect;
	RECT pageRect;
	int margin;
	int gap;
	int contentLeft;
	int contentWidth;
	int maxContentWidth;
	int labelWidth;
	int topRowHeight;
	int buttonHeight;
	int rowHeight;
	int top;
	int keyTop;
	int buttonsTop;
	int decryptStatusTop;
	int decryptStatusHeight;
	int tabTop;
	int tabBottom;
	int pageLeft;
	int pageTop;
	int pageRight;
	int pageBottom;
	int pageMargin;
	int passwordTop;
	int confirmTop;
	int encryptButtonTop;
	int recoveryTop;
	int messageTop;
	int statusLabelWidth;
	int clientWidth;
	int clientHeight;
	int comboWidth;
	int recoveryWidth;
	int refreshWidth;
	int fieldLeft;
	int rightColumnLeft;
	int rightColumnWidth;
	int unlockWidth;
	int lockWidth;
	int decryptWidth;
	int actionLeft;
	int actionAreaWidth;
	int actionGap;
	int actionTotalWidth;

	GetClientRect(window, &client);
	clientWidth = client.right - client.left;
	clientHeight = client.bottom - client.top;

	margin = ScaleForWindow(window, 22);
	gap = ScaleForWindow(window, 12);
	labelWidth = ScaleForWindow(window, 92);
	topRowHeight = ScaleForWindow(window, 24);
	buttonHeight = ScaleForWindow(window, 30);
	rowHeight = ScaleForWindow(window, 30);
	rightColumnWidth = ScaleForWindow(window, 112);
	recoveryWidth = rightColumnWidth;
	refreshWidth = rightColumnWidth;
	statusLabelWidth = ScaleForWindow(window, 56);
	unlockWidth = ScaleForWindow(window, 112);
	lockWidth = ScaleForWindow(window, 112);
	decryptWidth = ScaleForWindow(window, 170);
	actionGap = gap;

	contentWidth = clientWidth - margin * 2;
	maxContentWidth = ScaleForWindow(window, 620);
	if (contentWidth > maxContentWidth)
	{
		contentWidth = maxContentWidth;
		contentLeft = (clientWidth - contentWidth) / 2;
	} else {
		contentLeft = margin;
	}

	tabTop = ScaleForWindow(window, 12);
	messageTop = clientHeight - margin - rowHeight;
	tabBottom = messageTop - gap;
	if (tabBottom < tabTop + ScaleForWindow(window, 210))
		tabBottom = tabTop + ScaleForWindow(window, 210);

	MoveWindow(app->TabControl, contentLeft, tabTop, contentWidth, tabBottom - tabTop, TRUE);
	SetWindowPos(app->TabControl, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	tabRect.left = contentLeft;
	tabRect.top = tabTop;
	tabRect.right = contentLeft + contentWidth;
	tabRect.bottom = tabBottom;
	pageRect = tabRect;
	SendMessageW(app->TabControl, TCM_ADJUSTRECT, FALSE, (LPARAM)&pageRect);
	pageMargin = ScaleForWindow(window, 14);
	pageLeft = pageRect.left + pageMargin;
	pageTop = pageRect.top + ScaleForWindow(window, 14);
	pageRight = pageRect.right - pageMargin;
	pageBottom = pageRect.bottom - ScaleForWindow(window, 10);

	top = pageTop;
	buttonsTop = pageBottom - buttonHeight;
	keyTop = buttonsTop - ScaleForWindow(window, 26) - rowHeight;
	if (keyTop < top + topRowHeight + ScaleForWindow(window, 34))
		keyTop = top + topRowHeight + ScaleForWindow(window, 34);

	MoveWindow(app->VolumeLabel, pageLeft, top + ScaleForWindow(window, 3), labelWidth, topRowHeight, TRUE);

	fieldLeft = pageLeft + labelWidth;
	rightColumnLeft = pageRight - rightColumnWidth;
	comboWidth = rightColumnLeft - gap - fieldLeft;
	if (comboWidth < ScaleForWindow(window, 220))
		comboWidth = ScaleForWindow(window, 220);
	MoveWindow(app->VolumeCombo, fieldLeft, top, comboWidth, ScaleForWindow(window, 220), TRUE);
	MoveWindow(app->RefreshButton, rightColumnLeft, top, refreshWidth, topRowHeight, TRUE);

	decryptStatusTop = top + ScaleForWindow(window, 44);
	decryptStatusHeight = keyTop - decryptStatusTop - ScaleForWindow(window, 8);
	if (decryptStatusHeight < rowHeight)
		decryptStatusHeight = rowHeight;
	MoveWindow(app->DecryptStatusStatic, fieldLeft, decryptStatusTop, pageRight - fieldLeft, decryptStatusHeight, TRUE);

	MoveWindow(app->SecretLabel, pageLeft, keyTop + ScaleForWindow(window, 4), labelWidth, rowHeight, TRUE);
	MoveWindow(app->SecretEdit, fieldLeft, keyTop, comboWidth, topRowHeight, TRUE);
	MoveWindow(app->RecoveryCheck, rightColumnLeft, keyTop + ScaleForWindow(window, 2), recoveryWidth, rowHeight, TRUE);

	actionLeft = fieldLeft;
	actionAreaWidth = pageRight - fieldLeft;
	actionTotalWidth = unlockWidth + lockWidth + decryptWidth + actionGap * 2;
	if (actionTotalWidth > actionAreaWidth)
	{
		actionGap = ScaleForWindow(window, 8);
		decryptWidth = actionAreaWidth - unlockWidth - lockWidth - actionGap * 2;
		if (decryptWidth < ScaleForWindow(window, 142))
			decryptWidth = ScaleForWindow(window, 142);
	}

	MoveWindow(app->UnlockButton, actionLeft, buttonsTop, unlockWidth, buttonHeight, TRUE);
	MoveWindow(app->LockButton, actionLeft + unlockWidth + actionGap, buttonsTop, lockWidth, buttonHeight, TRUE);
	MoveWindow(app->DecryptButton, actionLeft + unlockWidth + actionGap + lockWidth + actionGap, buttonsTop, decryptWidth, buttonHeight, TRUE);

	MoveWindow(app->EncryptVolumeLabel, pageLeft, top + ScaleForWindow(window, 3), labelWidth, topRowHeight, TRUE);
	MoveWindow(app->EncryptVolumeCombo, fieldLeft, top, comboWidth, ScaleForWindow(window, 220), TRUE);
	MoveWindow(app->EncryptRefreshButton, rightColumnLeft, top, refreshWidth, topRowHeight, TRUE);

	passwordTop = top + ScaleForWindow(window, 44);
	confirmTop = passwordTop + ScaleForWindow(window, 38);
	encryptButtonTop = confirmTop + ScaleForWindow(window, 44);
	recoveryTop = encryptButtonTop + ScaleForWindow(window, 48);
	if (recoveryTop + topRowHeight > pageBottom)
		recoveryTop = pageBottom - topRowHeight;
	if (encryptButtonTop + buttonHeight > recoveryTop - gap)
		encryptButtonTop = recoveryTop - gap - buttonHeight;

	MoveWindow(app->EncryptPasswordLabel, pageLeft, passwordTop + ScaleForWindow(window, 4), labelWidth, rowHeight, TRUE);
	MoveWindow(app->EncryptPasswordEdit, fieldLeft, passwordTop, comboWidth, topRowHeight, TRUE);
	MoveWindow(app->EncryptConfirmLabel, pageLeft, confirmTop + ScaleForWindow(window, 4), labelWidth, rowHeight, TRUE);
	MoveWindow(app->EncryptConfirmEdit, fieldLeft, confirmTop, comboWidth, topRowHeight, TRUE);
	MoveWindow(app->EncryptButton, fieldLeft, encryptButtonTop, ScaleForWindow(window, 170), buttonHeight, TRUE);
	MoveWindow(app->RecoveryKeyLabel, pageLeft, recoveryTop + ScaleForWindow(window, 4), labelWidth, rowHeight, TRUE);
	MoveWindow(app->RecoveryKeyEdit, fieldLeft, recoveryTop, comboWidth, topRowHeight, TRUE);
	MoveWindow(app->CopyRecoveryButton, rightColumnLeft, recoveryTop, recoveryWidth, topRowHeight, TRUE);

	MoveWindow(app->StatusLabel, contentLeft, messageTop + ScaleForWindow(window, 4), statusLabelWidth, rowHeight, TRUE);
	MoveWindow(app->MessageStatic, contentLeft + statusLabelWidth, messageTop + ScaleForWindow(window, 4), contentWidth - statusLabelWidth, rowHeight, TRUE);
	UpdateTabVisibility(app);
}

static BOOL CreateControls(FVE_GUI_APP* app)
{
	app->Font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	app->TabControl = CreateChildWindow(app, WC_TABCONTROLW, L"", WS_TABSTOP | WS_CLIPSIBLINGS, 0, IDC_TAB_CONTROL);
	if (app->TabControl != NULL)
	{
		AddTabItem(app->TabControl, FVE_GUI_TAB_DECRYPT, app->Text.DecryptTab);
		AddTabItem(app->TabControl, FVE_GUI_TAB_ENCRYPT, app->Text.EncryptTab);
	}
	app->VolumeLabel = CreateChildWindow(app, L"STATIC", app->Text.VolumeLabel, 0, 0, -1);
	app->VolumeCombo = CreateChildWindow(app, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 0, IDC_VOLUME_COMBO);
	app->RefreshButton = CreateChildWindow(app, L"BUTTON", app->Text.RefreshButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_REFRESH_BUTTON);
	app->SecretLabel = CreateChildWindow(app, L"STATIC", app->Text.SecretLabel, 0, 0, -1);
	app->SecretEdit = CreateChildWindow(app, L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_SECRET_EDIT);
	app->RecoveryCheck = CreateChildWindow(app, L"BUTTON", app->Text.RecoveryCheck, BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_RECOVERY_CHECK);
	app->UnlockButton = CreateChildWindow(app, L"BUTTON", app->Text.UnlockButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_UNLOCK_BUTTON);
	app->LockButton = CreateChildWindow(app, L"BUTTON", app->Text.LockButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_LOCK_BUTTON);
	app->DecryptButton = CreateChildWindow(app, L"BUTTON", app->Text.TurnOffButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_DECRYPT_BUTTON);
	app->DecryptStatusStatic = CreateChildWindow(app, L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 0, IDC_DECRYPT_STATUS_STATIC);
	app->EncryptVolumeLabel = CreateChildWindow(app, L"STATIC", app->Text.VolumeLabel, 0, 0, -1);
	app->EncryptVolumeCombo = CreateChildWindow(app, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 0, IDC_ENCRYPT_VOLUME_COMBO);
	app->EncryptRefreshButton = CreateChildWindow(app, L"BUTTON", app->Text.RefreshButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_ENCRYPT_REFRESH_BUTTON);
	app->EncryptPasswordLabel = CreateChildWindow(app, L"STATIC", app->Text.EncryptPasswordLabel, 0, 0, -1);
	app->EncryptPasswordEdit = CreateChildWindow(app, L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_ENCRYPT_PASSWORD_EDIT);
	app->EncryptConfirmLabel = CreateChildWindow(app, L"STATIC", app->Text.ConfirmPasswordLabel, 0, 0, -1);
	app->EncryptConfirmEdit = CreateChildWindow(app, L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_ENCRYPT_CONFIRM_EDIT);
	app->EncryptButton = CreateChildWindow(app, L"BUTTON", app->Text.EncryptButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_ENCRYPT_BUTTON);
	app->RecoveryKeyLabel = CreateChildWindow(app, L"STATIC", app->Text.RecoveryKeyLabel, 0, 0, -1);
	app->RecoveryKeyEdit = CreateChildWindow(app, L"EDIT", L"", ES_AUTOHSCROLL | ES_READONLY | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_RECOVERY_KEY_EDIT);
	app->CopyRecoveryButton = CreateChildWindow(app, L"BUTTON", app->Text.CopyButton, BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_COPY_RECOVERY_BUTTON);
	app->StatusLabel = CreateChildWindow(app, L"STATIC", app->Text.StatusLabel, 0, 0, IDC_STATUS_LABEL);
	app->MessageStatic = CreateChildWindow(app, L"STATIC", L"", 0, 0, IDC_MESSAGE_STATIC);

	return app->TabControl != NULL &&
		app->VolumeLabel != NULL &&
		app->VolumeCombo != NULL &&
		app->RefreshButton != NULL &&
		app->SecretLabel != NULL &&
		app->SecretEdit != NULL &&
		app->RecoveryCheck != NULL &&
		app->UnlockButton != NULL &&
		app->LockButton != NULL &&
		app->DecryptButton != NULL &&
		app->DecryptStatusStatic != NULL &&
		app->EncryptVolumeLabel != NULL &&
		app->EncryptVolumeCombo != NULL &&
		app->EncryptRefreshButton != NULL &&
		app->EncryptPasswordLabel != NULL &&
		app->EncryptPasswordEdit != NULL &&
		app->EncryptConfirmLabel != NULL &&
		app->EncryptConfirmEdit != NULL &&
		app->EncryptButton != NULL &&
		app->RecoveryKeyLabel != NULL &&
		app->RecoveryKeyEdit != NULL &&
		app->CopyRecoveryButton != NULL &&
		app->StatusLabel != NULL &&
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
		app->ActiveTab = app->StartupTab == FVE_GUI_TAB_ENCRYPT ?
			FVE_GUI_TAB_ENCRYPT : FVE_GUI_TAB_DECRYPT;
		SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)app);
		if (!CreateControls(app))
			return -1;
		SetActiveTab(app, app->ActiveTab);
		LayoutControls(window, app);
		UpdateTabBodyBrush(app);
		RefreshVolumes(app, FALSE);
		return 0;
	}
	case WM_SIZE:
		if (app != NULL)
		{
			LayoutControls(window, app);
			UpdateTabBodyBrush(app);
		}
		return 0;
	case WM_GETMINMAXINFO:
	{
		MINMAXINFO* minMax = (MINMAXINFO*)lParam;
		minMax->ptMinTrackSize.x = ScaleForWindow(window, 560);
		minMax->ptMinTrackSize.y = ScaleForWindow(window, 340);
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
			if (control == app->RecoveryKeyEdit)
				return (LRESULT)PrepareWindowControlBackground((HDC)wParam);
			if (control == app->VolumeLabel ||
				control == app->SecretLabel ||
				control == app->RecoveryCheck ||
				control == app->DecryptStatusStatic ||
				control == app->EncryptVolumeLabel ||
				control == app->EncryptPasswordLabel ||
				control == app->EncryptConfirmLabel ||
				control == app->RecoveryKeyLabel)
			{
				return (LRESULT)PrepareTabControlBackground((HDC)wParam, app->TabBodyBrush);
			}
			if (control == app->StatusLabel ||
				control == app->MessageStatic)
			{
				SetTextColor((HDC)wParam, GetSysColor(COLOR_WINDOWTEXT));
				SetBkMode((HDC)wParam, TRANSPARENT);
				return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
			}
		}
		break;
	case WM_CTLCOLOREDIT:
		if (app != NULL)
		{
			HWND control = (HWND)lParam;
			if (control == app->SecretEdit ||
				control == app->EncryptPasswordEdit ||
				control == app->EncryptConfirmEdit ||
				control == app->RecoveryKeyEdit)
			{
				return (LRESULT)PrepareWindowControlBackground((HDC)wParam);
			}
		}
		break;
	case WM_CTLCOLORBTN:
		if (app != NULL && (HWND)lParam == app->RecoveryCheck)
			return (LRESULT)PrepareTabControlBackground((HDC)wParam, app->TabBodyBrush);
		break;
	case WM_NOTIFY:
		if (app != NULL)
		{
			NMHDR* notify = (NMHDR*)lParam;
			if (notify != NULL &&
				notify->hwndFrom == app->TabControl &&
				notify->code == TCN_SELCHANGE)
			{
				LRESULT selection = SendMessageW(app->TabControl, TCM_GETCURSEL, 0, 0);
				SetActiveTab(app, selection == FVE_GUI_TAB_ENCRYPT ? FVE_GUI_TAB_ENCRYPT : FVE_GUI_TAB_DECRYPT);
				return 0;
			}
		}
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
		case IDC_ENCRYPT_VOLUME_COMBO:
			if (HIWORD(wParam) == CBN_SELCHANGE)
				RefreshStatusText(app);
			return 0;
		case IDC_ENCRYPT_PASSWORD_EDIT:
		case IDC_ENCRYPT_CONFIRM_EDIT:
		case IDC_RECOVERY_KEY_EDIT:
			if (HIWORD(wParam) == EN_CHANGE)
				UpdateButtons(app);
			return 0;
		case IDC_REFRESH_BUTTON:
		case IDC_ENCRYPT_REFRESH_BUTTON:
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
		case IDC_ENCRYPT_BUTTON:
			RunEncrypt(app);
			return 0;
		case IDC_COPY_RECOVERY_BUTTON:
			CopyRecoveryKey(app);
			return 0;
		default:
			break;
		}
		break;
	case WM_DESTROY:
		ClearSecretEdit(&gApp);
		ClearEncryptPasswordEdits(&gApp);
		ClearRecoveryKeyEdit(&gApp);
		if (gApp.TabBodyBrush != NULL)
		{
			DeleteObject(gApp.TabBodyBrush);
			gApp.TabBodyBrush = NULL;
		}
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
	INITCOMMONCONTROLSEX commonControls;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	ZeroMemory(&commonControls, sizeof(commonControls));
	commonControls.dwSize = sizeof(commonControls);
	commonControls.dwICC = ICC_TAB_CLASSES;
	InitCommonControlsEx(&commonControls);

	ZeroMemory(&gApp, sizeof(gApp));
	gApp.Instance = hInstance;
	gApp.Elevated = IsProcessElevated();
	ParseStartupOptions(&gApp);
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
		ScaleForWindow(NULL, 700),
		ScaleForWindow(NULL, 360),
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
