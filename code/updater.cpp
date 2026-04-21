// Filename:-	updater.cpp
//
// See updater.h. Uses WinINet + a light JSON scrape - the GitHub releases
// endpoint has a stable field layout so we don't need a full JSON parser.

#include "stdafx.h"
#include "includes.h"
#include "updater.h"
#include "resource.h"
#include "modview_version.h"

#include <wininet.h>
#include <shellapi.h>
#include <process.h>

#pragma comment(lib, "wininet.lib")

// Configurable via #define if this fork moves elsewhere.
#ifndef MODVIEW_GITHUB_OWNER
#define MODVIEW_GITHUB_OWNER	"Milamber0"
#endif
#ifndef MODVIEW_GITHUB_REPO
#define MODVIEW_GITHUB_REPO		"ModView"
#endif

// Profile section / entry used to persist the "skip this version" choice.
// MFC routes WriteProfileString to HKEY_CURRENT_USER\Software\<AppName>
// because we never called SetRegistryKey to migrate to a company sub-key,
// which is fine for an app like ModView that doesn't live alongside others.
#define UPDATER_PROFILE_SECTION	"Updater"
#define UPDATER_PROFILE_SKIPTAG	"SkipVersion"

static CString Updater_LoadSkippedVersion()
{
	return AfxGetApp()->GetProfileString(UPDATER_PROFILE_SECTION,
										 UPDATER_PROFILE_SKIPTAG, "");
}

static void Updater_SaveSkippedVersion(const char *psTag)
{
	AfxGetApp()->WriteProfileString(UPDATER_PROFILE_SECTION,
									UPDATER_PROFILE_SKIPTAG, psTag);
}


// =============================================================================
// Version comparison
// =============================================================================

// Consumes leading digits into outNum; returns a pointer to the first
// non-digit (or '\0'). Does NOT consume the separator, caller does that.
static const char *parse_uint(const char *s, unsigned int *outNum)
{
	unsigned int v = 0;
	while (*s >= '0' && *s <= '9') {
		v = v * 10 + (unsigned int)(*s - '0');
		s++;
	}
	*outNum = v;
	return s;
}

bool Updater_IsNewerThan(const char *sNew, const char *sCurrent)
{
	if (!sNew || !sCurrent) return false;

	// Tags may be prefixed with 'v' in some projects - tolerate that.
	if (*sNew == 'v' || *sNew == 'V') sNew++;
	if (*sCurrent == 'v' || *sCurrent == 'V') sCurrent++;

	for (int component = 0; component < 4; component++) {
		unsigned int n = 0, c = 0;
		sNew     = parse_uint(sNew, &n);
		sCurrent = parse_uint(sCurrent, &c);

		if (n > c) return true;
		if (n < c) return false;

		// numerics equal so far - check separators
		bool newHasMore = (*sNew == '.');
		bool curHasMore = (*sCurrent == '.');

		if (newHasMore) sNew++;
		if (curHasMore) sCurrent++;

		if (!newHasMore && !curHasMore) break;
		// else keep comparing the next component (missing components implicitly 0)
	}

	// Numeric prefixes identical. Apply suffix rule: anything with a
	// non-digit-starting suffix (e.g. "-dev", "-beta") is OLDER than the
	// same numeric prefix with no suffix. This keeps "3.0-dev" from
	// claiming parity with "3.0".
	bool newHasSuffix = (*sNew != '\0');
	bool curHasSuffix = (*sCurrent != '\0');
	if (newHasSuffix && !curHasSuffix) return false;	// new has "-dev", current is clean
	if (!newHasSuffix && curHasSuffix) return true;		// new is clean, current has "-dev"
	return false;										// both clean or both suffixed - treat as equal
}


// =============================================================================
// Tiny JSON value extractor
// =============================================================================

