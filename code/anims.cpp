// Filename:-	anims.cpp
//
//	Module to read basic animation-description files...
//
// Currently reads either "<name>.frames" and/or "animtion.cfg" files...
//
#include "stdafx.h"
#include "includes.h"
//
#include "sequence.h"
//
#include "anims.h"



bool Anims_ReadFile_FRAMES(ModelContainer_t *pContainer, LPCSTR psLocalFilename_GLA)
{
	LPCSTR psFilename = va("%s%s.frames",gamedir,Filename_WithoutExt(psLocalFilename_GLA));

	FILE *fHandle = fopen(psFilename,"rt");

	if (fHandle)
	{
		// file format is like this per XSI...
		//
		//	models/test/m4/m44keith.xsi
		//	{
		//	startframe	"0"
		//	duration	"2"
		//	}
		//
		// so...

		Sequence_t Sequence;

		bool bStartFrameRead = false;
		bool bDurationRead	 = false;
		bool bFPSRead		 = false;

		char sLine[1024];

		while (fgets(sLine,sizeof(sLine)-1,fHandle)!=NULL)
		{
			if (bStartFrameRead && bDurationRead && bFPSRead)
			{
				pContainer->SequenceList.push_back(Sequence);
				bStartFrameRead	 = false;
				bDurationRead	 = false;
				bFPSRead		 = false;
			}

			sLine[sizeof(sLine)-1]='\0';
			_strlwr(sLine);

			// :-)
			CString str(sLine);
					str.TrimLeft();
					str.TrimRight();
					str.Replace("\"","");
				strcpy(sLine,str);

			if (strstr(sLine,".xsi"))
			{
				Sequence_Clear(&Sequence);
				strcpy(Sequence.sName,Filename_WithoutPath(Filename_WithoutExt(sLine)));

				// these can be really long...
				strncpy(Sequence.sNameWithPath,Filename_WithoutExt(sLine),sizeof(Sequence.sNameWithPath));
				        Sequence.sNameWithPath[sizeof(Sequence.sNameWithPath)-1]='\0';
			}
			else
			if (_strnicmp(sLine,"startframe",strlen("startframe"))==0)
			{
				CString str(&sLine[strlen("startframe")]);
						str.Replace("\"","");
				Sequence.iStartFrame = atoi(str);

				bStartFrameRead = true;
			}
			else
			if (_strnicmp(sLine,"duration",strlen("duration"))==0)
			{
				CString str(&sLine[strlen("duration")]);
						str.Replace("\"","");
				Sequence.iFrameCount = atoi(str);				

				bDurationRead = true;
			}
			else
			if (_strnicmp(sLine,"fps",strlen("fps"))==0)
			{
				CString str(&sLine[strlen("fps")]);
						str.Replace("\"","");
				Sequence.iFPS = atoi(str);				

				bFPSRead = true;
			}
		}
		fclose(fHandle);
	}
	// DT EDIT
	/*
	else
	{
		ErrorBox( va("Couldn't open file: %s\n", psFilename));
		return false;
	}
	*/
	return !!(pContainer->SequenceList.size());
}




// Code pasted from MD3View and hacked about a bit..............
//
// this has now been re-written to only add to pulldown menus when all menus have been scanned, this way
//	I can strcat frame info to the seq names while keeping a smooth tabbing line...
//
// Note that this function can automatically read either ID format or Raven format files transparently...
//
/*
typedef struct
{
	string	sName;
	int		iStartFrame;
	int		iFrameCount;
	int		iLoopFrame;
	int		iFrameSpeed;
	bool	bMultiSeq;
} Sequence_t;
*/
/*
typedef struct
{
	char	sName[MAX_QPATH];	// eg "run1"
	int		iStartFrame;
	int		iFrameCount;
	int		iLoopFrame;			// -1 for no wrap, else framenum to add to iStartFrame
//	int		iFrameSpeed;
//	bool	bMultiSeq;
	bool	bIsDefault;			// only true if no anim/enum file found
} Sequence_t;
*/

