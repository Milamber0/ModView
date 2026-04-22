// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "includes.h"
#include <mmsystem.h>

#include "ModView.h"
#include "ModViewDoc.h"
#include "ModViewView.h"
#include "ModViewTreeView.h"
#include "model.h"
#include "wintalk.h"
#include "text.h"
#include "clipboard.h"
#include "textures.h"
#include "script.h"
#include "image.h"
#include "png/png.h"
#include "r_image.h"
#include "SOF2NPCViewer.h"
#include "splash.h"
#include "files.h"
#include "r_common.h"
#include "updater.h"
//
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


static void XLS_To_SP(void);

#define PLAY_LAME_WAV							\
		if ( !PlaySound("k:\\util\\bhr_l.bin",	\
						NULL,					\
						SND_FILENAME|SND_ASYNC	\
						)						\
			)									\
		{										\
			/* error, but ignore that */		\
		}



/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_VIEW_WIREFRAME, OnViewWireframe)
	ON_UPDATE_COMMAND_UI(ID_VIEW_WIREFRAME, OnUpdateViewWireframe)
	ON_COMMAND(ID_VIEW_ALPHA, OnViewAlpha)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ALPHA, OnUpdateViewAlpha)
	ON_COMMAND(ID_VIEW_ALPHA_MODE, OnViewAlphaMode2)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ALPHA_MODE, OnUpdateViewAlphaMode2)
	ON_COMMAND(ID_VIEW_INTERPOLATE, OnViewInterpolate)
	ON_UPDATE_COMMAND_UI(ID_VIEW_INTERPOLATE, OnUpdateViewInterpolate)
	ON_COMMAND(ID_VIEW_BILINEAR, OnViewBilinear)
	ON_UPDATE_COMMAND_UI(ID_VIEW_BILINEAR, OnUpdateViewBilinear)
	ON_COMMAND(ID_VIEW_SCREENSHOT_FILE, OnViewScreenshotFile)
	ON_COMMAND(ID_VIEW_SCREENSHOT_CLIPBOARD, OnViewScreenshotClipboard)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_COMMAND(ID_VIEW_ORIGIN, OnViewOrigin)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ORIGIN, OnUpdateViewOrigin)
	ON_COMMAND(ID_VIEW_GLINFO, OnViewGlinfo)
	ON_COMMAND(ID_ANIMATION_START, OnAnimationStart)
	ON_COMMAND(ID_ANIMATION_STOP, OnAnimationStop)
	ON_COMMAND(ID_ANIMATION_REWIND, OnAnimationRewind)
	ON_COMMAND(ID_ANIMATION_FASTER, OnAnimationFaster)
	ON_COMMAND(ID_ANIMATION_SLOWER, OnAnimationSlower)
	ON_COMMAND(ID_ANIMATION_LERPING, OnAnimationLerping)
	ON_UPDATE_COMMAND_UI(ID_ANIMATION_LERPING, OnUpdateAnimationLerping)
	ON_COMMAND(ID_ANIMATION_SHADERS, OnAnimationShaders)
	ON_UPDATE_COMMAND_UI(ID_ANIMATION_SHADERS, OnUpdateAnimationShaders)
	ON_COMMAND(ID_VIEW_SHADERS, OnViewShaders)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHADERS, OnUpdateViewShaders)
	ON_COMMAND(ID_VIEW_ENTITYCOLOR, OnViewEntityColor)
	ON_COMMAND(ID_SEQUENCES_SEARCH, OnSequencesSearch)
	ON_COMMAND(ID_VIEW_DYNAMICGLOW, OnViewDynamicGlow)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DYNAMICGLOW, OnUpdateViewDynamicGlow)
	ON_COMMAND(ID_VIEW_DYNAMICGLOWSOFT, OnViewDynamicGlowSoft)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DYNAMICGLOWSOFT, OnUpdateViewDynamicGlowSoft)
	ON_COMMAND(ID_VIEW_GLOWFULLBRIGHTCOMP, OnViewGlowFullbrightComp)
	ON_UPDATE_COMMAND_UI(ID_VIEW_GLOWFULLBRIGHTCOMP, OnUpdateViewGlowFullbrightComp)
	ON_COMMAND(ID_VIEW_GLOWDEBUG, OnViewGlowDebug)
	ON_UPDATE_COMMAND_UI(ID_VIEW_GLOWDEBUG, OnUpdateViewGlowDebug)
	ON_COMMAND(ID_VIEW_WEAPONS, OnViewWeapons)
	ON_COMMAND_RANGE(ID_WEAPON_FIRST, ID_WEAPON_LAST, OnWeaponMenuCommand)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnUpdateFileSave)
	ON_COMMAND(ID_MODEL_SAVE_AS, OnModelSaveAs)
	ON_COMMAND(ID_FILE_WRITEIDEAL, OnFileWriteideal)
	ON_COMMAND(ID_ANIMATION_NEXTFRAME, OnAnimationNextframe)
	ON_COMMAND(ID_ANIMATION_PREVFRAME, OnAnimationPrevframe)
	ON_COMMAND(ID_VIEW_LOD0, OnViewLod0)
	ON_COMMAND(ID_VIEW_LOD1, OnViewLod1)
	ON_COMMAND(ID_VIEW_LOD2, OnViewLod2)
	ON_COMMAND(ID_VIEW_LOD3, OnViewLod3)
	ON_COMMAND(ID_VIEW_LOD4, OnViewLod4)
	ON_COMMAND(ID_VIEW_LOD5, OnViewLod5)
	ON_COMMAND(ID_VIEW_LOD6, OnViewLod6)
	ON_COMMAND(ID_VIEW_LOD7, OnViewLod7)
	ON_COMMAND(ID_EDIT_BGRNDCOLOUR, OnEditBgrndcolour)
	ON_UPDATE_COMMAND_UI(ID_VIEW_BONEHILITE, OnUpdateViewBonehilite)
	ON_COMMAND(ID_VIEW_BONEHILITE, OnViewBonehilite)
	ON_COMMAND(ID_VIEW_NORMALS, OnViewNormals)
	ON_UPDATE_COMMAND_UI(ID_VIEW_NORMALS, OnUpdateViewNormals)
	ON_COMMAND(ID_VIEW_SURFACEHILITE, OnViewSurfacehilite)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SURFACEHILITE, OnUpdateViewSurfacehilite)
	ON_COMMAND(ID_VIEW_VERTINDEXES, OnViewVertindexes)
	ON_UPDATE_COMMAND_UI(ID_VIEW_VERTINDEXES, OnUpdateViewVertindexes)
	ON_COMMAND(ID_VIEW_FOVCYCLE, OnViewFovcycle)
	ON_COMMAND(ID_FILE_READIDEAL, OnFileReadideal)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_COMMAND(ID_FILE_REFRESHTEXTURES, OnFileRefreshtextures)
	ON_UPDATE_COMMAND_UI(ID_FILE_PRINT, OnUpdateFilePrint)
	ON_UPDATE_COMMAND_UI(ID_FILE_PRINT_PREVIEW, OnUpdateFilePrintPreview)
	ON_UPDATE_COMMAND_UI(ID_FILE_PRINT_SETUP, OnUpdateFilePrintSetup)
	ON_UPDATE_COMMAND_UI(ID_EDIT_TESTFUNCTION, OnUpdateEditTestfunction)
	ON_COMMAND(ID_EDIT_TESTFUNCTION, OnEditTestfunction)
	ON_COMMAND(ID_FILE_RESETVIEWPARAMS, OnFileResetviewparams)
	ON_COMMAND(ID_ANIMATION_STARTWITHWRAPFORCE, OnAnimationStartwithwrapforce)
	ON_COMMAND(ID_FILE_WRITESCRIPT, OnFileWritescript)
	ON_COMMAND(ID_FILE_READSCRIPT, OnFileReadscript)
	ON_UPDATE_COMMAND_UI(ID_FILE_WRITESCRIPT, OnUpdateFileWritescript)
	ON_COMMAND(ID_VIEW_SURFACEHILITEWITHBONEREFS, OnViewSurfacehilitewithbonerefs)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SURFACEHILITEWITHBONEREFS, OnUpdateViewSurfacehilitewithbonerefs)
	ON_COMMAND(ID_VIEW_TAGSURFACES, OnViewTagsurfaces)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TAGSURFACES, OnUpdateViewTagsurfaces)
	ON_COMMAND(ID_VIEW_TAGSASRGB, OnViewTagsasrgb)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TAGSASRGB, OnUpdateViewTagsasrgb)
	ON_COMMAND(ID_PICMIP_0, OnPicmip0)
	ON_UPDATE_COMMAND_UI(ID_PICMIP_0, OnUpdatePicmip0)
	ON_COMMAND(ID_PICMIP_1, OnPicmip1)
	ON_UPDATE_COMMAND_UI(ID_PICMIP_1, OnUpdatePicmip1)
	ON_COMMAND(ID_PICMIP_2, OnPicmip2)
	ON_UPDATE_COMMAND_UI(ID_PICMIP_2, OnUpdatePicmip2)
	ON_COMMAND(ID_PICMIP_3, OnPicmip3)
	ON_UPDATE_COMMAND_UI(ID_PICMIP_3, OnUpdatePicmip3)
	ON_COMMAND(ID_PICMIP_4, OnPicmip4)
	ON_COMMAND(ID_PICMIP_5, OnPicmip5)
	ON_COMMAND(ID_PICMIP_6, OnPicmip6)
	ON_COMMAND(ID_PICMIP_7, OnPicmip7)
	ON_COMMAND(ID_VIEW_RULER, OnViewRuler)
	ON_UPDATE_COMMAND_UI(ID_VIEW_RULER, OnUpdateViewRuler)
	ON_COMMAND(ID_VIEW_FORCEWHITE, OnViewForcewhite)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FORCEWHITE, OnUpdateViewForcewhite)
	ON_COMMAND(ID_VIEW_SCREENSHOT_CLEAN, OnViewScreenshotClean)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SCREENSHOT_CLEAN, OnUpdateViewScreenshotClean)
	ON_COMMAND(ID_VIEW_VERTWEIGHTING, OnViewVertweighting)
	ON_UPDATE_COMMAND_UI(ID_VIEW_VERTWEIGHTING, OnUpdateViewVertweighting)
	ON_COMMAND(ID_VIEW_BBOX, OnViewBbox)
	ON_UPDATE_COMMAND_UI(ID_VIEW_BBOX, OnUpdateViewBbox)
	ON_COMMAND(ID_VIEW_FLOOR, OnViewFloor)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FLOOR, OnUpdateViewFloor)
	ON_COMMAND(ID_EDIT_SETFLOOR_ABS, OnEditSetfloorAbs)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SETFLOOR_ABS, OnUpdateEditSetfloorAbs)
	ON_COMMAND(ID_EDIT_SETFLOOR_CURRENT, OnEditSetfloorCurrent)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SETFLOOR_CURRENT, OnUpdateEditSetfloorCurrent)
	ON_COMMAND(ID_VIEW_BONEFILTERING, OnViewBonefiltering)
	ON_UPDATE_COMMAND_UI(ID_VIEW_BONEFILTERING, OnUpdateViewBonefiltering)
	ON_COMMAND(ID_EDIT_SETBONEWEIGHT_THRESHHOLD, OnEditSetboneweightThreshhold)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SETBONEWEIGHT_THRESHHOLD, OnUpdateEditSetboneweightThreshhold)
	ON_COMMAND(ID_BONEFILTER_INCTHRESHHOLD,OnEditBoneFilterINCThreshhold)
	ON_COMMAND(ID_BONEFILTER_DECTHRESHHOLD,OnEditBoneFilterDECThreshhold)
	ON_COMMAND(ID_VIEW_CRACKVIEWER, OnViewCrackviewer)
	ON_UPDATE_COMMAND_UI(ID_VIEW_CRACKVIEWER, OnUpdateViewCrackviewer)
	ON_COMMAND(ID_VIEW_UNSHADOWABLESURFACES, OnViewUnshadowablesurfaces)
	ON_UPDATE_COMMAND_UI(ID_VIEW_UNSHADOWABLESURFACES, OnUpdateViewUnshadowablesurfaces)
	ON_COMMAND(ID_FILE_VIEW_SOF2_NPCS, OnFileViewSof2Npcs)
	ON_UPDATE_COMMAND_UI(ID_FILE_VIEW_SOF2_NPCS, OnUpdateFileViewSof2Npcs)
	ON_COMMAND(IDM_EDIT_ALLOWSKELETONOVERRIDES, OnEditAllowskeletonoverrides)
	ON_UPDATE_COMMAND_UI(IDM_EDIT_ALLOWSKELETONOVERRIDES, OnUpdateEditAllowskeletonoverrides)
	ON_COMMAND(ID_VIEW_DOUBLESIDEDPOLYS, OnViewDoublesidedpolys)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DOUBLESIDEDPOLYS, OnUpdateViewDoublesidedpolys)
	ON_COMMAND(IDM_EDIT_TOPMOST, OnEditTopmost)
	ON_UPDATE_COMMAND_UI(IDM_EDIT_TOPMOST, OnUpdateEditTopmost)
	ON_COMMAND(ID_VIEW_TRIINDEXES, OnViewTriindexes)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TRIINDEXES, OnUpdateViewTriindexes)
	ON_COMMAND(ID_FILE_VIEW_JK2_BOTS, OnFileViewJk2Bots)
	ON_COMMAND(ID_ANIMATION_ENDFRAME, OnAnimationEndframe)
	ON_COMMAND(ID_ANIMATION_15FPS, OnAnimation15FPS)
	ON_COMMAND(ID_ANIMATION_20FPS, OnAnimation20FPS)
	ON_COMMAND(ID_ANIMATION_40FPS, OnAnimation40FPS)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_FILE_BATCHCONVERT, OnFileBatchconvert)
	ON_WM_ENTERSIZEMOVE()
	ON_WM_EXITSIZEMOVE()
	ON_COMMAND(ID_HELP_CHECKFORUPDATES, OnHelpCheckForUpdates)
	ON_MESSAGE(WM_UPDATER_AVAILABLE, OnUpdaterAvailable)
	END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

