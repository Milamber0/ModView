// Filename:-	scene_state.cpp
//
// Save/restore scene state (camera, animation, skins, bolts, sabers) to JSON

#include "stdafx.h"
#include "includes.h"
#include "R_Common.h"
#include "R_Model.h"
#include "R_Surface.h"
#include "textures.h"
#include "shader.h"
#include "skins.h"
#include <stdio.h>


// =============================================================================
// Get the path to modview_scenes.json next to the exe
// =============================================================================

static CString GetSceneFilePath()
{
	char exePath[MAX_PATH] = {0};
	GetModuleFileName(NULL, exePath, MAX_PATH);
	// Strip exe filename, keep directory
	char *lastSlash = strrchr(exePath, '\\');
	if (!lastSlash) lastSlash = strrchr(exePath, '/');
	if (lastSlash) *(lastSlash+1) = 0;
	return CString(exePath) + "modview_scenes.json";
}


// =============================================================================
// Escape a string for JSON output
// =============================================================================

static CString JsonEscape(LPCSTR s)
{
	CString out;
	for (; *s; s++) {
		if (*s == '\\') out += "\\\\";
		else if (*s == '"') out += "\\\"";
		else if (*s == '\n') out += "\\n";
		else if (*s == '\r') out += "\\r";
		else if (*s == '\t') out += "\\t";
		else out += *s;
	}
	return out;
}


// =============================================================================
// Save scene state for the current model
// =============================================================================

// Defaults for comparison (only write non-default values)
#define DEF_XPOS		0.0f
#define DEF_YPOS		0.0f
#define DEF_ZPOS		-30.0f
#define DEF_ROTX		15.5f
#define DEF_ROTY		44.0f
#define DEF_ROTZ		-90.0f
#define DEF_FOV			10.0f


