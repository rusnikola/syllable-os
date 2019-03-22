/*
 *  Keyboard Preferences
 *  Copyright (c) 2004 Ruslan Nickolaev (nruslan@hotbox.ru)
 *  Based on works by Daryl Dudey and Kurt Skuaen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <atheos/filesystem.h>
#include <appserver/keymap.h>
#include <util/application.h>
#include <util/invoker.h>
#include <util/resources.h>
#include <gui/window.h>
#include <gui/requesters.h>
#include <gui/frameview.h>
#include <gui/textview.h>
#include <gui/desktop.h>
#include <gui/button.h>
#include <gui/dropdownmenu.h>
#include <gui/checkbox.h>
#include <gui/slider.h>
#include <gui/image.h>
#include <iostream>

#define APP_CONFIG "config/Keyboard.cfg"
#define KEYMAPS_CONFIG "/system/config/keymaps.cfg"
#define DUAL_KEYMAP ".dual-keymap"
#define DEFAULT_KEYMAP "American"
#define KEYBWIND_XDELTA 400.0f
#define KEYBWIND_YDELTA 160.0f
#define KEYBWIND_YDELTA_ROOT 260.0f
#define KEYMAP_PATH 16

static const char *g_zDualKeymap = "/system/keymaps/.dual-keymap";
static const char *g_azError[] = {
	"Primary keymap file is unavailable",
	"Alternate keymap file is unavailable",
	"Error reading primary keymap file",
	"Error reading alternate keymap file",
	"Error writing dual-keymap file",
	"Incorrect version of primary or alternate keymap file",
	"Primary keymap file is not regular or symbolic link",
	"Alternate keymap file is not regular or symbolic link",
	"Disc write error"
};

static char g_zPrimKeymapPath[KEYMAP_PATH + 256] = "/system/keymaps/";
static char g_zAltKeymapPath[KEYMAP_PATH + 256] = "/system/keymaps/";
static char *g_zPrimKeymap, *g_zAltKeymap;
static int g_nDelay, g_nRepeat;
static bool g_nRoot = false, g_nHasAlt = false;

class KeyboardWind : public os::Window
{
	public:
		KeyboardWind(const os::Rect &cFrame);
		const char *GetKeymap(const char *zPrimKeymap, const char *zAltKeymap);
		void ShowError(int nErrorCode);
		void AddKeymaps();
		void Apply();
		virtual void HandleMessage(os::Message *pcMessage);
		virtual bool OkToQuit();
		virtual ~KeyboardWind();
	private:
		enum m_eErrors {
			ID_PRIMARY_NOTFOUND,
			ID_ALTERNATE_NOTFOUND,
			ID_PRIMARY_ERR_READ,
			ID_ALTERNATE_ERR_READ,
			ID_DUALKEYMAP_ERR_WRITE,
			ID_INCORRECT_VERSION,
			ID_PRIM_NOTREG,
			ID_ALT_NOTREG,
			ID_WRITE_ERR
		};
		enum m_eMessages {
			ID_DEFAULT,
			ID_UNDO,
			ID_APPLY,
			ID_USEALT,
			ID_DELAY,
			ID_REPEAT
		};
		os::View *m_pcMainView;
		os::FrameView *m_pcKeymapsView, *m_pcKeyboardSettings, *m_pcTestArea;
		os::TextView *m_pcTestText;
		os::StringView *m_pcPrimText, *m_pcAltText, *m_pcDelayText, *m_pcRepeatText;
		os::DropdownMenu *m_pcPrimMenu, *m_pcAltMenu;
		os::Button *m_pcApply, *m_pcUndo, *m_pcDefault;
		os::CheckBox *m_pcUseAlt;
		os::Slider *m_pcDelaySlider, *m_pcRepeatSlider;
		int m_nPrimPos, m_nAltPos;
		int m_nDefaultKeymap;
};

class KeyboardApp : public os::Application
{
	public:
		KeyboardApp();
		virtual bool OkToQuit();
		virtual ~KeyboardApp();
	private:
		KeyboardWind *m_pcWind;
};

KeyboardWind::KeyboardWind(const os::Rect &cFrame)
	: os::Window(cFrame, "keyboard_prefs", g_nRoot ? "Keyboard" : "Keyboard (view only)", os::WND_NOT_RESIZABLE)
{
	os::Rect cRect(GetBounds());
	os::Rect cTmpRect;
	float vBottom = cRect.bottom - 10.0f;

	m_pcMainView = new os::View(cRect, "keyboard_view", os::CF_FOLLOW_NONE);
	AddChild(m_pcMainView);

	if (g_nRoot)
	{
		vBottom -= 100.0f;

		// add text area
		m_pcTestArea = new os::FrameView(os::Rect(10.0f, cRect.bottom - 90.0f, cRect.right - 10.0f, cRect.bottom - 45.0f), "test_area", "Test area (apply changes first)", os::CF_FOLLOW_NONE);
		m_pcMainView->AddChild(m_pcTestArea);
		cTmpRect = m_pcTestArea->GetBounds();
		m_pcTestText = new os::TextView(os::Rect(10.0f, 15.0f, cTmpRect.right - 10.0f, cTmpRect.bottom - 10.0f), "test_text", "", os::CF_FOLLOW_NONE);
		m_pcTestArea->AddChild(m_pcTestText);

		// add buttons
		m_pcDefault = new os::Button(os::Rect(cRect.right - 70.0f, cRect.bottom - 32.0f, cRect.right - 10.0f, cRect.bottom - 12.0f), "default_button", "_Default", new os::Message(ID_DEFAULT), os::CF_FOLLOW_NONE);
		m_pcMainView->AddChild(m_pcDefault);
		m_pcUndo = new os::Button(os::Rect(cRect.right - 140.0f, cRect.bottom - 32.0f, cRect.right - 80.0f, cRect.bottom - 12.0f), "undo_button", "U_ndo", new os::Message(ID_UNDO), os::CF_FOLLOW_NONE);
		m_pcMainView->AddChild(m_pcUndo);
		m_pcApply = new os::Button(os::Rect(cRect.right - 210.0f, cRect.bottom - 32.0f, cRect.right - 150.0f, cRect.bottom - 12.0f), "apply_button", "_Apply", new os::Message(ID_APPLY), os::CF_FOLLOW_NONE);
		m_pcMainView->AddChild(m_pcApply);
		SetDefaultButton(m_pcApply);
	}

	// add keymap settings area
	m_pcKeymapsView = new os::FrameView(os::Rect(10.0f, 10.0f, 0.5 * cRect.right - 5.0f, vBottom), "keymaps_view", "Keymap settings", os::CF_FOLLOW_NONE);
	m_pcMainView->AddChild(m_pcKeymapsView);

	cTmpRect = m_pcKeymapsView->GetBounds();

	// add textviews
	m_pcPrimText = new os::StringView(os::Rect(10.0f, 40.0f, cTmpRect.right - 115.0f, 60.0f), "primary_text", "Primary:", os::ALIGN_LEFT, os::CF_FOLLOW_NONE);
	m_pcKeymapsView->AddChild(m_pcPrimText);
	m_pcAltText = new os::StringView(os::Rect(10.0f, 70.0f, cTmpRect.right - 115.0f, 90.0f), "alternate_text", "Alternate:", os::ALIGN_LEFT, os::CF_FOLLOW_NONE);
	m_pcKeymapsView->AddChild(m_pcAltText);

	// add dropdown menus
	m_pcPrimMenu = new os::DropdownMenu(os::Rect(cTmpRect.right - 110.0f, 40.0f, cTmpRect.right - 10.0f, 60.0f), "primary_menu", os::CF_FOLLOW_NONE);
	m_pcKeymapsView->AddChild(m_pcPrimMenu);
	m_pcPrimMenu->SetReadOnly();
	m_pcPrimMenu->SetTarget(this);
	m_pcAltMenu = new os::DropdownMenu(os::Rect(cTmpRect.right - 110.0f, 70.0f, cTmpRect.right - 10.0f, 90.0f), "alternate_menu", os::CF_FOLLOW_NONE);
	m_pcKeymapsView->AddChild(m_pcAltMenu);
	m_pcAltMenu->SetReadOnly();
	m_pcAltMenu->SetTarget(this);

	// add checkbox menu
	m_pcUseAlt = new os::CheckBox(os::Rect(10.0f, 105.0f, cTmpRect.right - 10.0f, 125.0f), "use_alt", "Use alternate keymap", new os::Message(ID_USEALT), os::CF_FOLLOW_NONE);
	m_pcKeymapsView->AddChild(m_pcUseAlt);
	m_pcUseAlt->SetValue(g_nHasAlt, true);
	m_pcUseAlt->SetEnable(g_nRoot);

	// add keyboard settings
	m_pcKeyboardSettings = new os::FrameView(os::Rect(0.5 * cRect.right + 5.0f, 10.0f, cRect.right - 10.0f, vBottom), "keyboard_settings", "Keyboard settings", os::CF_FOLLOW_NONE);
	m_pcMainView->AddChild(m_pcKeyboardSettings);

	cTmpRect = m_pcKeyboardSettings->GetBounds();

	// add delay slider with label
	m_pcDelayText = new os::StringView(os::Rect(10.0f, 30.0f, 85.0f, 50.0f), "delay_text", "Initial delay", os::ALIGN_LEFT, os::CF_FOLLOW_NONE);
	m_pcKeyboardSettings->AddChild(m_pcDelayText);
	m_pcDelaySlider = new os::Slider(os::Rect(90.0f, 15.0f, cTmpRect.right - 10.0f, 75.0f), "delay_slider", new os::Message(ID_DELAY), os::Slider::TICKS_BELOW, 11);
	m_pcKeyboardSettings->AddChild(m_pcDelaySlider);
	m_pcDelaySlider->SetStepCount(11);
	m_pcDelaySlider->SetLimitLabels("Short", "Long");
	m_pcDelaySlider->SetMinMax(0, 1000);
	m_pcDelaySlider->SetProgStrFormat("%.f msecs");
	m_pcDelaySlider->SetValue((float)g_nDelay, true);
	m_pcDelaySlider->SetEnable(g_nRoot);

	// add repeat slider with label
	m_pcRepeatText = new os::StringView(os::Rect(10.0f, 90.0f, 85.0f, 110.0f), "repeat_text", "Repeat delay", os::ALIGN_LEFT, os::CF_FOLLOW_NONE);
	m_pcKeyboardSettings->AddChild(m_pcRepeatText);
	m_pcRepeatSlider = new os::Slider(os::Rect(90.0f, 75.0f, cTmpRect.right - 10.0f, 135.0f), "repeat_slider", new os::Message(ID_REPEAT), os::Slider::TICKS_BELOW, 11);
	m_pcKeyboardSettings->AddChild(m_pcRepeatSlider);
	m_pcRepeatSlider->SetStepCount(11);
	m_pcRepeatSlider->SetLimitLabels("Fast", "Slow");
	m_pcRepeatSlider->SetMinMax(0, 200);
	m_pcRepeatSlider->SetProgStrFormat("%.f msecs");
	m_pcRepeatSlider->SetValue((float)g_nRepeat, true);
	m_pcRepeatSlider->SetEnable(g_nRoot);

	m_nPrimPos = m_nAltPos = m_nDefaultKeymap = 0;
	AddKeymaps();
	free(g_zPrimKeymap);
	free(g_zAltKeymap);
	m_pcPrimMenu->SetSelection(m_nPrimPos, true);
	m_pcAltMenu->SetSelection(m_nAltPos, true);

	// set icon
	os::Resources pcFEResources(get_image_id());
	os::BitmapImage *pcBitmapImage = new os::BitmapImage(pcFEResources.GetResourceStream("icon24x24.png"));
	SetIcon(pcBitmapImage->LockBitmap());
	delete pcBitmapImage;
}

void KeyboardWind::AddKeymaps()
{
	DIR *hDir;
	int hFd, i = 0;
	uint32 nMagic;
	dirent *psDirEntry;
	struct stat stbuf;

	if ((hDir = opendir("/system/keymaps")) != NULL)
	{
		while ((psDirEntry = readdir(hDir)) != NULL)
		{
			if (strcmp(psDirEntry->d_name, ".") && strcmp(psDirEntry->d_name, "..") && strcmp(psDirEntry->d_name, DUAL_KEYMAP))
			{
				strcpy(g_zPrimKeymapPath + KEYMAP_PATH, psDirEntry->d_name);
				if (((hFd = open(g_zPrimKeymapPath, O_RDONLY)) >= 0) && (fstat(hFd, &stbuf) >= 0))
				{
					if (S_ISREG(stbuf.st_mode))
					{
						nMagic = 0;
						read(hFd, &nMagic, sizeof(nMagic));
						if (nMagic == KEYMAP_MAGIC)
						{
							m_pcPrimMenu->AppendItem(psDirEntry->d_name);
							m_pcAltMenu->AppendItem(psDirEntry->d_name);
							if (!strcmp(DEFAULT_KEYMAP, psDirEntry->d_name))
								m_nDefaultKeymap = i;
							if (!strcmp(g_zPrimKeymap, psDirEntry->d_name))
								m_nPrimPos = i;
							if (!strcmp(g_zAltKeymap, psDirEntry->d_name))
								m_nAltPos = i;
							i++;
						}
					}
					close(hFd);
				}
			}
		}
		closedir(hDir);
	}
}

void KeyboardWind::ShowError(int nErrorCode)
{
	os::Alert *pcError = new os::Alert("Error", g_azError[nErrorCode], os::Alert::ALERT_WARNING, os::WND_NO_CLOSE_BUT | os::WND_NO_ZOOM_BUT | os::WND_NO_DEPTH_BUT | os::WND_NOT_RESIZABLE, "OK", NULL);
	pcError->CenterInWindow(this);
	pcError->Go(new os::Invoker);
}

const char *KeyboardWind::GetKeymap(const char *zPrimKeymap, const char *zAltKeymap)
{
	int hFdPrim, hFdAlt, hFdDest;
	int nErrorCode = ID_PRIMARY_NOTFOUND;
	bool nWriteOK = false;
	uint32 nNumDeadKeys, i;
	struct keymap_header sPrimHead, sAltHead;
	struct keymap *psPrimKeymap, *psAltKeymap;
	struct stat stbuf;
	deadkey *asDeadKey;

	if (((hFdPrim = open(zPrimKeymap, O_RDONLY)) >= 0) && (fstat(hFdPrim, &stbuf) >= 0))
	{
		if (S_ISREG(stbuf.st_mode))
		{
			if (zAltKeymap)
			{
				if ((hFdDest = open(g_zDualKeymap, O_WRONLY | O_CREAT | O_TRUNC, 0644 & ~umask(0))) >= 0)
				{
					if (read(hFdPrim, &sPrimHead, sizeof(sPrimHead)) == sizeof(sPrimHead))
					{
						psPrimKeymap = (struct keymap *)malloc(sPrimHead.m_nSize);
						if (read(hFdPrim, psPrimKeymap, sPrimHead.m_nSize) == (ssize_t)sPrimHead.m_nSize)
						{
							if (((hFdAlt = open(zAltKeymap, O_RDONLY)) >= 0) && (fstat(hFdAlt, &stbuf) >= 0))
							{
								if (S_ISREG(stbuf.st_mode))
								{
									if (read(hFdAlt, &sAltHead, sizeof(sAltHead)) == sizeof(sAltHead))
									{
										psAltKeymap = (struct keymap *)malloc(sAltHead.m_nSize);
										if (read(hFdAlt, psAltKeymap, sAltHead.m_nSize) == (ssize_t)sAltHead.m_nSize)
										{
											if ((sPrimHead.m_nVersion == CURRENT_KEYMAP_VERSION) && (sPrimHead.m_nVersion == sAltHead.m_nVersion))
											{
												close(hFdPrim);
												close(hFdAlt);
												for (i = 0; i < 128; i++)
												{
													psPrimKeymap->m_anMap[i][CS_CAPSL] = psAltKeymap->m_anMap[i][CM_NORMAL];
													psPrimKeymap->m_anMap[i][CS_SHFT_CAPSL] = psAltKeymap->m_anMap[i][CS_SHFT];
													psPrimKeymap->m_anMap[i][CS_CAPSL_OPT] = psAltKeymap->m_anMap[i][CS_OPT];
													psPrimKeymap->m_anMap[i][CS_SHFT_CAPSL_OPT] = psAltKeymap->m_anMap[i][CS_SHFT_OPT];
												}
												nNumDeadKeys = 0;
												asDeadKey = (deadkey *)malloc((psPrimKeymap->m_nNumDeadKeys + psAltKeymap->m_nNumDeadKeys) * sizeof(deadkey));
												for (i = 0; i < psPrimKeymap->m_nNumDeadKeys; i++)
												{
													if (psPrimKeymap->m_sDeadKey[i].m_nQualifier < CS_CAPSL)
														asDeadKey[nNumDeadKeys++] = psPrimKeymap->m_sDeadKey[i];
												}
												for (i = 0; i < psAltKeymap->m_nNumDeadKeys; i++)
												{
													switch (psAltKeymap->m_sDeadKey[i].m_nQualifier)
													{
														case CM_NORMAL:
														{
															asDeadKey[nNumDeadKeys] = psAltKeymap->m_sDeadKey[i];
															asDeadKey[nNumDeadKeys++].m_nQualifier = CS_CAPSL;
															break;
														}
														case CS_SHFT:
														{
															asDeadKey[nNumDeadKeys] = psAltKeymap->m_sDeadKey[i];
															asDeadKey[nNumDeadKeys++].m_nQualifier = CS_SHFT_CAPSL;
															break;
														}
														case CS_OPT:
														{
															asDeadKey[nNumDeadKeys] = psAltKeymap->m_sDeadKey[i];
															asDeadKey[nNumDeadKeys++].m_nQualifier = CS_CAPSL_OPT;
															break;
														}
														case CS_SHFT_OPT:
														{
															asDeadKey[nNumDeadKeys] = psAltKeymap->m_sDeadKey[i];
															asDeadKey[nNumDeadKeys++].m_nQualifier = CS_SHFT_CAPSL_OPT;
															break;
														}
													}
												}
												sPrimHead.m_nSize = sizeof(keymap) + nNumDeadKeys * sizeof(deadkey);
												psPrimKeymap->m_nNumDeadKeys = nNumDeadKeys;
												nNumDeadKeys *= sizeof(deadkey);

												if ((write(hFdDest, &sPrimHead, sizeof(sPrimHead)) == sizeof(sPrimHead)) && (write(hFdDest, psPrimKeymap, sizeof(keymap)) == sizeof(keymap)) && (write(hFdDest, asDeadKey, nNumDeadKeys) == (ssize_t)nNumDeadKeys))
													nWriteOK = true;

												close(hFdDest);
												free(asDeadKey);
												free(psPrimKeymap);
												free(psAltKeymap);

												if (nWriteOK)
													return g_zDualKeymap;
												else {
													unlink(g_zDualKeymap);
													nErrorCode = ID_WRITE_ERR;
												}
											}
											else
												nErrorCode = ID_INCORRECT_VERSION;
										}
										else
											nErrorCode = ID_ALTERNATE_ERR_READ;
										free(psAltKeymap);
									}
									else
										nErrorCode = ID_ALTERNATE_ERR_READ;
								}
								else
									nErrorCode = ID_ALT_NOTREG;
								close(hFdAlt);
							}
							else
								nErrorCode = ID_ALTERNATE_NOTFOUND;
						}
						else
							nErrorCode = ID_PRIMARY_ERR_READ;
						free(psPrimKeymap);
					}
					else
						nErrorCode = ID_PRIMARY_ERR_READ;
					close(hFdDest);
				}
				else
					nErrorCode = ID_DUALKEYMAP_ERR_WRITE;
			}
			else
			{
				unlink(g_zDualKeymap);
				close(hFdPrim);
				return zPrimKeymap;
			}
		}
		else
			nErrorCode = ID_PRIM_NOTREG;
		close (hFdPrim);
	}
	ShowError(nErrorCode);
	return NULL;
}

void KeyboardWind::HandleMessage(os::Message *pcMessage)
{
	switch (pcMessage->GetCode())
	{
		case ID_APPLY:
			Apply();
			break;

		case ID_DEFAULT:
		{
			if (g_nRoot)
			{
				m_pcPrimMenu->SetSelection(m_nDefaultKeymap, true);
				m_pcAltMenu->SetSelection(m_nDefaultKeymap, true);
				m_pcUseAlt->SetValue(0, true);
				m_pcDelaySlider->SetValue(300.0f);
				m_pcRepeatSlider->SetValue(40.0f);
			}
			break;
		}

		case ID_UNDO:
		{
			if (g_nRoot)
			{
				m_pcPrimMenu->SetSelection(m_nPrimPos, true);
				m_pcAltMenu->SetSelection(m_nAltPos, true);
				m_pcUseAlt->SetValue(g_nHasAlt, true);
				m_pcDelaySlider->SetValue((float)g_nDelay);
				m_pcRepeatSlider->SetValue((float)g_nRepeat);
			}
			break;
		}

		case ID_USEALT:
		{
			m_pcAltMenu->SetEnable(m_pcUseAlt->GetValue());
			break;
		}

		default:
			os::Window::HandleMessage(pcMessage);
			break;
	}
}

bool KeyboardWind::OkToQuit()
{
	int hDirFd, hFd;
	float avPoint[2];
	os::Rect cRect = GetFrame();

	avPoint[0] = cRect.left;
	avPoint[1] = cRect.top;

	if ((hDirFd = open(getenv("HOME"), O_RDONLY | O_DIRECTORY)) >= 0)
	{
		if ((hFd = based_open(hDirFd, APP_CONFIG, O_WRONLY | O_CREAT | O_TRUNC, 0644 & ~umask(0))) >= 0) {
			if (write(hFd, avPoint, sizeof(avPoint)) != sizeof(avPoint))
				std::cout << "Writing window position error" << std::endl;
			close(hFd);
		}
		else
			std::cout << "Error opening config file for writing" << std::endl;
		close(hDirFd);
	}
	else
		std::cout << "Reading home directory error" << std::endl;

	os::Application::GetInstance()->PostMessage(os::M_QUIT);
	return false;
}

KeyboardWind::~KeyboardWind()
{
}

void KeyboardWind::Apply()
{
	if (g_nRoot)
	{
		int hFd;
		const char *zKeymapFile;
		char *zAltKeymapPath = g_zAltKeymapPath;
		unsigned char anStringSize[2];
		ssize_t nCurStringSize;
		bool nHasAlt = m_pcUseAlt->GetValue();

		os::String cPrimKeymapName(m_pcPrimMenu->GetCurrentString());
		os::String cAltKeymapName(m_pcAltMenu->GetCurrentString());

		os::Application::GetInstance()->SetKeyboardTimings((int)m_pcDelaySlider->GetValue(), (int)m_pcRepeatSlider->GetValue());

		strcpy(g_zPrimKeymapPath + KEYMAP_PATH, m_pcPrimMenu->GetCurrentString().c_str());
		if (m_pcUseAlt->GetValue())
			strcpy(g_zAltKeymapPath + KEYMAP_PATH, m_pcAltMenu->GetCurrentString().c_str());
		else
			zAltKeymapPath = NULL;
		zKeymapFile = GetKeymap(g_zPrimKeymapPath,  zAltKeymapPath);
		if (zKeymapFile)
			os::Application::GetInstance()->SetKeymap(zKeymapFile + KEYMAP_PATH);

		anStringSize[0] = cAltKeymapName.CountChars();
		anStringSize[1] = nHasAlt ? cPrimKeymapName.CountChars() : 0;

		if ((hFd = open(KEYMAPS_CONFIG, O_WRONLY | O_CREAT | O_TRUNC, 0644 & ~umask(0))) >= 0)
		{
			if (write(hFd, anStringSize, sizeof(anStringSize)) == sizeof(anStringSize))
			{
				nCurStringSize = anStringSize[0];
				if (write(hFd, cAltKeymapName.c_str(), nCurStringSize) == nCurStringSize)
				{
					if (nHasAlt) {
						nCurStringSize = anStringSize[1];
						if (write(hFd, cPrimKeymapName.c_str(), nCurStringSize) != nCurStringSize)
							std::cout << "Error writing primary keymap information" << std::endl;
					}
				}
				else
					std::cout << "Error writing alternate keymap information" << std::endl;
			}
			else
				std::cout << "Error writing keymaps information" << std::endl;
			close(hFd);
		}
		else
			std::cout << "Error opening keymap settings file for writing" << std::endl;
	}
}

KeyboardApp::KeyboardApp()
	: os::Application("application/x.vnd-syllable-Keyboard")
{
	int hDirFd, hFd;
	const float vYdelta = g_nRoot ? KEYBWIND_YDELTA_ROOT : KEYBWIND_YDELTA;
	struct stat stbuf;
	float avPoint[2];
	unsigned char anStringSize[2];
	ssize_t nCurStringSize;
	os::String cKeymapName;
	bool nBadPrimKeymapInfo = true, nBadAltKeymapInfo = true;

	// get current resolution
	os::Desktop *pcDesk = new os::Desktop;
	os::Point cDeskPoint(pcDesk->GetResolution());

	os::Rect cRect(20.0f, 20.0f, 20.0f + KEYBWIND_XDELTA, 20.0f + vYdelta);
	delete pcDesk;
	GetKeyboardConfig(&cKeymapName, &g_nDelay, &g_nRepeat);

	if (cKeymapName == DUAL_KEYMAP)
		g_nHasAlt = true;
	else {
		nBadPrimKeymapInfo = false;
		g_zPrimKeymap = strdup(cKeymapName.c_str());
	}

	// opening settings
	if ((hDirFd = open(getenv("HOME"), O_RDONLY | O_DIRECTORY)) >= 0)
	{
		if (((hFd = based_open(hDirFd, APP_CONFIG, O_RDONLY)) >= 0) && (fstat(hFd, &stbuf) >= 0))
		{
			if (S_ISREG(stbuf.st_mode))
			{
				// get window position
				if (read(hFd, avPoint, sizeof(avPoint)) == sizeof(avPoint))
				{
					if ((avPoint[0] > cDeskPoint.x) || (avPoint[1] > cDeskPoint.y))
						std::cout << "Window position is incorrect" << std::endl;
					else
					{
						cRect.left = avPoint[0];
						cRect.top = avPoint[1];
						cRect.right = cRect.left + KEYBWIND_XDELTA;
						cRect.bottom = cRect.top + vYdelta;
					}
				}
				else
					std::cout << "Reading window position error" << std::endl;
			}
			else
				std::cout << "Config file is not regular or symbolic link" << std::endl;
			close(hFd);
		}
		else
			std::cout << "Error opening config file for reading" << std::endl;
		close(hDirFd);
	}
	else
		std::cout << "Reading home directory error" << std::endl;

	if (((hFd = open(KEYMAPS_CONFIG, O_RDONLY)) >= 0) && (fstat(hFd, &stbuf) >= 0))
	{
		if (S_ISREG(stbuf.st_mode))
		{
			if (read(hFd, anStringSize, sizeof(anStringSize)) == sizeof(anStringSize))
			{
				nCurStringSize = anStringSize[0];
				g_zAltKeymap = (char *)malloc(nCurStringSize + 1);
				if (nCurStringSize && (read(hFd, g_zAltKeymap, nCurStringSize) == nCurStringSize))
				{
					g_zAltKeymap[nCurStringSize] = '\0';
					nBadAltKeymapInfo = false;
					if (g_nHasAlt)
					{
						nCurStringSize = anStringSize[1];
						g_zPrimKeymap = (char *)malloc(nCurStringSize + 1);
						if (nCurStringSize && (read(hFd, g_zPrimKeymap, nCurStringSize) == nCurStringSize))
						{
							g_zPrimKeymap[nCurStringSize] = '\0';
							nBadPrimKeymapInfo = false;
						}
						else
						{
							free(g_zPrimKeymap);
							std::cout << "Reading primary keymap information error" << std::endl;
						}
					}
				}
				else
				{
					free(g_zAltKeymap);
					std::cout << "Reading alternate keymap information error" << std::endl;
				}
			}
			else
				std::cout << "Reading keymaps information error" << std::endl;
		}
		else
			std::cout << "Keymaps config file is not regular or symbolic link" << std::endl;
		close(hFd);
	}
	else
		std::cout << "Keymaps settings file not found" << std::endl;

	if (nBadPrimKeymapInfo)
		g_zPrimKeymap = strdup(DEFAULT_KEYMAP);
	if (nBadAltKeymapInfo)
		g_zAltKeymap =strdup(DEFAULT_KEYMAP);

	m_pcWind = new KeyboardWind(cRect);
	m_pcWind->Show();
	m_pcWind->MakeFocus();
}

bool KeyboardApp::OkToQuit()
{
	return true;
}

KeyboardApp::~KeyboardApp()
{
	m_pcWind->Close();
}

int main()
{
	if (getuid() == 0)
		g_nRoot = true;
	KeyboardApp *pcApp = new KeyboardApp;
	pcApp->Run();
	delete pcApp;
	return 0;
}