// True while the user is actively drag-resizing or moving the main window.
// We skip the glow post-process and several heavier GL operations in that
// window because the NVIDIA driver sporadically AVs when the GL backing
// surface is being resized underneath an in-flight SwapBuffers / copy.
bool g_bInSizeMove = false;

void CMainFrame::OnEnterSizeMove()
{
	g_bInSizeMove = true;
	CFrameWnd::OnEnterSizeMove();
}

void CMainFrame::OnExitSizeMove()
{
	g_bInSizeMove = false;
	CFrameWnd::OnExitSizeMove();
	// Force a full repaint now that the window has settled, so the glow
	// pass re-establishes the correct texture size.
	extern void ModelList_ForceRedraw(void);
	ModelList_ForceRedraw();
}

void CMainFrame::OnHelpCheckForUpdates()
{
	extern void Updater_CheckAndPromptInteractive(CWnd *);
	Updater_CheckAndPromptInteractive(this);
}

LRESULT CMainFrame::OnUpdaterAvailable(WPARAM /*wParam*/, LPARAM lParam)
{
	// Posted from the background update-check worker when a newer release
	// was found. The lParam carries an allocated context which the helper
	// below takes ownership of and frees.
	extern void Updater_ShowDialogFromBackgroundResult(CWnd *, LPARAM);
	Updater_ShowDialogFromBackgroundResult(this, lParam);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	// TODO: add member initialization code here	
}

CMainFrame::~CMainFrame()
{
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	{
		// Append shader buttons to the main toolbar.
		// Main toolbar images are 16x15. Shader bitmap is 15x15.
		// Create a padded 32x15 (2x 16x15) bitmap from the 30x15 (2x 15x15) source.
		CToolBarCtrl &tbc = m_wndToolBar.GetToolBarCtrl();

		CBitmap srcBmp;
		srcBmp.LoadBitmap(IDR_SHADER_TOOLBAR);
		BITMAP srcInfo;
		srcBmp.GetBitmap(&srcInfo);

		// Create a padded bitmap (16 pixels wide per icon instead of 15)
		CDC *pDC = GetDC();
		CDC srcDC, dstDC;
		srcDC.CreateCompatibleDC(pDC);
		dstDC.CreateCompatibleDC(pDC);

		int nIcons = srcInfo.bmWidth / 15;
		int padWidth = nIcons * 16;
		COLORREF btnFace = GetSysColor(COLOR_BTNFACE);

		CBitmap paddedBmp;
		paddedBmp.CreateCompatibleBitmap(pDC, padWidth, 15);

		CBitmap *pOldSrc = srcDC.SelectObject(&srcBmp);
		CBitmap *pOldDst = dstDC.SelectObject(&paddedBmp);

		// Fill padding with the current theme's button face color so the
		// space between icons matches the toolbar.
		dstDC.FillSolidRect(0, 0, padWidth, 15, btnFace);

		// Copy each 15x15 icon into a 16x15 slot
		for (int i = 0; i < nIcons; i++) {
			dstDC.BitBlt(i * 16, 0, 15, 15, &srcDC, i * 15, 0, SRCCOPY);
		}

		dstDC.SelectObject(pOldDst);
		srcDC.SelectObject(pOldSrc);

		// Remap 0xC0C0C0 (the classic Win95 button-face color baked into the
		// .bmp backgrounds) to the current COLOR_BTNFACE so icons blend with
		// the modern toolbar theme. MFC's CToolBar::LoadToolBar does this
		// automatically via CreateMappedBitmap, but TB_ADDBITMAP does not.
		{
			BITMAPINFO bmi = {0};
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = padWidth;
			bmi.bmiHeader.biHeight = -15; // top-down
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;

			int rowBytes = padWidth * 4;
			BYTE *pBits = new BYTE[rowBytes * 15];
			HDC hdc = pDC->m_hDC;
			HBITMAP hbm = (HBITMAP)paddedBmp.GetSafeHandle();

			if (::GetDIBits(hdc, hbm, 0, 15, pBits, &bmi, DIB_RGB_COLORS))
			{
				BYTE tgtB = GetBValue(btnFace);
				BYTE tgtG = GetGValue(btnFace);
				BYTE tgtR = GetRValue(btnFace);
				for (int y = 0; y < 15; y++) {
					BYTE *row = pBits + y * rowBytes;
					for (int x = 0; x < padWidth; x++) {
						if (row[x*4] == 0xC0 && row[x*4+1] == 0xC0 && row[x*4+2] == 0xC0) {
							row[x*4]   = tgtB;
							row[x*4+1] = tgtG;
							row[x*4+2] = tgtR;
						}
					}
				}
				::SetDIBits(hdc, hbm, 0, 15, pBits, &bmi, DIB_RGB_COLORS);
			}
			delete[] pBits;
		}

		ReleaseDC(pDC);

		// Add the padded bitmap to the toolbar
		TBADDBITMAP tbab;
		tbab.hInst = NULL;
		tbab.nID = (UINT_PTR)(HBITMAP)paddedBmp;
		int iFirstNewImage = (int)::SendMessage(tbc.m_hWnd, TB_ADDBITMAP, nIcons, (LPARAM)&tbab);
		paddedBmp.Detach(); // toolbar takes ownership

		// Add a separator and the custom buttons
		// Bitmap order: [0]=search, [1]=shader on/off, [2]=shader anim, [3]=color picker, [4]=glow, [5]=weapons
		TBBUTTON buttons[7];
		memset(buttons, 0, sizeof(buttons));
		buttons[0].fsStyle = TBSTYLE_SEP;
		buttons[1].iBitmap = iFirstNewImage + 0;
		buttons[1].idCommand = ID_SEQUENCES_SEARCH;
		buttons[1].fsState = TBSTATE_ENABLED;
		buttons[1].fsStyle = TBSTYLE_BUTTON;
		buttons[2].iBitmap = iFirstNewImage + 1;
		buttons[2].idCommand = ID_VIEW_SHADERS;
		buttons[2].fsState = TBSTATE_ENABLED;
		buttons[2].fsStyle = TBSTYLE_CHECK;
		buttons[3].iBitmap = iFirstNewImage + 2;
		buttons[3].idCommand = ID_ANIMATION_SHADERS;
		buttons[3].fsState = TBSTATE_ENABLED;
		buttons[3].fsStyle = TBSTYLE_CHECK;
		buttons[4].iBitmap = iFirstNewImage + 3;
		buttons[4].idCommand = ID_VIEW_ENTITYCOLOR;
		buttons[4].fsState = TBSTATE_ENABLED;
		buttons[4].fsStyle = TBSTYLE_BUTTON;
		buttons[5].iBitmap = iFirstNewImage + 4;
		buttons[5].idCommand = ID_VIEW_DYNAMICGLOW;
		buttons[5].fsState = TBSTATE_ENABLED;
		buttons[5].fsStyle = TBSTYLE_CHECK;
		buttons[6].iBitmap = iFirstNewImage + 5;
		buttons[6].idCommand = ID_VIEW_WEAPONS;
		buttons[6].fsState = TBSTATE_ENABLED;
		buttons[6].fsStyle = TBSTYLE_BUTTON;
		tbc.AddButtons(7, buttons);
	}

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	// Uncomment these three lines if you want the toolbar to be dockable
	//m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	//EnableDocking(CBRS_ALIGN_ANY);
	//DockControlBar(&m_wndToolBar);

	// CG: The following line was added by the Splash Screen component.
	CSplashWnd::ShowSplashScreen(this);
	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers


BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) 
{
	if (!m_splitter.CreateStatic(this, 1, 2))
	{
		return false;
	}
	if (!m_splitter.CreateView(0, 0, RUNTIME_CLASS(CModViewTreeView), CSize(150, 100), pContext))
	{
		return false;
	}
	if (!m_splitter.CreateView(0, 1, RUNTIME_CLASS(CModViewView), CSize(200, 100), pContext))
	{
		return false;
	}

//	m_splitter.Create(this,1,2,CSize(100,100),pContext);

	return true;
}

void CMainFrame::OnFileOpen() 
{		
	LPCSTR psFullPathedFilename = InputLoadFileName("",				// LPCSTR psInitialLoadName, 
													"Load Model",	// LPCSTR psCaption,
													Filename_PathOnly(Model_GetFullPrimaryFilename()),	//	"S:\\baseq3\\models\\test\\bonehier",	// LPCSTR psInitialDir, 
													Model_GetSupportedTypesFilter(true)			// LPCSTR psFilter
													);

	if (psFullPathedFilename)
	{
		GetActiveDocument()->OnOpenDocument(psFullPathedFilename);
	}
}