void SceneState_Save()
{
	if (!Model_Loaded()) return;

	LPCSTR psModelPath = Model_GetFullPrimaryFilename();
	if (!psModelPath || !psModelPath[0]) return;

	CString strSceneFile = GetSceneFilePath();
	CString strModelKey = JsonEscape(psModelPath);

	// Read existing file content (we'll merge our entry)
	CMapStringToString existingEntries;
	{
		FILE *f = fopen(strSceneFile, "rt");
		if (f) {
			// Simple parser: find "key": { ... } blocks
			char line[4096];
			CString currentKey;
			CString currentBlock;
			int braceDepth = 0;
			bool inEntry = false;

			while (fgets(line, sizeof(line), f)) {
				CString sLine(line);
				sLine.TrimLeft(); sLine.TrimRight();

				if (!inEntry) {
					// Look for "path": {
					int quote1 = sLine.Find('"');
					int quote2 = sLine.Find('"', quote1+1);
					if (quote1 >= 0 && quote2 > quote1 && sLine.Find('{', quote2) >= 0) {
						currentKey = sLine.Mid(quote1+1, quote2-quote1-1);
						currentBlock = "{\n";
						braceDepth = 1;
						inEntry = true;
					}
				} else {
					currentBlock += CString(line);
					for (int c = 0; c < sLine.GetLength(); c++) {
						if (sLine[c] == '{') braceDepth++;
						else if (sLine[c] == '}') braceDepth--;
					}
					if (braceDepth <= 0) {
						existingEntries[currentKey] = currentBlock;
						inEntry = false;
					}
				}
			}
			fclose(f);
		}
	}

	// Build our scene entry - only write non-default values
	CString entry = "{\n";
	bool hasContent = false;

	#define WRITE_FLOAT(name, val, def) \
		if (fabsf((val) - (def)) > 0.01f) { \
			if (hasContent) entry += ",\n"; \
			entry.AppendFormat("      \"%s\": %.2f", name, (val)); \
			hasContent = true; \
		}

	#define WRITE_INT(name, val, def) \
		if ((val) != (def)) { \
			if (hasContent) entry += ",\n"; \
			entry.AppendFormat("      \"%s\": %d", name, (val)); \
			hasContent = true; \
		}

	#define WRITE_BOOL(name, val, def) \
		if ((val) != (def)) { \
			if (hasContent) entry += ",\n"; \
			entry.AppendFormat("      \"%s\": %s", name, (val) ? "true" : "false"); \
			hasContent = true; \
		}

	#define WRITE_STRING(name, val) \
		if ((val) && strlen(val) > 0) { \
			if (hasContent) entry += ",\n"; \
			entry.AppendFormat("      \"%s\": \"%s\"", name, (LPCSTR)JsonEscape(val)); \
			hasContent = true; \
		}

	// Camera
	WRITE_FLOAT("xPos", AppVars.xPos, DEF_XPOS);
	WRITE_FLOAT("yPos", AppVars.yPos, DEF_YPOS);
	WRITE_FLOAT("zPos", AppVars.zPos, DEF_ZPOS);
	WRITE_FLOAT("rotX", AppVars.rotAngleX, DEF_ROTX);
	WRITE_FLOAT("rotY", AppVars.rotAngleY, DEF_ROTY);
	WRITE_FLOAT("rotZ", AppVars.rotAngleZ, DEF_ROTZ);
	WRITE_FLOAT("fov", AppVars.dFOV, DEF_FOV);

	// Animation - save sequence name if locked
	ModelHandle_t hModel = Model_GetPrimaryHandle();
	ModelContainer_t *pContainer = ModelContainer_FindFromModelHandle(hModel);
	if (pContainer && pContainer->iSequenceLockNumber_Primary >= 0) {
		LPCSTR psSeqName = Model_Sequence_GetName(hModel, pContainer->iSequenceLockNumber_Primary);
		WRITE_STRING("sequence", psSeqName);
	}

	// Skin - base skin + part overrides
	if (pContainer && !pContainer->strCurrentSkinFile.empty()) {
		WRITE_STRING("skin", pContainer->strCurrentSkinFile.c_str());
	}
	if (pContainer && !pContainer->strCurrentPartSkin_Head.empty()) {
		WRITE_STRING("skinHead", pContainer->strCurrentPartSkin_Head.c_str());
	}
	if (pContainer && !pContainer->strCurrentPartSkin_Torso.empty()) {
		WRITE_STRING("skinTorso", pContainer->strCurrentPartSkin_Torso.c_str());
	}
	if (pContainer && !pContainer->strCurrentPartSkin_Lower.empty()) {
		WRITE_STRING("skinLower", pContainer->strCurrentPartSkin_Lower.c_str());
	}

	// Entity color
	WRITE_INT("entityR", AppVars.entityRGBA[0], 255);
	WRITE_INT("entityG", AppVars.entityRGBA[1], 255);
	WRITE_INT("entityB", AppVars.entityRGBA[2], 255);

	// Saber blades
	WRITE_BOOL("saberR", AppVars.bSaberBlade[0], false);
	WRITE_BOOL("saberL", AppVars.bSaberBlade[1], false);
	WRITE_INT("saberRColor", AppVars.saberColorIndex[0], 0);
	WRITE_INT("saberLColor", AppVars.saberColorIndex[1], 1);
	if (AppVars.saberColorIndex[0] == 6) {
		WRITE_INT("saberRCustR", AppVars.saberCustomColor[0][0], 255);
		WRITE_INT("saberRCustG", AppVars.saberCustomColor[0][1], 255);
		WRITE_INT("saberRCustB", AppVars.saberCustomColor[0][2], 255);
	}
	if (AppVars.saberColorIndex[1] == 6) {
		WRITE_INT("saberLCustR", AppVars.saberCustomColor[1][0], 255);
		WRITE_INT("saberLCustG", AppVars.saberCustomColor[1][1], 255);
		WRITE_INT("saberLCustB", AppVars.saberCustomColor[1][2], 255);
	}

	// Bolted models - save each surface bolt that has something attached
	if (pContainer) {
		int boltIdx = 0;
		for (int b = 0; b < pContainer->iSurfaceBolt_MaxBoltPoints; b++) {
			BoltPoint_t &bp = pContainer->tSurfaceBolt_BoltPoints[b];
			for (int m = 0; m < (int)bp.vBoltedContainers.size(); m++) {
				ModelContainer_t &bolted = bp.vBoltedContainers[m];
				if (bolted.hModel) {
					LPCSTR psBoltName = Model_GetBoltName(hModel, b, false);
					// Reconstruct full path from gamedir + local path
					CString fullPath;
					fullPath.Format("%s%s", gamedir, bolted.sLocalPathName);
					if (hasContent) entry += ",\n";
					entry.AppendFormat("      \"bolt%d_model\": \"%s\"", boltIdx, (LPCSTR)JsonEscape(fullPath));
					hasContent = true;
					entry += ",\n";
					entry.AppendFormat("      \"bolt%d_point\": \"%s\"", boltIdx, (LPCSTR)JsonEscape(psBoltName));
					boltIdx++;
				}
			}
		}
	}

	#undef WRITE_FLOAT
	#undef WRITE_INT
	#undef WRITE_BOOL
	#undef WRITE_STRING

	entry += "\n    }";

	// Update our entry in the map
	existingEntries[strModelKey] = entry;

	// Write the full JSON file
	FILE *f = fopen(strSceneFile, "wt");
	if (f) {
		fprintf(f, "{\n");
		POSITION pos = existingEntries.GetStartPosition();
		bool first = true;
		while (pos) {
			CString key, val;
			existingEntries.GetNextAssoc(pos, key, val);
			if (!first) fprintf(f, ",\n");
			fprintf(f, "    \"%s\": %s", (LPCSTR)key, (LPCSTR)val);
			first = false;
		}
		fprintf(f, "\n}\n");
		fclose(f);
	}
}