// Finds "\"<key>\"" in src, then scans forward to the next JSON string value
// and copies up to outSize-1 chars into out. Escapes aren't un-escaped beyond
// literal backslashes - sufficient for the URL / tag fields we need.
static bool json_find_string(const char *src, const char *key, char *out, int outSize)
{
	if (!src || !key || !out || outSize <= 0) return false;
	out[0] = '\0';

	// Build "\"<key>\"" to disambiguate from substring matches.
	char sNeedle[128];
	_snprintf(sNeedle, sizeof(sNeedle) - 1, "\"%s\"", key);
	sNeedle[sizeof(sNeedle) - 1] = '\0';

	const char *p = strstr(src, sNeedle);
	if (!p) return false;

	p += strlen(sNeedle);
	// Skip : and whitespace
	while (*p && (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
	if (*p != '"') return false;
	p++;

	int w = 0;
	while (*p && *p != '"' && w < outSize - 1) {
		if (*p == '\\' && *(p + 1)) {
			// Unescape simple things we actually see from GitHub: \/ -> /, \" -> "
			if (*(p + 1) == '/' || *(p + 1) == '"' || *(p + 1) == '\\') {
				out[w++] = *(p + 1);
				p += 2;
				continue;
			}
		}
		out[w++] = *p++;
	}
	out[w] = '\0';
	return true;
}


// Extract the first assets[] entry's browser_download_url / name.
static bool json_find_first_asset(const char *src, char *outUrl, int outUrlSize,
								   char *outName, int outNameSize)
{
	const char *pAssets = strstr(src, "\"assets\"");
	if (!pAssets) return false;
	// Find the first '{' after the "assets": [ marker
	const char *pArr = strchr(pAssets, '[');
	if (!pArr) return false;
	const char *pFirst = strchr(pArr, '{');
	if (!pFirst) return false;

	// Bound the search to the first asset object (find matching '}')
	int depth = 1;
	const char *pEnd = pFirst + 1;
	while (*pEnd && depth > 0) {
		if (*pEnd == '{') depth++;
		else if (*pEnd == '}') depth--;
		if (depth == 0) break;
		pEnd++;
	}

	int len = (int)(pEnd - pFirst);
	if (len <= 0 || len > 32768) return false;

	// Copy into a scratch buffer so the key searches don't wander past
	// the first asset.
	char *scratch = (char *)malloc(len + 1);
	if (!scratch) return false;
	memcpy(scratch, pFirst, len);
	scratch[len] = '\0';

	bool ok = json_find_string(scratch, "browser_download_url", outUrl, outUrlSize);
	json_find_string(scratch, "name", outName, outNameSize);

	free(scratch);
	return ok;
}


// =============================================================================
// HTTP GET via WinINet
// =============================================================================

// Distinguishes network/connection failures from HTTP status failures so
// callers can render a useful message to the user. See http_get.
enum HttpError {
	HTTP_OK = 0,
	HTTP_ERR_NETWORK = 1,		// DNS, TLS, connection refused, etc.
	HTTP_ERR_STATUS = 2,		// got a response but status != 200
};

// Returns a heap-allocated, null-terminated buffer on success (caller frees
// with free()). On any failure returns NULL and writes details to outErr /
// outStatus / outLastError. outStatus is populated for HTTP_ERR_STATUS.
static char *http_get(const char *psUrl, const char *psUserAgent,
					  HttpError *outErr, DWORD *outStatus, DWORD *outLastError)
{
	if (outErr)       *outErr       = HTTP_ERR_NETWORK;
	if (outStatus)    *outStatus    = 0;
	if (outLastError) *outLastError = 0;

	HINTERNET hSession = InternetOpenA(psUserAgent, INTERNET_OPEN_TYPE_PRECONFIG,
									   NULL, NULL, 0);
	if (!hSession) {
		if (outLastError) *outLastError = GetLastError();
		return NULL;
	}

	// "Accept: application/vnd.github+json" gets us the stable v3 shape
	// regardless of GitHub's future default version bumps.
	const char *psHeaders = "Accept: application/vnd.github+json\r\n";

	HINTERNET hUrl = InternetOpenUrlA(hSession, psUrl, psHeaders,
									   (DWORD)strlen(psHeaders),
									   INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES |
									   INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE,
									   0);
	if (!hUrl) {
		if (outLastError) *outLastError = GetLastError();
		InternetCloseHandle(hSession);
		return NULL;
	}

	// Read the status code first so callers can distinguish "404 no releases"
	// from a real transport failure.
	DWORD dwStatus = 0;
	DWORD dwStatusSize = sizeof(dwStatus);
	if (HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
						&dwStatus, &dwStatusSize, NULL)) {
		if (outStatus) *outStatus = dwStatus;
		if (dwStatus != 200) {
			if (outErr) *outErr = HTTP_ERR_STATUS;
			InternetCloseHandle(hUrl);
			InternetCloseHandle(hSession);
			return NULL;
		}
	}

	// Read body into a growing buffer.
	int cap = 16384, used = 0;
	char *buf = (char *)malloc(cap);
	if (!buf) {
		InternetCloseHandle(hUrl);
		InternetCloseHandle(hSession);
		return NULL;
	}

	for (;;) {
		if (used + 8192 > cap) {
			cap *= 2;
			char *nb = (char *)realloc(buf, cap);
			if (!nb) { free(buf); buf = NULL; break; }
			buf = nb;
		}
		DWORD dwRead = 0;
		BOOL ok = InternetReadFile(hUrl, buf + used, 8192, &dwRead);
		if (!ok) {
			if (outLastError) *outLastError = GetLastError();
			free(buf); buf = NULL;
			break;
		}
		if (dwRead == 0) break;
		used += (int)dwRead;
	}

	InternetCloseHandle(hUrl);
	InternetCloseHandle(hSession);

	if (buf) {
		char *nb = (char *)realloc(buf, used + 1);
		if (nb) buf = nb;
		buf[used] = '\0';
		if (outErr) *outErr = HTTP_OK;
	}
	return buf;
}


// =============================================================================
// Public: fetch latest release
// =============================================================================

bool Updater_FetchLatest(UpdaterLatestRelease *pOut, char *outError, int outErrorSize)
{
	if (!pOut) return false;
	memset(pOut, 0, sizeof(*pOut));

	auto setErr = [&](const char *s) {
		if (outError && outErrorSize > 0) {
			strncpy(outError, s, outErrorSize - 1);
			outError[outErrorSize - 1] = '\0';
		}
	};

	char sUrl[512];
	_snprintf(sUrl, sizeof(sUrl) - 1,
			  "https://api.github.com/repos/%s/%s/releases/latest",
			  MODVIEW_GITHUB_OWNER, MODVIEW_GITHUB_REPO);
	sUrl[sizeof(sUrl) - 1] = '\0';

	char sUserAgent[64];
	_snprintf(sUserAgent, sizeof(sUserAgent) - 1,
			  "ModView/%s", MODVIEW_VERSION_STRING);
	sUserAgent[sizeof(sUserAgent) - 1] = '\0';

	HttpError httpErr = HTTP_OK;
	DWORD dwStatus = 0;
	DWORD dwLastError = 0;
	char *psJson = http_get(sUrl, sUserAgent, &httpErr, &dwStatus, &dwLastError);
	if (!psJson) {
		// Separate "no release yet" and "real network failure" for the
		// user - 404 from releases/latest means the repo exists but has
		// no published releases yet (the workflow hasn't run, or all
		// releases are draft / pre-release).
		if (httpErr == HTTP_ERR_STATUS && dwStatus == 404) {
			char sMsg[256];
			_snprintf(sMsg, sizeof(sMsg) - 1,
					  "No releases published yet for %s/%s.",
					  MODVIEW_GITHUB_OWNER, MODVIEW_GITHUB_REPO);
			sMsg[sizeof(sMsg) - 1] = '\0';
			setErr(sMsg);
		} else if (httpErr == HTTP_ERR_STATUS && dwStatus == 403) {
			setErr("GitHub rate-limited the update check. Try again later.");
		} else if (httpErr == HTTP_ERR_STATUS) {
			char sMsg[128];
			_snprintf(sMsg, sizeof(sMsg) - 1,
					  "GitHub returned HTTP %u.", dwStatus);
			sMsg[sizeof(sMsg) - 1] = '\0';
			setErr(sMsg);
		} else {
			char sMsg[256];
			_snprintf(sMsg, sizeof(sMsg) - 1,
					  "Could not reach GitHub. Check your network connection. (WinINet error %u)",
					  dwLastError);
			sMsg[sizeof(sMsg) - 1] = '\0';
			setErr(sMsg);
		}
		return false;
	}

	bool bOkTag      = json_find_string(psJson, "tag_name", pOut->sTagName, sizeof(pOut->sTagName));
	bool bOkRelUrl   = json_find_string(psJson, "html_url", pOut->sReleaseUrl, sizeof(pOut->sReleaseUrl));
	bool bOkAsset    = json_find_first_asset(psJson, pOut->sAssetUrl, sizeof(pOut->sAssetUrl),
											  pOut->sAssetName, sizeof(pOut->sAssetName));

	free(psJson);

	if (!bOkTag || !bOkRelUrl) {
		setErr("GitHub response could not be parsed.");
		return false;
	}
	// sAssetUrl being empty is OK here - user can still go to the release page.
	(void)bOkAsset;
	return true;
}


// =============================================================================
// Self-install
// =============================================================================

static void get_exe_path(char *out, int outSize)
{
	GetModuleFileNameA(NULL, out, outSize);
}

static void get_temp_dir(char *out, int outSize)
{
	GetTempPathA(outSize, out);
}

// Download the asset .zip to a temp path. Returns true on success and writes
// the temp zip path into outZipPath.
static bool download_asset(const char *psUrl, char *outZipPath, int outZipSize)
{
	char sTemp[MAX_PATH];
	get_temp_dir(sTemp, sizeof(sTemp));
	_snprintf(outZipPath, outZipSize - 1, "%sModView-update-%u.zip",
			  sTemp, (unsigned int)GetTickCount());
	outZipPath[outZipSize - 1] = '\0';

	char sUserAgent[64];
	_snprintf(sUserAgent, sizeof(sUserAgent) - 1, "ModView/%s", MODVIEW_VERSION_STRING);
	sUserAgent[sizeof(sUserAgent) - 1] = '\0';

	HINTERNET hSession = InternetOpenA(sUserAgent, INTERNET_OPEN_TYPE_PRECONFIG,
									   NULL, NULL, 0);
	if (!hSession) return false;
	HINTERNET hUrl = InternetOpenUrlA(hSession, psUrl, NULL, 0,
									   INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD |
									   INTERNET_FLAG_SECURE, 0);
	if (!hUrl) { InternetCloseHandle(hSession); return false; }

	FILE *f = fopen(outZipPath, "wb");
	if (!f) { InternetCloseHandle(hUrl); InternetCloseHandle(hSession); return false; }

	char buf[8192];
	DWORD dwRead;
	while (InternetReadFile(hUrl, buf, sizeof(buf), &dwRead) && dwRead > 0) {
		fwrite(buf, 1, dwRead, f);
	}
	fclose(f);
	InternetCloseHandle(hUrl);
	InternetCloseHandle(hSession);
	return true;
}


// Extract a .zip using Windows' built-in tar.exe (present since Win10 1803).
// Output dir is created if missing. Returns true on success.
static bool extract_zip(const char *psZipPath, const char *psOutDir)
{
	CreateDirectoryA(psOutDir, NULL);

	char sCmd[MAX_PATH * 3];
	_snprintf(sCmd, sizeof(sCmd) - 1,
			  "tar.exe -xf \"%s\" -C \"%s\"", psZipPath, psOutDir);
	sCmd[sizeof(sCmd) - 1] = '\0';

	STARTUPINFOA si = {0};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi = {0};

	if (!CreateProcessA(NULL, sCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
						NULL, NULL, &si, &pi)) {
		return false;
	}
	WaitForSingleObject(pi.hProcess, 60 * 1000);
	DWORD dwExit = 1;
	GetExitCodeProcess(pi.hProcess, &dwExit);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return dwExit == 0;
}


// Generates and launches a batch script that waits for the running exe
// to exit, copies the new exe into place, restarts it, and self-deletes.
// Returns true on success; the caller should then exit the process.
static bool launch_replace_script(const char *psNewExe, const char *psInstalledExe)
{
	char sTemp[MAX_PATH];
	get_temp_dir(sTemp, sizeof(sTemp));

	char sBatPath[MAX_PATH];
	_snprintf(sBatPath, sizeof(sBatPath) - 1,
			  "%sModView-update-%u.bat", sTemp, (unsigned int)GetTickCount());
	sBatPath[sizeof(sBatPath) - 1] = '\0';

	FILE *f = fopen(sBatPath, "wb");
	if (!f) return false;

	// Wait for ModView to release its exe lock, copy the new binary over,
	// relaunch, and self-delete. Retry budget caps the wait so a hung
	// process doesn't loop forever. `(goto) 2>nul & del %~f0` is the
	// idiom for a batch script deleting itself on exit.
	fprintf(f,
		"@echo off\r\n"
		"setlocal\r\n"
		"set TRIES=0\r\n"
		":wait\r\n"
		"ping 127.0.0.1 -n 2 >nul\r\n"
		"copy /Y \"%s\" \"%s\" >nul 2>&1\r\n"
		"if not errorlevel 1 goto launch\r\n"
		"set /a TRIES+=1\r\n"
		"if %%TRIES%% LSS 30 goto wait\r\n"
		"echo ModView update failed: could not overwrite \"%s\".\r\n"
		"pause\r\n"
		"goto cleanup\r\n"
		":launch\r\n"
		"start \"\" \"%s\"\r\n"
		":cleanup\r\n"
		"del \"%s\" >nul 2>&1\r\n"
		"(goto) 2>nul & del \"%%~f0\"\r\n",
		psNewExe, psInstalledExe,	// copy source / dest
		psInstalledExe,				// error message target
		psInstalledExe,				// relaunch
		psNewExe);					// cleanup of staged exe

	fclose(f);

	// Fire-and-forget the batch. Detach from our process so it survives
	// past our exit.
	STARTUPINFOA si = {0};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi = {0};

	char sCmd[MAX_PATH * 2];
	_snprintf(sCmd, sizeof(sCmd) - 1, "cmd.exe /c \"%s\"", sBatPath);
	sCmd[sizeof(sCmd) - 1] = '\0';

	if (!CreateProcessA(NULL, sCmd, NULL, NULL, FALSE,
						DETACHED_PROCESS | CREATE_NO_WINDOW,
						NULL, NULL, &si, &pi)) {
		return false;
	}
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
}


static bool install_from_asset(const UpdaterLatestRelease *pRel, CString &outError)
{
	if (!pRel->sAssetUrl[0]) {
		outError = "Release has no downloadable asset attached.";
		return false;
	}

	char sZipPath[MAX_PATH];
	if (!download_asset(pRel->sAssetUrl, sZipPath, sizeof(sZipPath))) {
		outError = "Failed to download the update archive.";
		return false;
	}

	char sTemp[MAX_PATH];
	get_temp_dir(sTemp, sizeof(sTemp));
	char sOutDir[MAX_PATH];
	_snprintf(sOutDir, sizeof(sOutDir) - 1, "%sModView-update-%u",
			  sTemp, (unsigned int)GetTickCount());
	sOutDir[sizeof(sOutDir) - 1] = '\0';

	if (!extract_zip(sZipPath, sOutDir)) {
		DeleteFileA(sZipPath);
		outError = "Failed to extract the update archive. (Is tar.exe available?)";
		return false;
	}
	DeleteFileA(sZipPath);	// no longer needed

	char sNewExe[MAX_PATH];
	_snprintf(sNewExe, sizeof(sNewExe) - 1, "%s\\ModView.exe", sOutDir);
	sNewExe[sizeof(sNewExe) - 1] = '\0';

	if (GetFileAttributesA(sNewExe) == INVALID_FILE_ATTRIBUTES) {
		outError = "Update archive did not contain ModView.exe.";
		return false;
	}

	char sInstalled[MAX_PATH];
	get_exe_path(sInstalled, sizeof(sInstalled));

	if (!launch_replace_script(sNewExe, sInstalled)) {
		outError = "Could not launch the update helper.";
		return false;
	}

	return true;
}


// =============================================================================
// Dialog presentation
// =============================================================================

// Custom update dialog. Built on top of the IDD_UPDATE_DIALOG resource so we
// get four distinct user actions plus a "skip this version" checkbox, which
// a plain MessageBox can't do. On close, m_iResult is one of:
//   IDC_UPDATE_INSTALL  - download + self-replace
//   IDC_UPDATE_OPENPAGE - open release page in the default browser
//   IDC_UPDATE_LATER    - do nothing (ask again next time)
// m_bSkipVersion is true if the checkbox was ticked. "Later + skip" is the
// interesting case: remembers this tag and won't prompt about it again.
class CUpdateAvailableDlg : public CDialog
{
public:
	CUpdateAvailableDlg(const UpdaterLatestRelease &rel, CWnd *pParent)
		: CDialog(IDD_UPDATE_DIALOG, pParent)
		, m_rel(rel)
		, m_iResult(IDC_UPDATE_LATER)
		, m_bSkipVersion(false)
	{}

	const UpdaterLatestRelease	m_rel;
	int							m_iResult;
	bool						m_bSkipVersion;

	DECLARE_MESSAGE_MAP()
protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnButton(UINT nID);
};

BEGIN_MESSAGE_MAP(CUpdateAvailableDlg, CDialog)
	ON_COMMAND_RANGE(IDC_UPDATE_INSTALL, IDC_UPDATE_LATER, OnButton)
END_MESSAGE_MAP()

BOOL CUpdateAvailableDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	CString sMsg;
	sMsg.Format(
		"A newer ModView release is available.\r\n\r\n"
		"Installed:  %s\r\n"
		"Available:  %s",
		MODVIEW_VERSION_STRING, m_rel.sTagName);
	SetDlgItemText(IDC_UPDATE_MESSAGE, sMsg);
	return TRUE;
}