void CMainFrame::OnViewWireframe() 
{
	AppVars.bWireFrame = !AppVars.bWireFrame;	
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewWireframe(CCmdUI* pCmdUI) 
{		
	pCmdUI->SetCheck(AppVars.bWireFrame);
}

void CMainFrame::OnViewAlpha() 
{
	AppVars.bUseAlpha = !AppVars.bUseAlpha;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewAlpha(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bUseAlpha);	
}

void CMainFrame::OnViewAlphaMode2()
{
	AppVars.bUseAlphaMode2 = !AppVars.bUseAlphaMode2;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewAlphaMode2(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(AppVars.bUseAlphaMode2);
}

void CMainFrame::OnViewInterpolate() 
{
	AppVars.bInterpolate = !AppVars.bInterpolate;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewInterpolate(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bInterpolate);	
}

// these 2 do the same thing as other functions, but it just seemed reasonable to also be in another menu...
//
void CMainFrame::OnAnimationLerping() 
{
	OnViewInterpolate();
}

void CMainFrame::OnUpdateAnimationLerping(CCmdUI* pCmdUI)
{
	OnUpdateViewInterpolate(pCmdUI);
}

void CMainFrame::OnAnimationShaders()
{
	AppVars.bShaderAnimation = !AppVars.bShaderAnimation;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateAnimationShaders(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(AppVars.bShaderAnimation);
}

void CMainFrame::OnViewShaders()
{
	AppVars.bShaderRendering = !AppVars.bShaderRendering;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewShaders(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(AppVars.bShaderRendering);
}


// Parse playerchoice.txt from the model's folder and return preset colors
struct PlayerChoiceColor {
	CString name;
	COLORREF color;
};

static bool ParsePlayerChoiceColors(CArray<PlayerChoiceColor, PlayerChoiceColor&> &colors)
{
	LPCSTR psModelPath = Model_GetFullPrimaryFilename();
	if (!psModelPath || !psModelPath[0]) return false;

	// Get the model's directory
	CString strDir(psModelPath);
	int lastSlash = strDir.ReverseFind('\\');
	if (lastSlash < 0) lastSlash = strDir.ReverseFind('/');
	if (lastSlash < 0) return false;
	strDir = strDir.Left(lastSlash + 1);

	CString strFile = strDir + "playerchoice.txt";
	FILE *f = fopen(strFile, "rt");
	if (!f) return false;

	char line[1024];
	while (fgets(line, sizeof(line), f)) {
		// Skip comments and blank lines
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '/' && *(p+1) == '/') continue;
		if (*p == '\n' || *p == '\r' || *p == 0) continue;

		// If line starts with '{', we're inside a block
		if (*p == '{') continue;
		if (*p == '}') continue;
		if (strstr(p, "setcvar")) continue;

		// This should be a choice name line (before a '{' block)
		CString choiceName(p);
		choiceName.TrimRight();
		choiceName.TrimLeft();
		if (choiceName.IsEmpty()) continue;

		// Extract just the last part of the path for display
		CString displayName = choiceName;
		int slash = displayName.ReverseFind('/');
		if (slash >= 0) displayName = displayName.Mid(slash + 1);

		// Read the block to get RGB values
		int r = 255, g = 255, b = 255;
		while (fgets(line, sizeof(line), f)) {
			p = line;
			while (*p == ' ' || *p == '\t') p++;
			if (*p == '}') break;
			if (strstr(p, "ui_char_color_red")) {
				char *val = strstr(p, "red");
				if (val) { val += 3; r = atoi(val); }
			} else if (strstr(p, "ui_char_color_green")) {
				char *val = strstr(p, "green");
				if (val) { val += 5; g = atoi(val); }
			} else if (strstr(p, "ui_char_color_blue")) {
				char *val = strstr(p, "blue");
				if (val) { val += 4; b = atoi(val); }
			}
		}

		PlayerChoiceColor pc;
		pc.name = displayName;
		pc.color = RGB(r, g, b);
		colors.Add(pc);
	}

	fclose(f);
	return colors.GetSize() > 0;
}


void CMainFrame::OnViewEntityColor()
{
	// Check for playerchoice.txt presets
	CArray<PlayerChoiceColor, PlayerChoiceColor&> presetColors;
	bool hasPresets = ParsePlayerChoiceColors(presetColors);

	// Build custom colors array for the color dialog (up to 16)
	COLORREF customColors[16];
	for (int i = 0; i < 16; i++) customColors[i] = RGB(255, 255, 255);
	for (int i = 0; i < presetColors.GetSize() && i < 16; i++) {
		customColors[i] = presetColors[i].color;
	}

	COLORREF initialColor = RGB(AppVars.entityRGBA[0], AppVars.entityRGBA[1], AppVars.entityRGBA[2]);

	CColorDialog dlg(initialColor, CC_FULLOPEN | CC_RGBINIT, this);
	dlg.m_cc.lpCustColors = customColors;

	if (dlg.DoModal() == IDOK) {
		COLORREF c = dlg.GetColor();
		AppVars.entityRGBA[0] = GetRValue(c);
		AppVars.entityRGBA[1] = GetGValue(c);
		AppVars.entityRGBA[2] = GetBValue(c);
		AppVars.entityRGBA[3] = 255;
		m_splitter.Invalidate(false);
	}
}


void CMainFrame::OnViewDynamicGlow()
{
	AppVars.bDynamicGlow = !AppVars.bDynamicGlow;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewDynamicGlow(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(AppVars.bDynamicGlow);
}

void CMainFrame::OnViewDynamicGlowSoft()
{
	AppVars.bDynamicGlowSoft = !AppVars.bDynamicGlowSoft;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewDynamicGlowSoft(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(AppVars.bDynamicGlowSoft);
}

void CMainFrame::OnViewGlowFullbrightComp()
{
	AppVars.bDynamicGlowFullbrightComp = !AppVars.bDynamicGlowFullbrightComp;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewGlowFullbrightComp(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(AppVars.bDynamicGlowFullbrightComp);
}

void CMainFrame::OnViewGlowDebug()
{
	AppVars.bDynamicGlowDebug = !AppVars.bDynamicGlowDebug;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewGlowDebug(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(AppVars.bDynamicGlowDebug);
}


// ============================================================================
// Weapon Bolt System
// ============================================================================

// Bolt location IDs encoded in the menu command: ID_WEAPON_FIRST + weaponIndex * 6 + locationIndex
// Locations: 0=Right Hand, 1=Left Hand, 2=Right Hip, 3=Left Hip, 4=Back, 5=Remove
#define WEAPON_LOC_RHAND	0
#define WEAPON_LOC_LHAND	1
#define WEAPON_LOC_RHIP		2
#define WEAPON_LOC_LHIP		3
#define WEAPON_LOC_BACK		4
#define WEAPON_LOC_REMOVE	5
#define WEAPON_LOCS_PER_WEAPON 6

struct WeaponEntry {
	CString displayName;
	CString fullPath;		// full disk path to .glm
	CString localPath;		// game-relative path
};

static CArray<WeaponEntry, WeaponEntry&> g_weaponList;

static void ScanWeapons()
{
	g_weaponList.RemoveAll();

	if (!gamedir[0]) return;

	CString strWeaponsDir;
	strWeaponsDir.Format("%smodels/weapons2/", gamedir);

	// Scan subdirectories
	char **ppDirs = NULL;
	int nDirs = 0;
	ppDirs = Sys_ListFiles(strWeaponsDir, "/", NULL, &nDirs, qtrue);

	if (!ppDirs || !nDirs) return;

	for (int d = 0; d < nDirs; d++) {
		if (ppDirs[d][0] == '.') continue;

		CString subDir;
		subDir.Format("%s%s/", (LPCSTR)strWeaponsDir, ppDirs[d]);

		// Look for *_w.glm files in this subdir
		char **ppFiles = NULL;
		int nFiles = 0;
		ppFiles = Sys_ListFiles(subDir, ".glm", NULL, &nFiles, qfalse);

		if (ppFiles && nFiles > 0) {
			for (int f = 0; f < nFiles; f++) {
				CString fileName(ppFiles[f]);
				fileName.MakeLower();

				WeaponEntry we;
				we.displayName = ppDirs[d];
				we.fullPath.Format("%s%s", (LPCSTR)subDir, ppFiles[f]);
				we.localPath.Format("models/weapons2/%s/%s", ppDirs[d], ppFiles[f]);
				g_weaponList.Add(we);
			}
			Sys_FreeFileList(ppFiles);
		}
	}

	Sys_FreeFileList(ppDirs);
}

// Each bolt location has a list of names to try (surface first, then bone fallbacks)
struct BoltLocationDef {
	LPCSTR names[4];	// NULL-terminated list of names to try
	bool   isBone[4];	// whether each name is a bone (true) or surface (false)
};

static BoltLocationDef g_boltLocations[] = {
	// RHAND: tag surface "*r_hand"
	{ {"*r_hand", "r_hand", NULL, NULL}, {false, false, false, false} },
	// LHAND: tag surface "*l_hand"
	{ {"*l_hand", "l_hand", NULL, NULL}, {false, false, false, false} },
	// RHIP: tag surface "*hip_r", then "*hip_fr"
	{ {"*hip_r", "*hip_fr", NULL, NULL}, {false, false, false, false} },
	// LHIP: tag surface "*hip_l", then "*hip_fl"
	{ {"*hip_l", "*hip_fl", NULL, NULL}, {false, false, false, false} },
	// BACK: tag surface "*back"
	{ {"*back", "thoracic", NULL, NULL}, {false, true, false, false} },
};

// ============================================================================
// Weapon Dialog
// ============================================================================

// CLiveSaberColorDialog: CColorDialog that live-updates the viewport while
// the user drags the hue/RGB picker, instead of only applying on OK. A short
// timer polls the dialog's R/G/B edit fields (well-known control IDs 706,
// 707, 708) and pushes the color into AppVars.saberCustomColor[hand] +
// kicks a ModelList_ForceRedraw. If the user cancels, we put the original
// color back.
class CLiveSaberColorDialog : public CColorDialog
{
public:
	CLiveSaberColorDialog(COLORREF clrInit, DWORD dwFlags, CWnd *pParentWnd, int iHand)
		: CColorDialog(clrInit, dwFlags, pParentWnd), m_iHand(iHand)
	{
		m_originalColor[0] = AppVars.saberCustomColor[iHand][0];
		m_originalColor[1] = AppVars.saberCustomColor[iHand][1];
		m_originalColor[2] = AppVars.saberCustomColor[iHand][2];
	}

	int m_iHand;
	byte m_originalColor[3];

	DECLARE_MESSAGE_MAP()
protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};

BEGIN_MESSAGE_MAP(CLiveSaberColorDialog, CColorDialog)
	ON_WM_TIMER()
END_MESSAGE_MAP()

BOOL CLiveSaberColorDialog::OnInitDialog()
{
	CColorDialog::OnInitDialog();
	SetTimer(1, 30, NULL);
	return TRUE;
}

void CLiveSaberColorDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1) {
		// 706/707/708 are the standard COLOR_RED/GREEN/BLUE edit-field IDs in
		// Win32's color dialog. Only push updates when all three parse cleanly
		// so we don't flicker when the user's mid-typing.
		BOOL bR = FALSE, bG = FALSE, bB = FALSE;
		int r = GetDlgItemInt(706, &bR, FALSE);
		int g = GetDlgItemInt(707, &bG, FALSE);
		int b = GetDlgItemInt(708, &bB, FALSE);
		if (bR && bG && bB) {
			if (r < 0) r = 0; else if (r > 255) r = 255;
			if (g < 0) g = 0; else if (g > 255) g = 255;
			if (b < 0) b = 0; else if (b > 255) b = 255;
			byte nR = (byte)r, nG = (byte)g, nB = (byte)b;
			if (AppVars.saberCustomColor[m_iHand][0] != nR ||
				AppVars.saberCustomColor[m_iHand][1] != nG ||
				AppVars.saberCustomColor[m_iHand][2] != nB)
			{
				AppVars.saberCustomColor[m_iHand][0] = nR;
				AppVars.saberCustomColor[m_iHand][1] = nG;
				AppVars.saberCustomColor[m_iHand][2] = nB;
				ModelList_ForceRedraw();
			}
		}
	}
	CColorDialog::OnTimer(nIDEvent);
}

class CWeaponDlg : public CDialog
{
public:
	CWeaponDlg(CWnd* pParent) : CDialog(IDD_WEAPON_DIALOG, pParent) {}

protected:
	CListBox m_weaponList;

	void PopulateColorCombo(int nID, int sel) {
		CComboBox *pCombo = (CComboBox*)GetDlgItem(nID);
		if (!pCombo) return;
		pCombo->AddString("Blue");
		pCombo->AddString("Green");
		pCombo->AddString("Yellow");
		pCombo->AddString("Orange");
		pCombo->AddString("Red");
		pCombo->AddString("Purple");
		pCombo->AddString("Custom...");
		pCombo->SetCurSel(sel);
	}

	virtual BOOL OnInitDialog() {
		CDialog::OnInitDialog();
		m_weaponList.SubclassDlgItem(IDC_WEAPON_LIST, this);

		ScanWeapons();
		for (int i = 0; i < g_weaponList.GetSize(); i++) {
			CString display;
			display.Format("%s / %s", (LPCSTR)g_weaponList[i].displayName,
				(LPCSTR)Filename_WithoutPath(g_weaponList[i].fullPath));
			int idx = m_weaponList.AddString(display);
			m_weaponList.SetItemData(idx, i);
		}
		if (m_weaponList.GetCount() > 0) m_weaponList.SetCurSel(0);

		CheckDlgButton(IDC_SABER_RBLADE, AppVars.bSaberBlade[0] ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_SABER_LBLADE, AppVars.bSaberBlade[1] ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_SABER_COLLISION_FX, AppVars.bSaberCollisionFX ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_SABER_FIX_SPARKS_OFFSET, AppVars.bFixSaberClashOffset ? BST_CHECKED : BST_UNCHECKED);

		PopulateColorCombo(IDC_SABER_RCOMBO, AppVars.saberColorIndex[0]);
		PopulateColorCombo(IDC_SABER_LCOMBO, AppVars.saberColorIndex[1]);
		return TRUE;
	}

	void BoltSelected(int location) {
		int sel = m_weaponList.GetCurSel();
		if (sel == LB_ERR) return;
		int weapIdx = (int)m_weaponList.GetItemData(sel);
		if (weapIdx < 0 || weapIdx >= g_weaponList.GetSize()) return;

		ModelHandle_t hModel = Model_GetPrimaryHandle();
		if (!hModel) return;

		BoltLocationDef &loc = g_boltLocations[location];
		for (int n = 0; loc.names[n] != NULL; n++) {
			if (Model_LoadBoltOn(g_weaponList[weapIdx].fullPath, hModel, loc.names[n], loc.isBone[n], true))
				break;
		}
		GetParent()->Invalidate(false);
	}

	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam) {
		switch (LOWORD(wParam)) {
		case IDC_WEAPON_RHAND: BoltSelected(WEAPON_LOC_RHAND); return TRUE;
		case IDC_WEAPON_LHAND: BoltSelected(WEAPON_LOC_LHAND); return TRUE;
		case IDC_WEAPON_RHIP:  BoltSelected(WEAPON_LOC_RHIP);  return TRUE;
		case IDC_WEAPON_LHIP:  BoltSelected(WEAPON_LOC_LHIP);  return TRUE;
		case IDC_WEAPON_BACK:  BoltSelected(WEAPON_LOC_BACK);   return TRUE;

		case IDC_WEAPON_REMOVE_ALL: {
			ModelHandle_t hModel = Model_GetPrimaryHandle();
			if (hModel) {
				// Remove all surface-bolted models (weapons are bolted to tag surfaces)
				ModelContainer_t *pC = ModelContainer_FindFromModelHandle(hModel);
				if (pC) {
					for (int b = pC->iSurfaceBolt_MaxBoltPoints - 1; b >= 0; b--) {
						while (!pC->tSurfaceBolt_BoltPoints[b].vBoltedContainers.empty()) {
							Model_DeleteBoltOn(hModel, b, false, 0);
						}
					}
				}
				GetParent()->Invalidate(false);
			}
			return TRUE;
		}

		case IDC_SABER_RBLADE:
			AppVars.bSaberBlade[0] = (IsDlgButtonChecked(IDC_SABER_RBLADE) == BST_CHECKED);
			GetParent()->Invalidate(false);
			return TRUE;
		case IDC_SABER_LBLADE:
			AppVars.bSaberBlade[1] = (IsDlgButtonChecked(IDC_SABER_LBLADE) == BST_CHECKED);
			GetParent()->Invalidate(false);
			return TRUE;

		case IDC_SABER_COLLISION_FX:
			AppVars.bSaberCollisionFX = (IsDlgButtonChecked(IDC_SABER_COLLISION_FX) == BST_CHECKED);
			GetParent()->Invalidate(false);
			return TRUE;

		case IDC_SABER_FIX_SPARKS_OFFSET:
			AppVars.bFixSaberClashOffset = (IsDlgButtonChecked(IDC_SABER_FIX_SPARKS_OFFSET) == BST_CHECKED);
			GetParent()->Invalidate(false);
			return TRUE;

		case IDC_SABER_RCOLOR: {
			AppVars.saberColorIndex[0] = 6;  // switch to custom
			((CComboBox*)GetDlgItem(IDC_SABER_RCOMBO))->SetCurSel(6);
			CLiveSaberColorDialog dlg(RGB(AppVars.saberCustomColor[0][0], AppVars.saberCustomColor[0][1], AppVars.saberCustomColor[0][2]), CC_FULLOPEN | CC_RGBINIT, this, 0);
			INT_PTR result = dlg.DoModal();
			if (result == IDOK) {
				COLORREF c = dlg.GetColor();
				AppVars.saberCustomColor[0][0] = GetRValue(c); AppVars.saberCustomColor[0][1] = GetGValue(c); AppVars.saberCustomColor[0][2] = GetBValue(c);
			} else {
				// Cancel: put back the color the user had before opening the picker.
				AppVars.saberCustomColor[0][0] = dlg.m_originalColor[0];
				AppVars.saberCustomColor[0][1] = dlg.m_originalColor[1];
				AppVars.saberCustomColor[0][2] = dlg.m_originalColor[2];
			}
			GetParent()->Invalidate(false);
			return TRUE;
		}
		case IDC_SABER_LCOLOR: {
			AppVars.saberColorIndex[1] = 6;
			((CComboBox*)GetDlgItem(IDC_SABER_LCOMBO))->SetCurSel(6);
			CLiveSaberColorDialog dlg(RGB(AppVars.saberCustomColor[1][0], AppVars.saberCustomColor[1][1], AppVars.saberCustomColor[1][2]), CC_FULLOPEN | CC_RGBINIT, this, 1);
			INT_PTR result = dlg.DoModal();
			if (result == IDOK) {
				COLORREF c = dlg.GetColor();
				AppVars.saberCustomColor[1][0] = GetRValue(c); AppVars.saberCustomColor[1][1] = GetGValue(c); AppVars.saberCustomColor[1][2] = GetBValue(c);
			} else {
				AppVars.saberCustomColor[1][0] = dlg.m_originalColor[0];
				AppVars.saberCustomColor[1][1] = dlg.m_originalColor[1];
				AppVars.saberCustomColor[1][2] = dlg.m_originalColor[2];
			}
			GetParent()->Invalidate(false);
			return TRUE;
		}

		case IDC_SABER_RCOMBO:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				AppVars.saberColorIndex[0] = ((CComboBox*)GetDlgItem(IDC_SABER_RCOMBO))->GetCurSel();
				GetParent()->Invalidate(false);
			}
			return TRUE;
		case IDC_SABER_LCOMBO:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				AppVars.saberColorIndex[1] = ((CComboBox*)GetDlgItem(IDC_SABER_LCOMBO))->GetCurSel();
				GetParent()->Invalidate(false);
			}
			return TRUE;
		}
		return CDialog::OnCommand(wParam, lParam);
	}

	virtual void OnCancel() { DestroyWindow(); }
	virtual void PostNcDestroy();

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CWeaponDlg, CDialog)
END_MESSAGE_MAP()

static CWeaponDlg *g_pWeaponDlg = NULL;

void CWeaponDlg::PostNcDestroy() {
	g_pWeaponDlg = NULL;
	CDialog::PostNcDestroy();
	delete this;
}

void CMainFrame::OnViewWeapons()
{
	if (!Model_Loaded()) return;

	if (g_pWeaponDlg && IsWindow(g_pWeaponDlg->m_hWnd)) {
		g_pWeaponDlg->SetForegroundWindow();
		return;
	}

	g_pWeaponDlg = new CWeaponDlg(this);
	g_pWeaponDlg->Create(IDD_WEAPON_DIALOG, this);
	g_pWeaponDlg->ShowWindow(SW_SHOW);
}

void CMainFrame::OnWeaponMenuCommand(UINT nID)
{
	// No longer used - weapon dialog handles everything
}


// ============================================================================
// Animation Search Dialog - modeless dialog with live filtering
// ============================================================================

class CAnimSearchDlg : public CDialog
{
public:
	CAnimSearchDlg(CWnd* pParent) : CDialog(IDD_ANIM_SEARCH, pParent) {}

protected:
	CEdit		m_edit;
	CListBox	m_list;
	CStringArray m_allAnims;	// full list of animation names
	CArray<int, int> m_allIndices;	// corresponding sequence indices

	virtual BOOL OnInitDialog() {
		CDialog::OnInitDialog();
		m_edit.SubclassDlgItem(IDC_ANIM_SEARCH_EDIT, this);
		m_list.SubclassDlgItem(IDC_ANIM_SEARCH_LIST, this);

		PopulateFullList();
		FilterList("");
		m_edit.SetFocus();
		return FALSE;
	}

	void PopulateFullList() {
		m_allAnims.RemoveAll();
		m_allIndices.RemoveAll();

		ModelHandle_t hModel = Model_GetPrimaryHandle();
		if (!hModel) return;

		int n = Model_GetNumSequences(hModel);
		for (int i = 0; i < n; i++) {
			LPCSTR psName = Model_Sequence_GetName(hModel, i, true);
			if (psName && psName[0]) {
				m_allAnims.Add(psName);
				m_allIndices.Add(i);
			}
		}
	}

	void FilterList(LPCSTR psFilter) {
		m_list.ResetContent();
		CString strFilter(psFilter);
		strFilter.MakeLower();
		strFilter.TrimLeft(); strFilter.TrimRight();

		for (int i = 0; i < m_allAnims.GetSize(); i++) {
			CString strName(m_allAnims[i]);
			CString strLower(strName);
			strLower.MakeLower();

			bool bMatch = true;
			if (!strFilter.IsEmpty()) {
				bool hasWildcard = (strFilter.Find('*') >= 0);

				if (!hasWildcard) {
					// No wildcards: simple substring match (implicit *filter*)
					bMatch = (strLower.Find(strFilter) >= 0);
				} else {
					// Wildcards present: split by * and match parts in order
					bool leadingWild = (strFilter.GetAt(0) == '*');
					bool trailingWild = (strFilter.GetAt(strFilter.GetLength()-1) == '*');

					CStringArray parts;
					int start = 0;
					for (int c = 0; c <= strFilter.GetLength(); c++) {
						if (c == strFilter.GetLength() || strFilter.GetAt(c) == '*') {
							CString part = strFilter.Mid(start, c - start);
							if (!part.IsEmpty()) parts.Add(part);
							start = c + 1;
						}
					}

					if (parts.GetSize() == 0) {
						bMatch = true;
					} else {
						bMatch = true;
						int searchFrom = 0;
						for (int p = 0; p < parts.GetSize(); p++) {
							int found = strLower.Find(parts[p], searchFrom);
							if (found < 0) { bMatch = false; break; }
							if (p == 0 && !leadingWild && found != 0) { bMatch = false; break; }
							searchFrom = found + parts[p].GetLength();
						}
						if (bMatch && !trailingWild && parts.GetSize() > 0) {
							CString lastPart = parts[parts.GetSize()-1];
							if (strLower.GetLength() < lastPart.GetLength() ||
								strLower.Mid(strLower.GetLength() - lastPart.GetLength()) != lastPart)
								bMatch = false;
						}
					}
				}
			}

			if (bMatch) {
				int idx = m_list.AddString(strName);
				m_list.SetItemData(idx, m_allIndices[i]);
			}
		}
	}

	virtual BOOL PreTranslateMessage(MSG* pMsg) {
		if (pMsg->message == WM_KEYDOWN && pMsg->hwnd == m_edit.m_hWnd) {
			if (pMsg->wParam == VK_DOWN) {
				int sel = m_list.GetCurSel();
				int count = m_list.GetCount();
				if (sel < count - 1) m_list.SetCurSel(sel + 1);
				else if (sel == LB_ERR && count > 0) m_list.SetCurSel(0);
				return TRUE;
			}
			if (pMsg->wParam == VK_UP) {
				int sel = m_list.GetCurSel();
				if (sel > 0) m_list.SetCurSel(sel - 1);
				return TRUE;
			}
		}
		return CDialog::PreTranslateMessage(pMsg);
	}

	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam) {
		if (LOWORD(wParam) == IDC_ANIM_SEARCH_EDIT && HIWORD(wParam) == EN_CHANGE) {
			CString str;
			m_edit.GetWindowText(str);
			FilterList(str);
			return TRUE;
		}
		if (LOWORD(wParam) == IDC_ANIM_SEARCH_LIST && HIWORD(wParam) == LBN_DBLCLK) {
			int sel = m_list.GetCurSel();
			if (sel != LB_ERR) {
				int seqIndex = (int)m_list.GetItemData(sel);
				ModelHandle_t hModel = Model_GetPrimaryHandle();
				if (hModel) {
					Model_Sequence_Lock(hModel, seqIndex, true);
					extern void ModelList_Rewind(void);
					ModelList_Rewind();
					GetParent()->Invalidate(false);
				}
			}
			return TRUE;
		}
		return CDialog::OnCommand(wParam, lParam);
	}

	virtual void OnOK() {
		// Enter key: select current item instead of closing
		int sel = m_list.GetCurSel();
		if (sel != LB_ERR) {
			int seqIndex = (int)m_list.GetItemData(sel);
			ModelHandle_t hModel = Model_GetPrimaryHandle();
			if (hModel) {
				Model_Sequence_Lock(hModel, seqIndex, true);
				extern void ModelList_Rewind(void);
				ModelList_Rewind();
				GetParent()->Invalidate(false);
			}
		}
	}

	virtual void OnCancel() {
		DestroyWindow();
	}

	virtual void PostNcDestroy();	// implemented after g_pAnimSearchDlg declaration

	// Make the list resize with the dialog
	virtual void OnSize(UINT nType, int cx, int cy) {
		CDialog::OnSize(nType, cx, cy);
		if (m_edit.m_hWnd && m_list.m_hWnd) {
			m_edit.MoveWindow(40, 5, cx - 47, 14);
			m_list.MoveWindow(7, 23, cx - 14, cy - 30);
		}
	}

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CAnimSearchDlg, CDialog)
	ON_WM_SIZE()
END_MESSAGE_MAP()

static CAnimSearchDlg *g_pAnimSearchDlg = NULL;

void CAnimSearchDlg::PostNcDestroy() {
	g_pAnimSearchDlg = NULL;
	CDialog::PostNcDestroy();
	delete this;
}

void CMainFrame::OnSequencesSearch()
{
	if (!Model_Loaded()) return;

	// If already open, bring to front
	if (g_pAnimSearchDlg && IsWindow(g_pAnimSearchDlg->m_hWnd)) {
		g_pAnimSearchDlg->SetForegroundWindow();
		return;
	}

	g_pAnimSearchDlg = new CAnimSearchDlg(this);
	g_pAnimSearchDlg->Create(IDD_ANIM_SEARCH, this);
	g_pAnimSearchDlg->ShowWindow(SW_SHOW);
}


void CMainFrame::OnViewBilinear() 
{
	AppVars.bBilinear = !AppVars.bBilinear;	
	TextureList_SetFilter();
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewBilinear(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bBilinear);
}

void CMainFrame::OnViewOrigin() 
{
	AppVars.bOriginLines = !AppVars.bOriginLines;
	m_splitter.Invalidate(false);	
}

void CMainFrame::OnUpdateViewOrigin(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bOriginLines);
}