static bool LoadAnimationCFG(LPCSTR psFullPath, ModelContainer_t *pContainer, FILE *handle)//, HDC hDC)	// hDC for text metrics
{
//	int  iLongestTextMetric = 0;
//	SIZE Size;
	bool bOk = false;	

//	FILE *handle = fopen(psFullPath,"rt");

	if (handle)
	{
		static char sLineBuffer[2048];

		int iFirstFrameAfterBoth = -1;	// stuff I need to do for ID's non-folded frame numbers
		int iFirstFrameAfterTorso= -1;

		while (1)
		{
			ZEROMEM(sLineBuffer);
			if (!fgets(sLineBuffer,sizeof(sLineBuffer),handle))
			{
				if (ferror(handle))
				{
					ErrorBox(va("Error while reading \"%s\"!",psFullPath));
//					ClearAnimationCFG();
				}
				break;	// whether error or EOF
			}

			char sComment[2048] = {0};	// keep comments now because of the way ID cfg files work

			// zap any comments...
			//
			char *p = strstr(sLineBuffer,"//");
			if (p)
			{
				strcpy(sComment,p+2);
				*p=0;
			}

			// update, to read ID cfg files, we need to skip over some stuff that Raven ones don't have...
			//
			// our cfg files don't have "sex" (how depressingly apt...)
			//
			if (_strnicmp(sLineBuffer,"sex",3)==0)
				continue;
			//
			// or this other crap either...
			//
			if (_strnicmp(sLineBuffer,"footsteps",9)==0)
				continue;
			if (_strnicmp(sLineBuffer,"headoffset",10)==0)
				continue;
			if (_strnicmp(sLineBuffer,"soundpath",9)==0)
				continue;

			Sequence_t seq;
			memset(&seq,0,sizeof(seq));

			char sLine[2048];
			int iElementsDone = sscanf( sLineBuffer, "%s %d %d %d %d", &sLine, &seq.iStartFrame, &seq.iFrameCount, &seq.iLoopFrame, &seq.iFPS );
			if (iElementsDone == EOF)
				continue;	// probably skipping over a comment line

			bool bElementsScannedOk = false;

			if (iElementsDone == 5)
			{
				// then it must be a Raven line...
				//
				bElementsScannedOk = true;
//				mdview.bAnimIsMultiPlayerFormat = false;
			}
			else
			{
				// try scanning it as an ID line...
				//
				iElementsDone = sscanf( sLineBuffer, "%d %d %d %d", &seq.iStartFrame, &seq.iFrameCount, &seq.iLoopFrame, &seq.iFPS );
				if (iElementsDone == 4)
				{
//					mdview.bAnimIsMultiPlayerFormat = true;
					// scanned an ID line in ok, now convert it to Raven format...
					//
					iElementsDone = sscanf( sComment, "%s", &sLine );	// grab anim name from original saved comment
					if (iElementsDone == 1)
					{
						// ... and convert their loop format to ours...
						//
						if (seq.iLoopFrame == 0)
						{
							seq.iLoopFrame = -1;
						}
						else
						{
							seq.iLoopFrame = seq.iFrameCount - seq.iLoopFrame;
						}


						// now do the folding number stuff since ID don't do it in their files...
						//
						if ( !_strnicmp(sLine,"TORSO_",6) && iFirstFrameAfterBoth == -1)
						{
							iFirstFrameAfterBoth = seq.iStartFrame;
						}
						if ( !_strnicmp(sLine,"LEGS_",5))
						{
							if (iFirstFrameAfterTorso == -1)
							{
								iFirstFrameAfterTorso = seq.iStartFrame;
							}

							// now correct the leg framenumber...
							//
							if (iFirstFrameAfterBoth != -1)	// if it did, then there'd be no torso frames, so no adj nec.
							{
								seq.iStartFrame -= (iFirstFrameAfterTorso - iFirstFrameAfterBoth);
							}
						}

						bElementsScannedOk = true;
					}
				}
			}

			if (bElementsScannedOk)
			{	
				strcpy(seq.sName,sLine);//seq.sName = sLine;

				pContainer->SequenceList.push_back(seq);

				//
				// this line seems to be ok...
				//
//				OutputDebugString(va("%s  %d %d %d %d\n",seq.sName.c_str(), seq.iStartFrame, seq.iFrameCount, seq.iLoopFrame, seq.iFrameSpeed ));


				// "both" or "torso" get added to 'upper' menu...
				//
/*				if ( (!strnicmp(seq.sName.c_str(),"BOTH_",5)) || (!strnicmp(seq.sName.c_str(),"TORSO_",6)) )
				{
					Sequences_UpperAnims.push_back(seq);
					if (iAnimLockLongestString < strlen(seq.sName.c_str()))
						iAnimLockLongestString = strlen(seq.sName.c_str());

					if (GetTextExtentPoint(	hDC,						// HDC hdc,           // handle to device context
											seq.sName.c_str(),			// LPCTSTR lpString,  // pointer to text string
											strlen(seq.sName.c_str()),	// int cbString,      // number of characters in string
											&Size						// LPSIZE lpSize      // pointer to structure for string size
											)
						)
					{
						if (iLongestTextMetric < Size.cx)
							iLongestTextMetric = Size.cx;
					}

//					Menu_UpperAnims_AddItem(seq.sName.c_str());					
				}

				// "both" or "legs" get added to 'lower' menu...
				//
				if ( (!strnicmp(seq.sName.c_str(),"BOTH_",5)) || (!strnicmp(seq.sName.c_str(),"LEGS_",5)) )
				{
					Sequences_LowerAnims.push_back(seq);
					if (iAnimLockLongestString < strlen(seq.sName.c_str()))
						iAnimLockLongestString = strlen(seq.sName.c_str());

					if (GetTextExtentPoint(	hDC,						// HDC hdc,           // handle to device context
											seq.sName.c_str(),			// LPCTSTR lpString,  // pointer to text string
											strlen(seq.sName.c_str()),	// int cbString,      // number of characters in string
											&Size						// LPSIZE lpSize      // pointer to structure for string size
											)
						)
					{
						if (iLongestTextMetric < Size.cx)
							iLongestTextMetric = Size.cx;
					}
 
//					Menu_LowerAnims_AddItem(seq.sName.c_str());
				}
*/
			}
			else
			{
				// so do we report this as an error or what?
				//
				ErrorBox(sLineBuffer);
			}
		}

		fclose(handle);
/*
		// now add to menus... (this code is awful, it was simple at first then mutated with feature-add)
		//
		char sLine[2048];
		vector< Sequence_t >::iterator it;
		for (it=Sequences_UpperAnims.begin(); it!=Sequences_UpperAnims.end(); it++)
		{
			sprintf(sLine,(*it).sName.c_str());

			while (1)
			{
				GetTextExtentPoint(	hDC,			// HDC hdc,           // handle to device context
									sLine,			// LPCTSTR lpString,  // pointer to text string
									strlen(sLine),	// int cbString,      // number of characters in string
									&Size			// LPSIZE lpSize      // pointer to structure for string size
									);
				if (Size.cx >= iLongestTextMetric)
					break;

				strcat(sLine," ");
			}

			Menu_UpperAnims_AddItem(va("%s (%d...%d)%s",sLine,(*it).iStartFrame,((*it).iStartFrame+(*it).iFrameCount)-1,((*it).iLoopFrame==-1)?"":va("  Loop %d",(*it).iStartFrame+(*it).iLoopFrame)));
		}

		for (it=Sequences_LowerAnims.begin(); it!=Sequences_LowerAnims.end(); it++)
		{
			sprintf(sLine,(*it).sName.c_str());

			while (1)
			{
				GetTextExtentPoint(	hDC,			// HDC hdc,           // handle to device context
									sLine,			// LPCTSTR lpString,  // pointer to text string
									strlen(sLine),	// int cbString,      // number of characters in string
									&Size			// LPSIZE lpSize      // pointer to structure for string size
									);
				if (Size.cx >= iLongestTextMetric)
					break;

				strcat(sLine," ");
			}

			Menu_LowerAnims_AddItem(va("%s (%d...%d)%s",sLine,(*it).iStartFrame,((*it).iStartFrame+(*it).iFrameCount)-1,((*it).iLoopFrame==-1)?"":va("  Loop %d",(*it).iStartFrame+(*it).iLoopFrame)));
		}
  */

		/*
		// a bit of sanity checking, to cope with something Bob tried to do...   :-)
		//
		Sequence_t* pSeq = NULL;
		gl_model* pModel;		
		
		if ((pModel = pModel_Lower)!=0)
		{
			pSeq = Animation_GetLowerSequence(Animation_GetNumLowerSequences()-1);
			ReportFrameMismatches(pSeq,pModel);
		}

		if ((pModel = pModel_Upper)!=0)
		{
			pSeq = Animation_GetUpperSequence(Animation_GetNumUpperSequences()-1);
			ReportFrameMismatches(pSeq,pModel);
		}
		*/
	}

	//return bOk;
	return !!(pContainer->SequenceList.size());
}