// =============================================================================
// Restore scene state for a model path
// =============================================================================

// Simple JSON value extraction helpers
static CString JsonGetString(const CString &block, LPCSTR key) {
	CString search; search.Format("\"%s\":", key);
	int pos = block.Find(search);
	if (pos < 0) return "";
	pos += search.GetLength();
	int q1 = block.Find('"', pos);
	if (q1 < 0) return "";
	int q2 = block.Find('"', q1+1);
	if (q2 < 0) return "";
	return block.Mid(q1+1, q2-q1-1);
}

static float JsonGetFloat(const CString &block, LPCSTR key, float def) {
	CString search; search.Format("\"%s\":", key);
	int pos = block.Find(search);
	if (pos < 0) return def;
	pos += search.GetLength();
	while (pos < block.GetLength() && block[pos] == ' ') pos++;
	return (float)atof(block.Mid(pos));
}

static int JsonGetInt(const CString &block, LPCSTR key, int def) {
	CString search; search.Format("\"%s\":", key);
	int pos = block.Find(search);
	if (pos < 0) return def;
	pos += search.GetLength();
	while (pos < block.GetLength() && block[pos] == ' ') pos++;
	CString val = block.Mid(pos);
	if (val.Left(4) == "true") return 1;
	if (val.Left(5) == "false") return 0;
	return atoi(val);
}