LPCSTR GetYearAsString(void)
{
	static char sTemp[20];	
    time_t ltime;	

    time( &ltime );    
	
    struct tm *today = localtime( &ltime );    
	
	strftime( sTemp, sizeof(sTemp), "%Y", today );

	return &sTemp[0];
}


void CMainFrame::OnViewScreenshotFile()
{
	if (!Model_Loaded())
	{
		ErrorBox("No model loaded!");
		return;
	}

	// Build default filename from model name
	CString strDefault;
	strDefault.Format("%s.png", Filename_WithoutPath(Filename_PathOnly(Model_GetFullPrimaryFilename())));

	CFileDialog dlg(FALSE, "png", strDefault,
		OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
		"PNG Files (*.png)|*.png|All Files (*.*)|*.*||",
		this);
	dlg.m_ofn.lpstrTitle = "Save Transparent Screenshot";

	if (dlg.DoModal() == IDOK)
	{
		CWaitCursor wait;
		CString strPath = dlg.GetPathName();

		if (ScreenShotPNG_Transparent(strPath, g_iScreenWidth, g_iScreenHeight))
		{
			ModelList_Render(g_iScreenWidth, g_iScreenHeight);
		}
	}

	m_splitter.Invalidate(false);
}


   
void CMainFrame::OnViewScreenshotClipboard() 
{
	if (Model_Loaded())
	{
		CWaitCursor wait;

		// slightly iffy here, I'm going to assume that the rendering context is still valid from last time.
		// I can't do much else because I need to supply DC shit that I don't have in order to issue an OnDraw
		//	command to do it legally, so fuck it...
		//
		gbTextInhibit = AppVars.bCleanScreenShots;	//true;
		{
			ModelList_Render( g_iScreenWidth, g_iScreenHeight );	// render to back buffer

			ScreenShot(NULL,va("(C) Raven Software %s",GetYearAsString()));
		}
		gbTextInhibit = false;		

		void *pvDIB;
		int iBytes;
		if (BMP_GetMemDIB(pvDIB, iBytes))
		{
			ClipBoard_SendDIB(pvDIB, iBytes);
		}
		BMP_Free();
	}

	m_splitter.Invalidate(false);
}