bool Anims_ReadFile_ANIMATION_CFG(ModelContainer_t *pContainer, LPCSTR psLocalFilename_GLA)
{
	LPCSTR psFilename = va("%s%s/animation.cfg",gamedir,Filename_PathOnly(psLocalFilename_GLA));

	FILE *fHandle = fopen(psFilename,"rt");

	if (fHandle)
	{
		if (LoadAnimationCFG(psFilename, pContainer, fHandle))
		{
			//
		}
		fclose(fHandle);
	}
	// DT EDIT
	/*
	else
	{
		ErrorBox( va("Couldn't open file: %s\n", psFilename));
		return false;
	}
	*/

	return !!(pContainer->SequenceList.size());
}


// Look up a sequence by animID string (case-insensitive). Returns NULL if
// not in the container's SequenceList. Used to resolve animevents.cfg lines
// that reference an animation by name (e.g. BOTH_ATTACK1).
static const Sequence_t *FindSequenceByName(const ModelContainer_t *pContainer, const char *sName)
{
	for (size_t i = 0; i < pContainer->SequenceList.size(); i++)
	{
		if (_stricmp(pContainer->SequenceList[i].sName, sName) == 0)
			return &pContainer->SequenceList[i];
	}
	return NULL;
}

// animevents.cfg lives next to animation.cfg in the .gla's folder. Format is
// loosely:
//    UPPEREVENTS { animID AEV_EFFECT frame effectPath boltName chance ... }
//    LOWEREVENTS { ... }
// We only care about AEV_EFFECT rows; other event types (AEV_SOUNDCHAN, AEV_FIRE,
// AEV_MOVE, AEV_FOOTSTEP, etc.) are skipped. UPPEREVENTS/LOWEREVENTS blocks are
// treated identically for ModView (no upper/lower distinction at playback).
bool Anims_ReadFile_ANIMEVENTS_CFG(ModelContainer_t *pContainer, LPCSTR psLocalFilename_GLA)
{
	LPCSTR psFilename = va("%s%s/animevents.cfg",gamedir,Filename_PathOnly(psLocalFilename_GLA));

	FILE *fHandle = fopen(psFilename,"rt");
	if (!fHandle) return false;

	char sLine[2048];
	int  iTotalEvents = 0;

	while (fgets(sLine, sizeof(sLine), fHandle))
	{
		// strip comments
		char *pc = strstr(sLine, "//");
		if (pc) *pc = 0;

		// trim leading/trailing whitespace and braces
		CString str(sLine);
		str.TrimLeft();
		str.TrimRight();
		if (str.IsEmpty()) continue;

		// skip section headers and braces
		if (_strnicmp(str, "UPPEREVENTS", 11) == 0) continue;
		if (_strnicmp(str, "LOWEREVENTS", 11) == 0) continue;
		if (str[0] == '{' || str[0] == '}') continue;

		// Tokenise: animID, eventType, frame, <args...>
		char sAnimID[MAX_QPATH]   = {0};
		char sEventType[64]       = {0};
		int  iRelFrame            = 0;
		// Use a fixed-field sscanf first to grab the common prefix
		int iScanned = sscanf(str, "%63s %63s %d", sAnimID, sEventType, &iRelFrame);
		if (iScanned < 3) continue;
		if (_stricmp(sEventType, "AEV_EFFECT") != 0) continue;	// only handling effects

		// After the frame, skip those three tokens in the line and grab
		// effectPath + boltName + chance from the remainder.
		const char *rest = str;
		for (int skip = 0; skip < 3; skip++)
		{
			while (*rest && !isspace((unsigned char)*rest)) rest++;
			while (*rest && isspace((unsigned char)*rest)) rest++;
		}
		char sEffectPath[128] = {0};
		char sBoltName[64]    = {0};
		int  iChance          = 0;
		int iRestScanned = sscanf(rest, "%127s %63s %d", sEffectPath, sBoltName, &iChance);
		if (iRestScanned < 2) continue;
		if (iRestScanned < 3) iChance = 0;	// default: always

		// Resolve animID to a concrete frame range so dispatch is a lookup.
		const Sequence_t *pSeq = FindSequenceByName(pContainer, sAnimID);
		if (!pSeq) continue;	// unknown animation, skip silently (game data can reference anims we don't have)

		int iAbsFrame = pSeq->iStartFrame + iRelFrame;

		AnimEvent_t ev = {0};
		strncpy(ev.sEffectPath, sEffectPath, sizeof(ev.sEffectPath) - 1);
		strncpy(ev.sBoltName,   sBoltName,   sizeof(ev.sBoltName)   - 1);
		ev.iChance = iChance;

		pContainer->AnimEventsByFrame[iAbsFrame].push_back(ev);
		iTotalEvents++;
	}

	fclose(fHandle);
	return iTotalEvents > 0;
}