void SceneState_Restore(LPCSTR psModelPath)
{
	if (!psModelPath || !psModelPath[0]) return;

	CString strSceneFile = GetSceneFilePath();
	CString strModelKey = JsonEscape(psModelPath);

	// Read the file and find our model's entry
	FILE *f = fopen(strSceneFile, "rt");
	if (!f) return;

	CString block;
	{
		char line[4096];
		CString currentKey;
		int braceDepth = 0;
		bool inEntry = false;
		bool found = false;

		while (fgets(line, sizeof(line), f)) {
			CString sLine(line);
			sLine.TrimLeft(); sLine.TrimRight();

			if (!inEntry) {
				int quote1 = sLine.Find('"');
				int quote2 = sLine.Find('"', quote1+1);
				if (quote1 >= 0 && quote2 > quote1) {
					currentKey = sLine.Mid(quote1+1, quote2-quote1-1);
					if (sLine.Find('{', quote2) >= 0) {
						braceDepth = 1;
						inEntry = true;
						if (currentKey == strModelKey) {
							block = CString(line);
							found = true;
						}
					}
				}
			} else {
				if (found) block += CString(line);
				for (int c = 0; c < sLine.GetLength(); c++) {
					if (sLine[c] == '{') braceDepth++;
					else if (sLine[c] == '}') braceDepth--;
				}
				if (braceDepth <= 0) {
					if (found) break;
					inEntry = false;
				}
			}
		}
		fclose(f);
		if (!found) return;
	}

	// Restore camera
	AppVars.xPos = JsonGetFloat(block, "xPos", DEF_XPOS);
	AppVars.yPos = JsonGetFloat(block, "yPos", DEF_YPOS);
	AppVars.zPos = JsonGetFloat(block, "zPos", DEF_ZPOS);
	AppVars.rotAngleX = JsonGetFloat(block, "rotX", DEF_ROTX);
	AppVars.rotAngleY = JsonGetFloat(block, "rotY", DEF_ROTY);
	AppVars.rotAngleZ = JsonGetFloat(block, "rotZ", DEF_ROTZ);
	AppVars.dFOV = JsonGetFloat(block, "fov", DEF_FOV);

	// Restore entity color
	AppVars.entityRGBA[0] = (byte)JsonGetInt(block, "entityR", 255);
	AppVars.entityRGBA[1] = (byte)JsonGetInt(block, "entityG", 255);
	AppVars.entityRGBA[2] = (byte)JsonGetInt(block, "entityB", 255);

	// Restore saber state
	AppVars.bSaberBlade[0] = JsonGetInt(block, "saberR", 0) != 0;
	AppVars.bSaberBlade[1] = JsonGetInt(block, "saberL", 0) != 0;
	AppVars.saberColorIndex[0] = JsonGetInt(block, "saberRColor", 0);
	AppVars.saberColorIndex[1] = JsonGetInt(block, "saberLColor", 1);
	AppVars.saberCustomColor[0][0] = (byte)JsonGetInt(block, "saberRCustR", 255);
	AppVars.saberCustomColor[0][1] = (byte)JsonGetInt(block, "saberRCustG", 255);
	AppVars.saberCustomColor[0][2] = (byte)JsonGetInt(block, "saberRCustB", 255);
	AppVars.saberCustomColor[1][0] = (byte)JsonGetInt(block, "saberLCustR", 255);
	AppVars.saberCustomColor[1][1] = (byte)JsonGetInt(block, "saberLCustG", 255);
	AppVars.saberCustomColor[1][2] = (byte)JsonGetInt(block, "saberLCustB", 255);

	// Restore skins: base skin first, then part overrides on top
	ModelHandle_t hModel = Model_GetPrimaryHandle();
	CString skin = JsonGetString(block, "skin");
	if (!skin.IsEmpty() && hModel) {
		Model_ApplyOldSkin(hModel, skin);
	}
	// Apply part skins (head_, torso_, lower_) on top of the base
	CString skinHead = JsonGetString(block, "skinHead");
	CString skinTorso = JsonGetString(block, "skinTorso");
	CString skinLower = JsonGetString(block, "skinLower");
	if (!skinHead.IsEmpty() && hModel) Model_ApplyOldSkin(hModel, skinHead);
	if (!skinTorso.IsEmpty() && hModel) Model_ApplyOldSkin(hModel, skinTorso);
	if (!skinLower.IsEmpty() && hModel) Model_ApplyOldSkin(hModel, skinLower);

	// Restore animation sequence
	CString seq = JsonGetString(block, "sequence");
	if (!seq.IsEmpty() && hModel) {
		Model_Sequence_Lock(hModel, (LPCSTR)seq, true, false);
	}

	// Restore bolted models
	if (hModel) {
		for (int b = 0; b < 10; b++) {
			CString modelKey; modelKey.Format("bolt%d_model", b);
			CString pointKey; pointKey.Format("bolt%d_point", b);
			CString boltModel = JsonGetString(block, modelKey);
			CString boltPoint = JsonGetString(block, pointKey);
			if (!boltModel.IsEmpty() && !boltPoint.IsEmpty()) {
				// Unescape backslashes
				boltModel.Replace("\\\\", "\\");
				Model_LoadBoltOn(boltModel, hModel, boltPoint, false, true);
			}
		}
	}
}