void CUpdateAvailableDlg::OnButton(UINT nID)
{
	m_iResult = (int)nID;
	CButton *pChk = (CButton *)GetDlgItem(IDC_UPDATE_SKIP_VERSION);
	m_bSkipVersion = pChk && (pChk->GetCheck() == BST_CHECKED);
	EndDialog(IDOK);
}


static void show_update_dialog(CWnd *pParent, const UpdaterLatestRelease &rel)
{
	CUpdateAvailableDlg dlg(rel, pParent);
	dlg.DoModal();

	// If the user ticked "skip this version", remember the tag regardless of
	// which action they took below. Even after Install, re-saving the skip
	// is harmless - a newer release next cycle won't match it.
	if (dlg.m_bSkipVersion) {
		Updater_SaveSkippedVersion(rel.sTagName);
	}

	switch (dlg.m_iResult) {
	case IDC_UPDATE_INSTALL: {
		CWaitCursor wait;
		CString err;
		if (install_from_asset(&rel, err)) {
			::MessageBox(pParent ? pParent->GetSafeHwnd() : NULL,
						 "The update has been staged. ModView will now close and restart.",
						 "ModView Update", MB_OK | MB_ICONINFORMATION);
			if (AfxGetMainWnd()) AfxGetMainWnd()->PostMessage(WM_CLOSE);
		} else {
			::MessageBox(pParent ? pParent->GetSafeHwnd() : NULL,
						 err, "ModView Update", MB_OK | MB_ICONEXCLAMATION);
		}
		break;
	}
	case IDC_UPDATE_OPENPAGE:
		ShellExecuteA(NULL, "open", rel.sReleaseUrl, NULL, NULL, SW_SHOWNORMAL);
		break;
	case IDC_UPDATE_LATER:
	default:
		// Nothing to do - skip flag (if set) is already saved above.
		break;
	}
}


