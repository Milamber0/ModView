// Filename:-	updater.h
//
// GitHub release self-updater. Queries the GitHub API for the latest
// release of Milamber0/ModView, compares against MODVIEW_VERSION_STRING,
// and either notifies the user, opens the release page in a browser, or
// downloads the release asset and launches a helper batch script that
// swaps the running exe.

#ifndef UPDATER_H
#define UPDATER_H


struct UpdaterLatestRelease {
	char	sTagName[64];		// e.g. "3.1"
	char	sReleaseUrl[512];	// human-readable release page (html_url)
	char	sAssetUrl[512];		// direct .zip download (browser_download_url)
	char	sAssetName[128];	// e.g. "ModView-3.1-windows-x86.zip"
};


// Queries GitHub synchronously. Returns true on success, false on any
// failure (network, parse, HTTP). On failure outError gets a short
// human-readable message. Call from a worker thread if you need
// non-blocking behavior.
bool Updater_FetchLatest(UpdaterLatestRelease *pOut, char *outError, int outErrorSize);

// String compare two "X.Y[.Z][-suffix]" versions. Returns true when sNew is
// numerically newer than sCurrent. Anything with a non-numeric suffix
// (e.g. "3.0-dev") compares as older than the same numeric prefix, so a
// local dev build won't claim to be newer than the 3.0 release.
bool Updater_IsNewerThan(const char *sNew, const char *sCurrent);

// Manual "Check for Updates..." menu action. Blocks on the network call
// (wrapped in a wait cursor by the caller) and shows a result dialog:
//  - Newer release available: offers Install / Open Page / Later
//  - Up to date: brief message box
//  - Network / parse failure: error message box
// Shows "no model required" - fine to call anytime.
void Updater_CheckAndPromptInteractive(CWnd *pParent);

// Kicks off a background thread that calls Updater_FetchLatest. If a newer
// release is found, posts a message back to the main window which then
// shows the same update dialog as the manual path. Silent if the check
// fails (no network, etc) or if we're already up to date. Skipped
// automatically for local "-dev" builds.
void Updater_StartBackgroundCheck(CWnd *pMainWnd);

// Window message posted by the background worker when a newer release
// is detected. lParam carries an opaque context allocated by the worker;
// the main window passes it to Updater_ShowDialogFromBackgroundResult,
// which takes ownership and frees it.
#define WM_UPDATER_AVAILABLE		(WM_USER + 0x42)

void Updater_ShowDialogFromBackgroundResult(CWnd *pParent, LPARAM lParam);


#endif	// #ifndef UPDATER_H
