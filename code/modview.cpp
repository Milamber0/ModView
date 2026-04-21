// ModView.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "includes.h"
#include "ModView.h"

#include "MainFrm.h"
#include "ModViewDoc.h"
#include "ModViewView.h"
#include "CommArea.h"
#include "wintalk.h"
#include "textures.h"
#include "Splash.h"
#include <afxadv.h>

// Custom MRU list that shows "parentfolder/model.glm" instead of just "model.glm"
class CModelRecentFileList : public CRecentFileList
{
public:
	CModelRecentFileList(UINT nStart, LPCTSTR lpszSection, LPCTSTR lpszEntryFormat, int nSize, int nMaxDispLen = AFX_ABBREV_FILENAME_LEN)
		: CRecentFileList(nStart, lpszSection, lpszEntryFormat, nSize, nMaxDispLen) {}

	virtual BOOL GetDisplayName(CString& strName, int nIndex, LPCTSTR lpszCurDir, int nCurDir, BOOL bAtLeastName = TRUE) const
	{
		if (nIndex < 0 || nIndex >= m_nSize || m_arrNames[nIndex].IsEmpty())
			return FALSE;

		// Extract "parentfolder/filename" from the full path
		CString fullPath = m_arrNames[nIndex];
		int lastSlash = fullPath.ReverseFind('\\');
		if (lastSlash < 0) lastSlash = fullPath.ReverseFind('/');
		if (lastSlash >= 0) {
			CString withoutFile = fullPath.Left(lastSlash);
			int prevSlash = withoutFile.ReverseFind('\\');
			if (prevSlash < 0) prevSlash = withoutFile.ReverseFind('/');
			if (prevSlash >= 0) {
				strName = fullPath.Mid(prevSlash + 1);
				return TRUE;
			}
		}
		strName = fullPath;
		return TRUE;
	}
};

bool gbStartMinimized = false;

void App_Init(void);

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CModViewApp

BEGIN_MESSAGE_MAP(CModViewApp, CWinApp)
	//{{AFX_MSG_MAP(CModViewApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CModViewApp construction

CModViewApp::CModViewApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance	
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CModViewApp object

CModViewApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CModViewApp initialization

bool bSafeToAddToMRU = false;
bool gbMainFrameInitialised = false;
BOOL CModViewApp::InitInstance()
{
	// CG: The following block was added by the Splash Screen component.
\
	{
\
		CCommandLineInfo cmdInfo;
\
		ParseCommandLine(cmdInfo);
\

\
		CSplashWnd::EnableSplashScreen(cmdInfo.m_bShowSplash);
\
	}
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	//Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	// Change the registry key under which our settings are stored.
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization.
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	LoadStdProfileSettings(16);  // Load standard INI file options (including MRU)

	// Replace MFC's default MRU with our custom one that shows parent folder names
	if (m_pRecentFileList) {
		CModelRecentFileList *pNewMRU = new CModelRecentFileList(
			m_pRecentFileList->m_nStart, _T("Recent File List"), _T("File%d"),
			m_pRecentFileList->m_nSize, m_pRecentFileList->m_nMaxDisplayLength);
		pNewMRU->ReadList();
		delete m_pRecentFileList;
		m_pRecentFileList = pNewMRU;
	}

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CModViewDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CModViewView));
	AddDocTemplate(pDocTemplate);

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	App_Init();

/*	TEMP ADDITION ONLY
// enable file manager drag/drop and DDE Execute open
   EnableShellOpen();
   RegisterShellFileTypes();
*/   

	// Dispatch commands specified on the command line  (ignore this, try and avoid that crappy document class)
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	if (gbStartMinimized)
	{
		extern void FuckingWellSetTheDocumentNameAndDontBloodyIgnoreMeYouCunt(LPCSTR psDocName);
					FuckingWellSetTheDocumentNameAndDontBloodyIgnoreMeYouCunt("Untitled");
	}

	// The one and only window has been initialized, so show and update it.
	m_pMainWnd->ShowWindow(gbStartMinimized?SW_MINIMIZE:SW_SHOW);
	m_pMainWnd->UpdateWindow();

	bSafeToAddToMRU = true;

	gbMainFrameInitialised = true;

	CSplashWnd::HideSplashScreen(true);
	StatusMessage(NULL);

	// Fire-and-forget check for a newer release on GitHub. Runs on a worker
	// thread so the main UI stays responsive; posts WM_UPDATER_AVAILABLE
	// back to the main frame if a newer tag is found. Silent on failure
	// (no network, etc) and skipped entirely for '-dev' builds.
	{
		extern void Updater_StartBackgroundCheck(CWnd *);
		Updater_StartBackgroundCheck(m_pMainWnd);
	}
	/*
extern void R_LoadQuaternionIndex(const char* filename);

	R_LoadQuaternionIndex("quaternions.bin");
	*/
	return TRUE;
}

void Filename_AddToMRU(LPCSTR psFilename)
{
	if (bSafeToAddToMRU)
	{
		theApp.AddToRecentFileList(psFilename);
	}
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

#include "modview_version.h"

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	// Both labels pick up the version from the CMake-generated header so CI
	// release builds (-DMODVIEW_VERSION_STRING=3.X) display the right number
	// without touching the .rc by hand. The window caption stays "About
	// ModView 3" from the .rc - the product line name, not the version.
	CString sVersion, sCredit;
	sVersion.Format("ModView Version %s", MODVIEW_VERSION_STRING);
	sCredit.Format("%s Created by Milamber. Originally Written by Ste Cork (+ MFC help from Mike Crowns)", MODVIEW_VERSION_STRING);
	SetDlgItemText(IDC_ABOUT_VERSION, sVersion);
	SetDlgItemText(IDC_ABOUT_CREDIT, sCredit);
	return TRUE;
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CModViewApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CModViewApp message handlers


BOOL CModViewApp::OnIdle(LONG lCount) 
{
	// this works, but doesn't get called fast/often enough to be useful at the moment. Maybe one day...

//	static int i=0;
//	i++;
//	OutputDebugString(va("%d\n",i));
  	
	if (WinTalk_HandleMessages())
	{
		OnAppExit();
	}

	return CWinApp::OnIdle(lCount);
}


// a place I can just stuff any remaining non-windows shutdown code...
//
void App_FinalExit(void)
{
	Model_Delete();
	FakeCvars_Shutdown();
	CommArea_ShutDown();
}

// GL not running at this point, it gets started when the window is created. This is for stuff before that.
//
void App_Init(void)
{
	App_OnceOnly();
	FakeCvars_OnceOnlyInit();
	CommArea_ServerInitOnceOnly();
}


int CModViewApp::ExitInstance()
{
	bSafeToAddToMRU = false;

	// Persist MRU and standard profile settings before running teardown, so a
	// crash inside App_FinalExit() (GL/shutdown issues) can't eat the recent
	// files list.
	SaveStdProfileSettings();
	if (m_pRecentFileList) {
		m_pRecentFileList->WriteList();
	}

	App_FinalExit();

	return CWinApp::ExitInstance();
}



BOOL CModViewApp::PreTranslateMessage(MSG* pMsg)
{
	// CG: The following lines were added by the Splash Screen component.
	if (CSplashWnd::PreTranslateAppMessage(pMsg))
		return TRUE;

	return CWinApp::PreTranslateMessage(pMsg);
}