void Updater_CheckAndPromptInteractive(CWnd *pParent)
{
	CWaitCursor wait;
	UpdaterLatestRelease rel;
	char sErr[256] = {0};
	if (!Updater_FetchLatest(&rel, sErr, sizeof(sErr))) {
		::MessageBox(pParent ? pParent->GetSafeHwnd() : NULL, sErr,
					 "ModView Update", MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	if (!Updater_IsNewerThan(rel.sTagName, MODVIEW_VERSION_STRING)) {
		CString s;
		s.Format("ModView %s is up to date.\n(Latest release: %s)",
				 MODVIEW_VERSION_STRING, rel.sTagName);
		::MessageBox(pParent ? pParent->GetSafeHwnd() : NULL, s,
					 "ModView Update", MB_OK | MB_ICONINFORMATION);
		return;
	}

	show_update_dialog(pParent, rel);
}


// =============================================================================
// Background check on startup
// =============================================================================

struct BgCheckContext {
	HWND					hWndNotify;
	UpdaterLatestRelease	rel;
};

static unsigned __stdcall bg_check_thread(void *pArg)
{
	BgCheckContext *ctx = (BgCheckContext *)pArg;

	char sErr[256] = {0};
	if (Updater_FetchLatest(&ctx->rel, sErr, sizeof(sErr))) {
		if (Updater_IsNewerThan(ctx->rel.sTagName, MODVIEW_VERSION_STRING)) {
			// Skip the notification if the user previously chose "don't
			// remind me about this version" for exactly this tag. Newer
			// releases still notify - only the specific tag is suppressed.
			// Manual "Check for Updates..." always prompts regardless of
			// this preference, so the user can un-skip by going there.
			CString sSkipped = Updater_LoadSkippedVersion();
			if (sSkipped.IsEmpty() || sSkipped.CompareNoCase(ctx->rel.sTagName) != 0) {
				PostMessage(ctx->hWndNotify, WM_UPDATER_AVAILABLE, 0, (LPARAM)ctx);
				return 0;
			}
		}
	}
	// Silent failure / up-to-date / skipped - just free and exit.
	delete ctx;
	return 0;
}


void Updater_StartBackgroundCheck(CWnd *pMainWnd)
{
	if (!pMainWnd) return;

	// Dev builds opt out (MODVIEW_VERSION_STRING contains "-dev"). We don't
	// want the update prompt firing on the author's own workstation every
	// time they run the tool.
	if (strstr(MODVIEW_VERSION_STRING, "-dev") != NULL) return;

	BgCheckContext *ctx = new BgCheckContext;
	memset(ctx, 0, sizeof(*ctx));
	ctx->hWndNotify = pMainWnd->GetSafeHwnd();

	unsigned threadId = 0;
	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, bg_check_thread, ctx, 0, &threadId);
	if (hThread) {
		CloseHandle(hThread);
	} else {
		delete ctx;
	}
}


// Helper the main-window message handler calls when it receives
// WM_UPDATER_AVAILABLE. Takes ownership of the lParam context.
void Updater_ShowDialogFromBackgroundResult(CWnd *pParent, LPARAM lParam)
{
	BgCheckContext *ctx = (BgCheckContext *)lParam;
	if (!ctx) return;
	show_update_dialog(pParent, ctx->rel);
	delete ctx;
}
