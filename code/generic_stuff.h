// Filename:-	generic_stuff.h
//

#ifndef GENERIC_STUFF_H
#define GENERIC_STUFF_H


extern char qdir[];
extern char	gamedir[];

// True when the exe was launched with "-log" on the command line. Debug log
// files (modview_efx.log, shader_log.txt, etc.) are only written when this
// is set, so normal end-user runs don't leave scratch .txt/.log files next
// to the exe. Flipped on by CModViewApp::InitInstance.
extern bool	g_bLogDebug;

void SetQdirFromPath( const char *path );
// Extracts the "<drive>/.../base/" (or "/development/") prefix from a full
// path into outGamedir without mutating the global `gamedir` or triggering
// Media_Delete. Returns true if a recognizable basedir was found. Use this
// when you need to temporarily speak a different gamedir for a single
// asset load (e.g. a bolted model that was saved under a different base
// than the currently-loaded primary).
bool ExtractGamedirFromPath( const char *path, char *outGamedir, int outSize );
void Filename_RemoveQUAKEBASE(CString &string);
bool FileExists (LPCSTR psFilename);
long FileLen( LPCSTR psFilename);
char *Filename_WithoutPath(LPCSTR psFilename);
char *Filename_WithoutExt(LPCSTR psFilename);
char *Filename_PathOnly(LPCSTR psFilename);
char *Filename_ExtOnly(LPCSTR psFilename);
char *String_EnsureMinLength(LPCSTR psString, int iMinLength);
char *String_ToLower(LPCSTR psString);
char *String_ToUpper(LPCSTR psString);
char *String_ForwardSlash(LPCSTR psString);
char *String_RemoveOccurences(LPCSTR psString, LPCSTR psSubStr);
char *va(char *format, ...);
char *scGetTempPath(void);
LPCSTR InputSaveFileName(LPCSTR psInitialSaveName, LPCSTR psCaption, LPCSTR psInitialPath, LPCSTR psFilter, LPCSTR psExtension);
LPCSTR InputLoadFileName(LPCSTR psInitialLoadName, LPCSTR psCaption, LPCSTR psInitialDir, LPCSTR psFilter);
LPCSTR scGetComputerName(void);
LPCSTR scGetUserName(void);

extern bool gbErrorBox_Inhibit;
LPCSTR ModView_GetLastError();
void ErrorBox(const char *sString);
void InfoBox(const char *sString);
void WarningBox(const char *sString);
void StatusMessage(LPCSTR psString);

void  SystemErrorBox(DWORD dwError = GetLastError());
LPCSTR SystemErrorString(DWORD dwError = GetLastError());

//#define GetYesNo(psQuery)	(!!(MessageBox(AppVars.hWnd,psQuery,"Query",MB_YESNO|MB_ICONWARNING|MB_TASKMODAL)==IDYES))
bool GetYesNo(const char *psQuery);

long filesize(FILE *handle);
int LoadFile (LPCSTR psPathedFilename, void **bufferptr, int bReportErrors = true);
int SaveFile (LPCSTR psPathedFilename, const void *pBuffer, int iSize);


bool TextFile_Read(CString &strFile, LPCSTR psFullPathedFilename, bool bLoseSlashSlashREMs = true, bool bLoseBlankLines = true);
bool SendFileToNotepad(LPCSTR psFilename);
bool SendStringToNotepad(LPCSTR psWhatever, LPCSTR psLocalFileName);



#endif	// #ifndef GENERIC_STUFF_H


////////////// eof /////////////