void CMainFrame::OnEditCopy() 
{
	OnViewScreenshotClipboard();
}

void CMainFrame::OnEditPaste() 
{	
}

void CMainFrame::OnViewGlinfo() 
{
	InfoBox(va("%s",GL_GetInfo()));
}

void CMainFrame::OnAnimationStart() 
{
	Model_StartAnim();
}

void CMainFrame::OnAnimationStartwithwrapforce() 
{
	Model_StartAnim(true);
}

void CMainFrame::OnAnimationStop() 
{
	Model_StopAnim();
}

void CMainFrame::OnAnimationRewind() 
{
	ModelList_Rewind();
}

void CMainFrame::OnAnimationFaster() 
{
	AppVars.dAnimSpeed *= ANIM_FASTER;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnAnimationSlower()
{
	AppVars.dAnimSpeed *= ANIM_SLOWER;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateFileSave(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(false);	
}

void CMainFrame::OnUpdateFileSaveAs(CCmdUI* pCmdUI) 
{
//	pCmdUI->Enable(true);	
}

void CMainFrame::OnModelSaveAs()
{
	LPCSTR psFullPathedFilename = InputSaveFileName(va("%s",Filename_WithoutExt(Model_GetFullPrimaryFilename())),
													"Save Model",	// LPCSTR psCaption,
													Filename_PathOnly(Model_GetFullPrimaryFilename()),	//	"S:\\baseq3\\models\\test\\bonehier",	// LPCSTR psInitialDir, 
													Model_GetSupportedTypesFilter(true),			// LPCSTR psFilter
													".glm"
													);

	if (psFullPathedFilename)
	{
		Model_Save(psFullPathedFilename);
	}

}

void CMainFrame::OnFileWriteideal() 
{
	if (GetYesNo(va("Write \"<modelname>.ideal\" file\n\nAre you sure?")))
	{
		AppVars_WriteIdeal();
	}
}

void CMainFrame::OnFileReadideal() 
{
	AppVars_ReadIdeal();	
}


void CMainFrame::OnAnimationNextframe() 
{
	ModelList_StepFrame(1);
}

void CMainFrame::OnAnimationPrevframe() 
{
	ModelList_StepFrame(-1);	
}

void CMainFrame::OnViewLod0() 
{
	AppVars.iLOD = 0;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnViewLod1() 
{
	AppVars.iLOD = 1;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnViewLod2() 
{
	AppVars.iLOD = 2;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnViewLod3() 
{
	AppVars.iLOD = 3;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnViewLod4() 
{
	AppVars.iLOD = 4;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnViewLod5() 
{
	AppVars.iLOD = 5;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnViewLod6() 
{
	AppVars.iLOD = 6;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnViewLod7() 
{
	AppVars.iLOD = 7;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnEditBgrndcolour() 
{
	CHOOSECOLOR cc;
	static COLORREF  crefs[16];

	memset(&cc,0,sizeof(cc));

	cc.lStructSize	= sizeof(cc);
	cc.hwndOwner	= AppVars.hWnd;
//			cc.hInstance	= NULL;
	cc.lpCustColors	= crefs;
	cc.rgbResult	= AppVars._B<<16 | AppVars._G<<8 | AppVars._R;	//  COLORREF     rgbResult; 
	cc.Flags		= CC_RGBINIT | CC_ANYCOLOR | CC_SOLIDCOLOR | /*CC_FULLOPEN | */ 0;

	if (ChooseColor(&cc))
	{
		DWORD d = cc.rgbResult;				
		AppVars._B = (cc.rgbResult>>16) & 0xFF;
		AppVars._G = (cc.rgbResult>>8 ) & 0xFF;
		AppVars._R = (cc.rgbResult>>0 ) & 0xFF;

		m_splitter.Invalidate(false);
	} 
}

void CMainFrame::OnUpdateViewBonehilite(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bBoneHighlight);
}

void CMainFrame::OnViewBonehilite() 
{
	AppVars.bBoneHighlight = !AppVars.bBoneHighlight;
	m_splitter.Invalidate(false);	
}

void CMainFrame::OnViewNormals()
{
	AppVars.bVertexNormals = !AppVars.bVertexNormals;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewNormals(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(AppVars.bVertexNormals);
}

void CMainFrame::OnViewSurfacehilite() 
{
	AppVars.bSurfaceHighlight = !AppVars.bSurfaceHighlight;
	m_splitter.Invalidate(false);	
}

void CMainFrame::OnUpdateViewSurfacehilite(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( AppVars.bSurfaceHighlight );
}

void CMainFrame::OnViewSurfacehilitewithbonerefs() 
{
	AppVars.bSurfaceHighlightShowsBoneWeighting = !AppVars.bSurfaceHighlightShowsBoneWeighting;
	m_splitter.Invalidate(false);
}

// only allow this option if surface highlighting is on...
//
void CMainFrame::OnUpdateViewSurfacehilitewithbonerefs(CCmdUI* pCmdUI)
{
	pCmdUI->Enable	( AppVars.bSurfaceHighlight );
	pCmdUI->SetCheck( AppVars.bSurfaceHighlightShowsBoneWeighting );
}

void CMainFrame::OnViewVertindexes()
{
	AppVars.bVertIndexes = !AppVars.bVertIndexes;	
	m_splitter.Invalidate(false);
}

// only allow this option if surface highlighting is on...
//
void CMainFrame::OnUpdateViewVertindexes(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable	( AppVars.bSurfaceHighlight );	
	pCmdUI->SetCheck( AppVars.bVertIndexes );
}

void CMainFrame::OnViewFovcycle() 
{
	AppVars.dFOV = (AppVars.dFOV == 10.0f)?80.0f:(AppVars.dFOV == 80.0f)?90:10.0f;
	m_splitter.Invalidate(false);
}


void CMainFrame::OnUpdateEditPaste(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(false);
}

void CMainFrame::OnFileRefreshtextures()
{
	KillAllShaderFiles();	// clear cached shader text + shader registry so they reload
	TextureList_Refresh();
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateFilePrint(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(false);	
}

void CMainFrame::OnUpdateFilePrintPreview(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(false);	
}

void CMainFrame::OnUpdateFilePrintSetup(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(false);	
}

void CMainFrame::OnUpdateEditTestfunction(CCmdUI* pCmdUI) 
{
//	pCmdUI->Enable(!_stricmp(scGetComputerName(),"SPOCK"));
	pCmdUI->Enable(!_stricmp(scGetUserName(),"scork"));
}


#include "jpeg_interface.h"
#include "Splash.h"
void CMainFrame::OnEditTestfunction() 
{
	XLS_To_SP();

#if 0
	// runtime watermarking stuff...
	//
	SetQdirFromPath( "w:/game/base/blah" );

	LPCSTR psTexturename = "textures/tests/ste_special_test_data/merk3";//blank";//merk3";
	TextureHandle_t hTexture = TextureHandle_ForName( psTexturename );
	if (hTexture == -1)
	{
		hTexture = Texture_Load( psTexturename );
	}

	Texture_t *pTexture = Texture_GetTextureData( hTexture );	
	
	//DStamp_MarkImage(pTexture, "JDoePCMagazine");
	DStamp_AnalyseImage(pTexture);


	LPCSTR psOutputName = "c:\\test.tga";
	TGA_Write(psOutputName, pTexture->pPixels, pTexture->iWidth, pTexture->iHeight, 32); 

	// load it back in again...
	//
/*	byte *pPixels = NULL;
	int iWidth,iHeight;

//	LoadJPG( "textures/tests/ste_special_test_data/redlineattop.jpg", &pPixels, &iWidth, &iHeight );
//	LoadTGA( "textures/tests/ste_special_test_data/redlineattop.tga", &pPixels, &iWidth, &iHeight );	
		
	extern bool bHackToAllowFullPathDuringTestFunc;
				bHackToAllowFullPathDuringTestFunc = true;
				{
//					LoadJPG( "c:\\test.jpg", &pPixels, &iWidth, &iHeight );
//					SaveFile("c:\\test_jpg.bin", pPixels, iWidth * iHeight * 4);				

					LoadTGA( psOutputName, &pPixels, &iWidth, &iHeight );
//					TGA_Write("c:\\test2.tga", pPixels, iWidth, iHeight, 32); 
//					SaveFile("c:\\test_tga.bin", pPixels, iWidth * iHeight * 4);
				}
				bHackToAllowFullPathDuringTestFunc = false;

	if (pPixels)
	{
		LPCSTR psMessage = DStamp_ReadImage(pPixels, iWidth, iHeight, 32);

		if (psMessage)
		{
			InfoBox(va("Found DStamp:\n\n\"%s\"",psMessage));
		}
		else
		{
			ErrorBox("No Dstamp found");
		}
		

		free(pPixels);
	}

//	PNG_Save("c:\\test.png", pTexture->pPixels, pTexture->iWidth, pTexture->iHeight, 4);
*/
/*	CWaitCursor wait;

	if (Model_LoadPrimary("S:\\base\\models\\test\\jake\\jake.glm"))
	{
		if (Model_LoadBoltOn(	"S:\\base\\models\\test\\conetree4\\conetree4.glm",
								AppVars.hModelLastLoaded, 
								"boltpoint_righthand",
								true		// bBoltIsBone
								)
			)
		{
			if (Model_LoadBoltOn( 	"S:\\base\\models\\test\\cube\\cube.glm",
									AppVars.hModelLastLoaded, 
									"s_branch3_twig3",
									true	// bBoltIsBone
								)
				)
			{
				// ... 
			}
		}
	}
*/
#endif
}

void CMainFrame::OnFileResetviewparams() 
{
	AppVars_ResetViewParams();
	m_splitter.Invalidate(false);
}


void CMainFrame::OnUpdateFileWritescript(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(Model_Loaded());
}

extern void Filename_AddToMRU(LPCSTR psFilename);
void CMainFrame::OnFileWritescript() 
{
	LPCSTR psFullPathedFilename = InputSaveFileName(va("%s%s",Filename_WithoutExt(Model_GetFullPrimaryFilename()),Script_GetExtension()),	// LPCSTR psInitialSaveName, 
													"Write Script",			// LPCSTR psCaption, 
													Filename_PathOnly(Model_GetFullPrimaryFilename()),	//LPCSTR psInitialPath,
													Script_GetFilter(),		// LPCSTR psFilter
													Script_GetExtension()	// LPCSTR psExtension
													);
	if (psFullPathedFilename)
	{
		CWaitCursor wait;

		if (Script_Write(psFullPathedFilename))
		{				
			Filename_AddToMRU(psFullPathedFilename);
		}
	}	
}

void CMainFrame::OnFileReadscript() 
{
	LPCSTR psFullPathedFilename = InputLoadFileName("",				// LPCSTR psInitialLoadName, 
													"Read Script",	// LPCSTR psCaption,
													Filename_PathOnly(Model_GetFullPrimaryFilename()),	//	"S:\\baseq3\\models\\test\\bonehier",	// LPCSTR psInitialDir, 
													Script_GetFilter()			// LPCSTR psFilter
													);
	if (psFullPathedFilename)
	{
		CWaitCursor wait;

		if (Script_Read(psFullPathedFilename))
		{				
			Filename_AddToMRU(psFullPathedFilename);
		}
	}
}


void CMainFrame::OnViewTagsurfaces() 
{
	AppVars.bShowTagSurfaces = !AppVars.bShowTagSurfaces;
	m_splitter.Invalidate(false);	
}

void CMainFrame::OnUpdateViewTagsurfaces(CCmdUI* pCmdUI) 
{		
	pCmdUI->SetCheck( AppVars.bShowTagSurfaces );
}

void CMainFrame::OnViewTagsasrgb() 
{
	AppVars.bShowOriginsAsRGB = !AppVars.bShowOriginsAsRGB;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewTagsasrgb(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( AppVars.bShowOriginsAsRGB );
}

void CMainFrame::OnPicmip0()
{
	TextureList_ReMip(0);
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdatePicmip0(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TextureList_GetMip() == 0);
}

void CMainFrame::OnPicmip1()
{
	TextureList_ReMip(1);
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdatePicmip1(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TextureList_GetMip() == 1);
}

void CMainFrame::OnPicmip2()
{
	TextureList_ReMip(2);
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdatePicmip2(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(TextureList_GetMip() == 2);
}

void CMainFrame::OnPicmip3()
{
	TextureList_ReMip(3);
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdatePicmip3(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(TextureList_GetMip() == 3);
}

void CMainFrame::OnPicmip4()
{
	TextureList_ReMip(4);
	m_splitter.Invalidate(false);
}

void CMainFrame::OnPicmip5()
{
	TextureList_ReMip(5);
	m_splitter.Invalidate(false);
}

void CMainFrame::OnPicmip6()
{
	TextureList_ReMip(6);
	m_splitter.Invalidate(false);
}

void CMainFrame::OnPicmip7()
{
	TextureList_ReMip(7);
	m_splitter.Invalidate(false);
}

void CMainFrame::OnViewRuler() 
{
	AppVars.bRuler = !AppVars.bRuler;
	m_splitter.Invalidate(false);	
}

void CMainFrame::OnUpdateViewRuler(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bRuler);
}


// psMessage can be NULL to mean "pick your own 'ready' message"...
//
extern bool gbMainFrameInitialised;
extern bool gbSplashScreenRunning;
void CMainFrame::StatusMessage(LPCTSTR psMessage)
{
	if (this && gbMainFrameInitialised)
	{
		extern int giGalleryItemsRemaining;
		m_wndStatusBar.SetWindowText((psMessage && psMessage[0])?psMessage:(Gallery_Active()?va("( Gallery: %d remaining )", giGalleryItemsRemaining):"Ready"));
	}
	else
	{
		if (gbSplashScreenRunning)
		{
			CSplashWnd::StatusMessage(psMessage);
		}
	}
}

void CMainFrame::OnViewForcewhite() 
{
	AppVars.bForceWhite = !AppVars.bForceWhite;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewForcewhite(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bForceWhite);
}

void CMainFrame::OnViewScreenshotClean() 
{
	AppVars.bCleanScreenShots =	!AppVars.bCleanScreenShots;
//	m_splitter.Invalidate(false);	// not needed for this bool
}

void CMainFrame::OnUpdateViewScreenshotClean(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bCleanScreenShots);
}

void CMainFrame::OnViewVertweighting() 
{
	AppVars.bVertWeighting = !AppVars.bVertWeighting;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewVertweighting(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( AppVars.bVertWeighting );
	pCmdUI->Enable  ( AppVars.bVertIndexes );
}

void CMainFrame::OnViewBbox() 
{
	AppVars.bBBox = !AppVars.bBBox;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewBbox(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bBBox);
}


void CMainFrame::OnViewFloor()
{
	AppVars.bFloor = !AppVars.bFloor;
	// Snap the floor to the loaded model's lowest point so it lands at the
	// character's feet instead of floating 24 units below (the arbitrary
	// default). Matches what "Edit -> Set Floor to Current" does manually,
	// and keeps saber-vs-floor collisions reachable for normal attack
	// animations.
	if (AppVars.bFloor && Model_Loaded()) {
		AppVars.fFloorZ = Model_GetLowestPointOnPrimaryModel();
	}
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewFloor(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bFloor);
}

void CMainFrame::OnEditSetfloorAbs()
{
	LPCSTR psPrompt = "Input new floor value, e.g. \"-50\"";
	LPCSTR psFloorZ = GetString(psPrompt);
	
	if (psFloorZ)
	{
		AppVars.fFloorZ = atof(psFloorZ);
		m_splitter.Invalidate(false);
	}	
}

void CMainFrame::OnUpdateEditSetfloorAbs(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(AppVars.bFloor && Model_Loaded());
}

void CMainFrame::OnEditSetfloorCurrent() 
{
	AppVars.fFloorZ = Model_GetLowestPointOnPrimaryModel();
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateEditSetfloorCurrent(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(AppVars.bFloor && Model_Loaded());
}

void CMainFrame::OnViewBonefiltering() 
{
	AppVars.bBoneWeightThreshholdingActive = !AppVars.bBoneWeightThreshholdingActive;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewBonefiltering(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bBoneWeightThreshholdingActive);
}

void CMainFrame::OnEditSetboneweightThreshhold() 
{
	bool bAgain = true;

	do
	{	
		LPCSTR psPrompt = "Input new bone-weight-threshhold percentage, e.g. \"50\", which will cause all bone-weights below that value to be ignored\n\n( Input must be within the range 0..100 )";
		LPCSTR psPercent= GetString(psPrompt);

		if (psPercent)
		{
			AppVars.fBoneWeightThreshholdPercent = atof(psPercent);
			if (AppVars.fBoneWeightThreshholdPercent < 0.0f
				||
				AppVars.fBoneWeightThreshholdPercent > 100.0f
				)
			{
				PLAY_LAME_WAV;
				ErrorBox(va("%f is not within the range 0..100 now, is it?\n\nDuh!!!!!!!!!!!",AppVars.fBoneWeightThreshholdPercent));				
			}
			else
			{
				bAgain = false;
				m_splitter.Invalidate(false);
			}
		}
	}
	while (bAgain);
}

void CMainFrame::OnUpdateEditSetboneweightThreshhold(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(AppVars.bBoneWeightThreshholdingActive && Model_Loaded());	//model_loaded check not nec., but matches others on menu
}


void CMainFrame::OnEditBoneFilterINCThreshhold()
{
	if (AppVars.bBoneWeightThreshholdingActive)
	{
		AppVars.fBoneWeightThreshholdPercent += 1.0f;
		if (AppVars.fBoneWeightThreshholdPercent > 100.0f)
			AppVars.fBoneWeightThreshholdPercent = 100.0f;

		m_splitter.Invalidate(false);
	}			
}

void CMainFrame::OnEditBoneFilterDECThreshhold()
{
	if (AppVars.bBoneWeightThreshholdingActive)
	{
		AppVars.fBoneWeightThreshholdPercent -= 1.0f;
		if (AppVars.fBoneWeightThreshholdPercent < 0.0f)
			AppVars.fBoneWeightThreshholdPercent = 0.0f;

		m_splitter.Invalidate(false);
	}			
}


void CMainFrame::OnViewCrackviewer() 
{
	if (!_stricmp(scGetUserName(),"scork"))
	{
		AppVars.bCrackHighlight = !AppVars.bCrackHighlight;
		m_splitter.Invalidate(false);
	}
}

void CMainFrame::OnUpdateViewCrackviewer(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bCrackHighlight);
	pCmdUI->Enable(!_stricmp(scGetUserName(),"scork"));
}

void CMainFrame::OnViewUnshadowablesurfaces() 
{
	AppVars.bShowUnshadowableSurfaces = !AppVars.bShowUnshadowableSurfaces;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewUnshadowablesurfaces(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck( AppVars.bShowUnshadowableSurfaces );	
}

#define sSOF2BASEDIR "s:\\base\\"
void CMainFrame::OnFileViewSof2Npcs() 
{	
	if (!gamedir[0])
	{
		LPCSTR psBasePathToCopy = sSOF2BASEDIR;
		if (!GetYesNo("Warning: base path not known yet, shall I assume ' " sSOF2BASEDIR " ' ?"))
		{
			psBasePathToCopy = GetString("Enter suitable base path\n\n( format example: \"" sSOF2BASEDIR "\" )");
			if (!psBasePathToCopy)
				return;
		}

		strcpy(gamedir,psBasePathToCopy);
	}

	CString strScript;
	CSOF2NPCViewer Viewer(true, &strScript, gamedir);	

	Model_StopAnim();	// or the screen update stops the GDI stuff from showing up

	if (Viewer.DoModal() == IDOK)
	{
		if (Gallery_Active())
		{
// pick this up in the timer loop now...
//
//			CString strCaption;
//			while (GalleryRead_ExtractEntry(strCaption, strScript))
//			{
//				OutputDebugString(va("\"%s\" (script len %d)\n",(LPCSTR)strCaption,strScript.GetLength()));
//			}
			extern CString strGalleryErrors;
			extern CString strGalleryWarnings;
			extern CString strGalleryInfo;

			strGalleryErrors = strGalleryWarnings = strGalleryInfo = "";
			Model_StopAnim();
			gbTextInhibit = AppVars.bCleanScreenShots;	//true;
			return;
		}
		else
		{
			// normal double-click on a single template list entry...
			//
			if (!strScript.IsEmpty())
			{
				strScript += "\n\n\n";
				//SendStringToNotepad(strScript,"temp.txt");

				string strOutputFileName( va("%s\\%s",scGetTempPath(),"temp.mvs") );
				
				int iReturn = SaveFile(strOutputFileName.c_str(),(LPCSTR)strScript, strScript.GetLength());
				if (iReturn != -1)
				{
					extern bool Document_ModelLoadPrimary(LPCSTR psFilename);
					Document_ModelLoadPrimary(strOutputFileName.c_str());
				}
			}
		}
	}
}

void CMainFrame::OnUpdateFileViewSof2Npcs(CCmdUI* pCmdUI) 
{
}

void CMainFrame::OnEditAllowskeletonoverrides() 
{
	AppVars.bAllowGLAOverrides = !AppVars.bAllowGLAOverrides;	
}

void CMainFrame::OnUpdateEditAllowskeletonoverrides(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bAllowGLAOverrides);	
}

void CMainFrame::OnViewDoublesidedpolys() 
{
	AppVars.bShowPolysAsDoubleSided = !AppVars.bShowPolysAsDoubleSided;
	m_splitter.Invalidate(false);
}

void CMainFrame::OnUpdateViewDoublesidedpolys(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bShowPolysAsDoubleSided);
}










void GetUsedIDs(set <int> &UsedIDs, LPCSTR psLocalDir)
{
	char **ppsFiles;
	int iFiles;

	// scan for skin files...
	//
	ppsFiles =	Sys_ListFiles(	psLocalDir,	// const char *directory, 
								".sp",		// const char *extension, 
								NULL,		// char *filter, 
								&iFiles,	// int *numfiles, 
								qfalse		// qboolean wantsubs 
								);

	if ( !ppsFiles || !iFiles )
	{
		return;
	}

	// load and parse files...
	//
	for ( int i=0; i<iFiles; i++ )
	{
		char *psText = NULL;

		CString strFileName(va("%s\\%s",psLocalDir,ppsFiles[i]));
		int iBytesLoaded = LoadFile(strFileName, (void **)&psText, true);
		if (iBytesLoaded != -1)
		{
			CString strTemp(psText);
			free(psText);

			int iLoc = strTemp.Find("\nID ");
			if (iLoc != -1)
			{
				LPCSTR pID = &((LPCSTR)strTemp)[iLoc+4];
				int iID = atoi(pID);
				UsedIDs.insert(iID);
			}
		}
	}

	Sys_FreeFileList( ppsFiles );
}

int GetUniqueStripEdID( LPCSTR psLocalWorkDir, LPCSTR psNetDir)
{
	set <int> UsedIDs;

	GetUsedIDs(UsedIDs, psLocalWorkDir);
	GetUsedIDs(UsedIDs, psNetDir);

	for (int iID=1; iID<256; iID++)
	{
		if (!UsedIDs.count(iID))
			return iID;
	}

	ErrorBox("Unable to find unique ID, all in use!\n");
	return 0;
}




typedef struct
{
	CString strLabel;
	int iLevel;
	CString strChar;
	int iCharSpeechNum;
} SpeechLabel_t;

typedef struct
{
	SpeechLabel_t SpeechLabel;
	
	CString strCharacter;
	CString strSpeech;

} Speech_t;

typedef vector <Speech_t>		Speeches_t;
typedef map <int, Speeches_t>	SpeechesPerLevel_t;
								SpeechesPerLevel_t SpeechesPerLevel;

static void XLS_To_SP(void)
{		
	SpeechesPerLevel.clear();

#define sWORKDIR "c:\\JK2Temp"
#define sNETDIR  "w:\\game\\base\\strip"

	CString strFileName_ExportedXLS		(sWORKDIR "\\JK2_final_script.txt");
//	CString strFileName_XRefLevelnames	(sWORKDIR "\\JK2_LevelNamesInOrder.txt");


	LPCSTR psError = NULL;
	char *psData_XLS = NULL;
	
	StatusMessage(va("Reading \"%s\"\n",(LPCSTR) strFileName_ExportedXLS));
	int iSize = LoadFile(strFileName_ExportedXLS, (void**)&psData_XLS);
	if (iSize != -1)
	{
		CWaitCursor wait;

		CString strXLS(psData_XLS);
		free(psData_XLS);

		// heh...
		//
		CString strStatsOnly_LineCount(strXLS);
		int iStatsOnly_LinesRemaining = strStatsOnly_LineCount.Replace("\n","");
		strStatsOnly_LineCount.Empty();	// tiny mem opt


		while (!strXLS.IsEmpty())
		{
			iStatsOnly_LinesRemaining--;

			CString strThisLine;

			int iLoc = strXLS.Find('\n');
			if (iLoc != -1)
			{
				strThisLine =	strXLS.Left(iLoc);
								strXLS = strXLS.Mid(iLoc+1);
			}
			else
			{
				strThisLine = strXLS;
				strXLS.Empty();
			}

			strThisLine.TrimRight();
			strXLS.TrimRight();

			if (!strThisLine.IsEmpty())
			{	
				CString strThisLineOriginalForErrorReporting(strThisLine);

				// line example:
				//
				// 02KYK004	Kyle	Jan?  I�ve found an exit.				
				//

				// get the label...
				//
				iLoc = strThisLine.FindOneOf(" \t");
				if (iLoc != -1)
				{
					CString strXLS_Label(strThisLine.Left(iLoc));
					strThisLine = strThisLine.Mid(iLoc);
					strThisLine.TrimLeft();

					// get the character name...
					//					
					iLoc = strThisLine.FindOneOf(" \t");
					if (iLoc != -1)
					{
						CString strXLS_Character(strThisLine.Left(iLoc));
						strThisLine = strThisLine.Mid(iLoc);
						strThisLine.TrimLeft();

						// get the speech...
						//
						CString strXLS_Speech(strThisLine);

						//strXLS_Speech.Replace("\"","");
						strXLS_Speech.TrimLeft();
						strXLS_Speech.TrimRight();

						if (!strXLS_Speech.IsEmpty())
						{
							// Lucas use a lot of stupid non-ascii chars, so we have to correct them...
							//
							// (smart quotes are left in after conversion, since they're normally for
							//	stuff like {don't say "hello" to me}
							//
							// TCHAR versions of these won't fucking compile (thanks yet again, MS)
							//
							strXLS_Speech.Replace(va("%c",0x93),"\"");			// smart quotes -> '"'
							strXLS_Speech.Replace(va("%c",0x94),"\"");			// smart quotes -> '"'
							strXLS_Speech.Replace(va("%c",0x0B),".");			// full stop
							strXLS_Speech.Replace(va("%c",0x85),"...");			// "..."-char ->  3-char "..."
							strXLS_Speech.Replace(va("%c",0x91),va("%c",0x27));	// "'"
							strXLS_Speech.Replace(va("%c",0x92),va("%c",0x27));	// "'"
							strXLS_Speech.Replace(va("%c",0x96),va("%c",0x2D));	// "-"
							strXLS_Speech.Replace(va("%c",0x97),va("%c",0x2D));	// "-"

							// remove any leading or trailing double-quotes, but leave any in the middle,
							//	so don't just blanket-delete them...
							//
							while (strXLS_Speech.GetAt(0) == '"')
							{
								strXLS_Speech = strXLS_Speech.Mid(1);
							}

							while (strXLS_Speech.GetAt(strXLS_Speech.GetLength()-1) == '"')
							{
								strXLS_Speech = strXLS_Speech.Left(strXLS_Speech.GetLength()-1);
							}


							// find any more crap lurking...
							//
							for (int i=0; i<strXLS_Speech.GetLength(); i++)
							{
								if (!__isascii(strXLS_Speech.GetAt(i)))
								{
									int z=1;
								}
								
								if (strXLS_Speech.GetAt(i) > 127 )
								{
									int z=1;
								}
							}

							if (!strXLS_Speech.IsEmpty())
							{
	//							OutputDebugString(va("Label: \"%s\", Char: \"%s\", Speech: \"%s\"\n",(LPCSTR)strXLS_Label, (LPCSTR)strXLS_Character, (LPCSTR) strXLS_Speech ));


								// now enter into table...
								//
								// CString strXLS_Label;
								// CString strXLS_Character;
								// CString strXLS_Speech;
								//
								// first, decode label into components...
								//
								CString strTemp = strXLS_Label.Left(2);
								int iLabel_Level = atoi(strTemp);

								CString strLabel_Char = strXLS_Label.Mid(2);
										strLabel_Char = strLabel_Char.Left(3);

								strTemp = strXLS_Label.Mid(5);
								int iLabel_CharSpeechNum = atoi(strTemp);

	//							OutputDebugString(va("Level: %d, Char: \"%s\", Speech %d\n",iLabel_Level,(LPCSTR)strLabel_Char, iLabel_CharSpeechNum));

								Speech_t Speech;							
								
								Speech.SpeechLabel.strLabel			= strXLS_Label;
								Speech.SpeechLabel.iLevel			= iLabel_Level;
								Speech.SpeechLabel.strChar			= strLabel_Char;
								Speech.SpeechLabel.iCharSpeechNum	= iLabel_CharSpeechNum;
								Speech.strCharacter = strXLS_Character;
								Speech.strSpeech	= strXLS_Speech;

								SpeechesPerLevel[iLabel_Level].push_back(Speech);

								StatusMessage(va("Lines remaining %d\n",iStatsOnly_LinesRemaining));
							}
							else
							{
								WarningBox(va("Unable to get speech from line:\n%s\n",(LPCSTR)strThisLineOriginalForErrorReporting));
							}
						}
						else
						{
							WarningBox(va("Unable to get speech from line:\n%s\n",(LPCSTR)strThisLineOriginalForErrorReporting));
						}
					}
					else
					{
						WarningBox(va("Unable to get character name from line:\n%s\n",(LPCSTR)strThisLineOriginalForErrorReporting));
					}
				}
				else
				{
					WarningBox(va("Unable to get label from line:\n%s\n",(LPCSTR)strThisLineOriginalForErrorReporting));
				}
			}
		}

		StatusMessage("All speeches read\n");

		// read in the levelnames cross-reference file...
		//
		CString strFileName_LevelNames(sWORKDIR "\\levelnames.txt");

		char *psData_LevelNames = NULL;
		
		StatusMessage(va("Reading \"%s\"\n",(LPCSTR) strFileName_LevelNames));
		iSize = LoadFile(strFileName_LevelNames, (void**)&psData_LevelNames);
		if (iSize != -1)
		{
			CWaitCursor wait;

			typedef map <int, CString> LevelNames_t; LevelNames_t LevelNames;

			CString strLevelNames(psData_LevelNames);
			free(psData_LevelNames);

			while (!strLevelNames.IsEmpty())
			{
				CString strThisLine;

				int iLoc = strLevelNames.Find('\n');
				if (iLoc != -1)
				{
					strThisLine =	strLevelNames.Left(iLoc);
									strLevelNames = strLevelNames.Mid(iLoc+1);
				}
				else
				{
					strThisLine = strLevelNames;
					strLevelNames.Empty();
				}

				if (!strThisLine.IsEmpty())
				{
					// format example:
					//
					// 01 kejim_post	
					//
					strThisLine.TrimLeft();
					strThisLine.TrimRight();

					int iLevelNumber = atoi(strThisLine);

					iLoc = strThisLine.FindOneOf(" \t");
					if (iLoc != -1)
					{
						strThisLine = strThisLine.Mid(iLoc);
						strThisLine.TrimLeft();
						strThisLine.MakeLower();

						LevelNames[iLevelNumber] = strThisLine;
					}
				}
			}		

			// now write them out as SP files...
			//
			for (SpeechesPerLevel_t::iterator itLevel = SpeechesPerLevel.begin(); itLevel != SpeechesPerLevel.end(); ++itLevel)
			{
				int iLevel			= (*itLevel).first;
				Speeches_t &Speeches= (*itLevel).second;

				// simple stuff for now...
				//
				if (Speeches.size() > 256 )
				{
					int z=1;
				}
				else
				{
					CString strStripEdFileNameBase((LevelNames.find(iLevel) != LevelNames.end())?(LPCSTR)LevelNames[iLevel]:"_unknown");

					CString strStripEdText;

					StatusMessage(va("Constructing SP file \"%s\"\n",(LPCSTR)strStripEdFileNameBase));

					// some basic stuff first...
					//				
					strStripEdText += "VERSION 1\n";
					strStripEdText += "CONFIG W:\\bin\\striped.cfg\n";
					strStripEdText += va("ID %d\n",GetUniqueStripEdID( sWORKDIR, sNETDIR ));
					strStripEdText += va("REFERENCE %s\n",String_ToUpper(strStripEdFileNameBase));
					strStripEdText += va("DESCRIPTION \"%s\"\n",String_ToLower(strStripEdFileNameBase));
					strStripEdText += va("COUNT %d\n",Speeches.size());


					for (int iSpeech=0; iSpeech < Speeches.size(); iSpeech++)
					{
						Speech_t &Speech = Speeches[iSpeech];

	//					OutputDebugString(va("%d/%d: \"%s\"\n",iSpeech,Speeches.size(),(LPCSTR) Speeches[iSpeech].strSpeech));

						StatusMessage(va("%s: Adding string %d\n",(LPCSTR)strStripEdFileNameBase,iSpeech));

						strStripEdText += va("INDEX %d\n",iSpeech);
						strStripEdText += "{\n";
						strStripEdText += va("   REFERENCE %s\n",String_ToUpper(Speech.SpeechLabel.strLabel));
						strStripEdText += va("   TEXT_LANGUAGE1 \"%s\"\n",Speech.strSpeech);    
						strStripEdText += "}\n";
					}

					CString strOutputFileName(va("%s\\%s.sp",sWORKDIR,(LPCSTR)strStripEdFileNameBase));
					StatusMessage(va("Writing \"%s\"... \n",(LPCSTR) strOutputFileName));

					strStripEdText.Replace("\n","\r\n");	// make it suitable for binary write
					SaveFile((LPCSTR) strOutputFileName, (LPCSTR) strStripEdText, strStripEdText.GetLength() );
				}
			}
		}
		else
		{
			ErrorBox(va("Unable to open \"%s\"!",(LPCSTR)strFileName_LevelNames));
		}
	}
	else
	{
		ErrorBox(va("Unable to open \"%s\"!",(LPCSTR)strFileName_ExportedXLS));
	}

	StatusMessage(NULL);
}



void CMainFrame::OnEditTopmost() 
{
	AppVars.bAlwaysOnTop = !AppVars.bAlwaysOnTop;

	SetWindowPos(AppVars.bAlwaysOnTop?&wndTopMost:&wndNoTopMost,0,0,0,0,SWP_NOSIZE|SWP_NOMOVE);
}

void CMainFrame::OnUpdateEditTopmost(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(AppVars.bAlwaysOnTop);
}

void CMainFrame::OnViewTriindexes() 
{
	AppVars.bTriIndexes = !AppVars.bTriIndexes;
	m_splitter.Invalidate(false);
}

// only allow this option if surface highlighting is on...
//
void CMainFrame::OnUpdateViewTriindexes(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable	( AppVars.bSurfaceHighlight );	
	pCmdUI->SetCheck( AppVars.bTriIndexes );
}

#define sJK2BASEDIR "w:\\game\\base\\"
void CMainFrame::OnFileViewJk2Bots() 
{
	if (!gamedir[0])
	{
		LPCSTR psBasePathToCopy = sJK2BASEDIR;
		if (!GetYesNo("Warning: base path not known yet, shall I assume ' " sJK2BASEDIR " ' ?"))
		{
			psBasePathToCopy = GetString("Enter suitable base path\n\n( format example: \"" sJK2BASEDIR "\" )");
			if (!psBasePathToCopy)
				return;
		}

		strcpy(gamedir,psBasePathToCopy);
	}

	CString strScript;
	CSOF2NPCViewer Viewer(false, &strScript, gamedir);	

	Model_StopAnim();	// or the screen update stops the GDI stuff from showing up

	if (Viewer.DoModal() == IDOK)
	{
		if (Gallery_Active())
		{
// pick this up in the timer loop now...
//
//			CString strCaption;
//			while (GalleryRead_ExtractEntry(strCaption, strScript))
//			{
//				OutputDebugString(va("\"%s\" (script len %d)\n",(LPCSTR)strCaption,strScript.GetLength()));
//			}
			extern CString strGalleryErrors;
			extern CString strGalleryWarnings;
			extern CString strGalleryInfo;

			strGalleryErrors = strGalleryWarnings = strGalleryInfo = "";
			Model_StopAnim();
			gbTextInhibit = AppVars.bCleanScreenShots;	//true;
			return;
		}
		else
		{
			// normal double-click on a single template list entry...
			//
			if (!strScript.IsEmpty())
			{
				strScript += "\n\n\n";
				//SendStringToNotepad(strScript,"temp.txt");

				string strOutputFileName( va("%s\\%s",scGetTempPath(),"temp.mvs") );
				
				int iReturn = SaveFile(strOutputFileName.c_str(),(LPCSTR)strScript, strScript.GetLength());
				if (iReturn != -1)
				{
					extern bool Document_ModelLoadPrimary(LPCSTR psFilename);
					Document_ModelLoadPrimary(strOutputFileName.c_str());
				}
			}
		}
	}
}

void CMainFrame::OnAnimationEndframe() 
{
	ModelList_GoToEndFrame();
}

void CMainFrame::OnAnimation15FPS()
{
	AppVars.dAnimSpeed = 0.0666666666666667; //15 fps
}

void CMainFrame::OnAnimation20FPS()
{
	AppVars.dAnimSpeed = 0.05; //20 fps
}

void CMainFrame::OnAnimation40FPS()
{
	AppVars.dAnimSpeed = 0.025; //40 fps
}

#define sJK3BASEDIR "c:\\ja\\base\\"
void CMainFrame::OnFileBatchconvert()
{
	LPCSTR psBasePathToCopy = sJK3BASEDIR;
	if (!GetYesNo("Warning: base path not known yet, shall I assume ' " sJK3BASEDIR " ' ?"))
	{
		psBasePathToCopy = GetString("Enter suitable base path\n\n( format example: \"" sJK3BASEDIR "\" )");
		if (!psBasePathToCopy)
			return;
	}

	// Scan for list of available .GLM files
	char **ppsGLMFiles;
	int iGLMFiles;

	ppsGLMFiles =	//ri.FS_ListFiles( "shaders", ".shader", &iSkinFiles );
		Sys_ListFiles(	psBasePathToCopy,		// const char *directory, 
		"/",		// const char *extension, 
		"*.glm",		// char *filter, 
		&iGLMFiles,	// int *numfiles, 
		qfalse		// qboolean wantsubs 
		);

	if(!iGLMFiles)
	{
		WarningBox(va("WARNING: no GLM files found in '%s'\n",psBasePathToCopy ));
		return;
	}

	for ( int i=0; i<iGLMFiles; i++ )
	{
		char sFileName[128];

		string strThisGLMFile(ppsGLMFiles[i]);
		
		Com_sprintf( sFileName, sizeof(sFileName), "%s%s", psBasePathToCopy, strThisGLMFile.c_str() );
		StatusMessage( va("Scanning GLM file %d/%d: \"%s\"...",i+1,iGLMFiles,sFileName));

		// Load in this GLM file
		GetActiveDocument()->OnOpenDocument(sFileName);

		// Save compressed GLM file
		StatusMessage( va("Saving compressed GLM file %d/%d: \"%s\"...",i+1,iGLMFiles,sFileName));
		Model_Save(sFileName);
	}

	Sys_FreeFileList( ppsGLMFiles );
}