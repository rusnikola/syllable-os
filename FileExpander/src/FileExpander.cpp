/*
 *  FileExpander 0.7 (GUI files extraction tool)
 *  Copyright (c) 2004 Ruslan Nickolaev (nruslan@hotbox.ru)
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

// Headers
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>
#include <wait.h>
#include <atheos/filesystem.h>
#include <atheos/fs_attribs.h>
#include <util/invoker.h>
#include <util/message.h>
#include <util/application.h>
#include <util/resources.h>
#include <gui/desktop.h>
#include <gui/window.h>
#include <gui/view.h>
#include <gui/requesters.h>
#include <gui/menu.h>
#include <gui/filerequester.h>
#include <gui/button.h>
#include <gui/stringview.h>
#include <gui/frameview.h>
#include <gui/radiobutton.h>
#include <gui/checkbox.h>
#include <gui/image.h>
#include <gui/bitmap.h>
#include <iostream>
#include "etextview.h"

// Constants
#define GUI_FILE_BROWSER "FileBrowser"
#define SHELL "/bin/bash"
#define EXPANDER_VERSION "0.7.1"
#define EXPANDER_SETTINGS "config/FileExpander.cfg"

enum Expander_Settings
{
    EXPANDER_EXTRA = 300,
    EXPANDER_EXTRA_BORDER = 10,
    TEXTBUF_MAXINDEX = 4095,
    HASH_SIZE = 256,
    HASH_COUNT = 2,
    HASH_FULL_SIZE = HASH_SIZE * HASH_COUNT,
    HASH_MULTIPLIER = 31,
    RULE_COUNT = 2,
    STATUS_STRING = 15,
    FBROWSERLEN = 12,
    COMMAND_MAX = 2 * PATH_MAX
};

// Preferences settings
enum Prefs_Settings
{
    AUTOEXPAND = 01,
    CLOSEWIN = 02,
    OPENFOLDER = 04,
    AUTOLISTING = 010,
    DESTFOLDER_BIT1 = 020,
    DESTFOLDER_BIT2 = 040
    // free: 0100 (for 1 byte)
    // free: 0200 (for 1 byte)
};

static char prefs_settings; // preferences variable

// Expander status
static char StatusBuffer[NAME_MAX + STATUS_STRING + 1] = "Expanding file ";

// File browser execute command
static char g_pzFileBrowser[FBROWSERLEN + PATH_MAX + 15] = GUI_FILE_BROWSER " ";

// Hash struct
struct hash_struct
{
   char *name;
   char **rule;
   hash_struct *next;
};

// Global variables
static char *defDestPath, *pzTextBuffer, **rule;
static hash_struct **hash_table;

// Main window errors
const static char *ExpanderError[] =
{
    "Destination path isn't correct! ",
    "Source file not found! ",
    "Source is a directory! ",
    "Unrecognized file format! ",
    "File unpacking/listing in progress now! "
};

//
// Expander status
//
const static char *ExpanderStatus[] =
{
    "Expanding aborted",
    "File expanded",
    "Error occurred"
};

//
// Menu elements
//
static char *ExpanderMenu[] = { "Application", "File", "Edit", 0 };

static struct g_sExpanderMenuItem {
    char *title;
    char *shortcut;
    char *icon;
} g_asExpanderMenuItem[] = {
    // Application
    { "Preferences...", "Ctrl+P", "prefs16x16.png" }, { "About...", "", "about16x16.png" },
    { "", NULL, NULL }, { "Quit", "Ctrl+Q", "quit16x16.png" }, { NULL, NULL, NULL },

    // File
    { "Set source...", "Ctrl+S", "source16x16.png" }, { "Set destination...", "Ctrl+D", "dest16x16.png" }, { "Set password...", "Ctrl+W", "passw16x16.png" },
    { "", NULL, NULL }, { "Expand", "Ctrl+E", "expand16x16.png" }, { "Show contents", "Ctrl+L", "show16x16.png" },
    { NULL, NULL, NULL },

    // Edit
    { "Cut", "Ctrl+X", "cut16x16.png" }, { "Copy", "Ctrl+C", "copy16x16.png" }, { "Paste", "Ctrl+V", "paste16x16.png" },
    { "Clear", "", "clear16x16.png" }, { "", NULL, NULL }, { "Select all", "", "select16x16.png" },
    { NULL, NULL, NULL }
};

// resources
static char *g_apzFE_Bitmap[] = {
    "about24x24.png", "about32x32.png", "prefs24x24.png",
    "passw24x24.png", "error24x24.png", "error32x32.png"
};

// "C"-style functions
extern "C" {
    static void ExpanderList(void *pData);
    static void ExpanderExtract(void *pData);
    static unsigned int hash(char *p);
}

// "C++"-style functions
os::Bitmap *CopyBitmap(os::Bitmap *pcSrcIcon);

class ExpanderWindow;

class ExpanderPassw : public os::Window
{
    public:
        ExpanderPassw(ExpanderWindow *pcParent);
        virtual void HandleMessage(os::Message *pcMessage);
        virtual ~ExpanderPassw();
    private:
        ExpanderWindow *m_pcParent;
        os::View *m_pcView;
        os::StringView *m_pcText;
        os::TextView *m_pcPassw;
        os::CheckBox *m_pcUsePassw;
        os::Button *m_pcAccept, *m_pcDiscard;

        enum m_ePasswMessages {
            M_PASSW_USEPASSW,
            M_PASSW_ACCEPT,
            M_PASSW_DISCARD
        };
};

class ExpanderPreferences : public os::Window
{
public:
    ExpanderPreferences(const os::Rect &cFrame, ExpanderWindow *parentWindow);
    virtual void HandleMessage(os::Message *pcMessage);
    virtual ~ExpanderPreferences();
private:

    enum Prefs_Index
    {
        M_PREF_SAVE,
        M_PREF_CANCEL,
        M_PREF_AUTO_EXPAND,
        M_PREF_CLOSE_WIN,
        M_PREF_SAME_LEAVE_DIR,
        M_PREF_USE_DIR,
        M_PREF_SELECT,
        M_PREF_OPEN_DIST_EXTR,
        M_PREF_AUTO_CONTENTS
    };

    void SetPrefBit(bool nValue, int nBit);

    os::View *m_pcView;
    os::FrameView *m_pcFrameView;
    ExpanderWindow *m_pcParent;
    os::Button *m_pcSaveButton, *m_pcCancelButton, *m_pcSelectButton;
    os::StringView *m_pcExpansionString, *m_pcDestination, *m_pcOtherString;
    os::CheckBox *m_pcAutoExpand, *m_pcCloseWindow, *m_pcOpenDistExtr, *m_pcAutoContents;
    os::RadioButton *m_pcLeaveEmpty, *m_pcSameDir, *m_pcUseDir;
    os::TextView *m_pcDirText;
    os::FileRequester *m_pcFileReq;

    // flags
    bool IsFileReq;
};

class ExpanderErrors : public os::Window
{
public:
    ExpanderErrors(const os::Rect &cFrame, ExpanderWindow *parentWindow);
    void AddErrorText();
    virtual ~ExpanderErrors();
    os::TextView *m_pcErrorText;
    os::View *m_pcView;
    ExpanderWindow *m_pcParent;
};

class ExpanderWindow : public os::Window
{
public:
    ExpanderWindow(const os::Rect &cFrame, const char *pzPath);
    virtual bool OkToQuit();
    void ListUnLock(bool anAction);
    void SwitchExpand();
    void UpdateInfo();
    virtual void HandleMessage(os::Message *pcMessage);
    virtual ~ExpanderWindow();
    char *m_sysPath[RULE_COUNT], *CWDPath;
    os::String m_pcPasswString;
    pid_t shell_process, list_process;
    os::TextView *pcListArchive;
    os::StringView *pcExpandStatus;
    os::CheckBox *m_pcList;
    ExpanderPreferences *m_pcPrefWind;
    ExpanderPassw *m_pcPasswWind;
    ExpanderErrors *m_pcErrWind;
    bool m_cExpandList, m_nPasswEnable, IsNotFullyListed;

    enum Window_Index
    {
        M_MENU_APPLICATION_PREFS,
        M_MENU_APPLICATION_ABOUT,
        M_MENU_APPLICATION_QUIT,
        M_MENU_FILE_SOURCE,
        M_MENU_FILE_DEST,
        M_MENU_FILE_PASSW,
        M_MENU_FILE_EXPAND,
        M_MENU_FILE_LIST,
        M_MENU_EDIT_CUT,
        M_MENU_EDIT_COPY,
        M_MENU_EDIT_PASTE,
        M_MENU_EDIT_CLEAR,
        M_MENU_EDIT_SELECTALL,
        MENU_ITEM_COUNT, // count of menu items
        M_FILEREQ_LOAD,
        M_FILEREQ_SAVE,
        M_CHECKBOX_LIST,
        M_TEXTVIEW_SOURCE,
        M_TEXTVIEW_DEST,
        M_TEXTVIEW_LIST
    };

    enum m_eFE_Bitmaps {
        FEBITMAP_ABOUT24X24,
        FEBITMAP_ABOUT32X32,
        FEBITMAP_PREFS24X24,
        FEBITMAP_PASSW24X24,
        FEBITMAP_ERROR24X24,
        FEBITMAP_ERROR32X32,
        FEBITMAP_COUNT // count of bitmaps
    };

    os::Bitmap *m_apcBitmap[FEBITMAP_COUNT];

private:

    enum Error_Index
    {
        ERR_DEST_NOT_FOUND,
        ERR_SOURCE_NOT_FOUND,
        ERR_SOURCE_IS_DIR,
        ERR_UNKNOWN_FORMAT,
        ERR_QUIT
    };

    enum Menu_Index
    {
        M_MENU_APPLICATION,
        M_MENU_FILE,
        M_MENU_EDIT,
        MENU_COUNT // count of menus
    };

    void GetCommand(char *pzBuffer, const char *pzSource, char *pzCombRule, const char *pzPassw);
    void ShowError(int nCode);
    void UpdatingMenu(os::Menu *menuName, bool expr, os::MenuItem *trueItem, os::MenuItem *falseItem);
    void SetFunctionsEnable(bool bStatus);
    char *GetSource(char *srcPath);
    bool GetRule(unsigned int i, char *pzText);
    os::Menu *pcMenuBar, *m_pcMenu[MENU_COUNT];
    os::MenuItem *hideList, *stopMenuItem, *m_pcMenuItem[MENU_ITEM_COUNT];
    os::FileRequester *pcSetSource, *pcSetDest;
    os::Button *pcSourceButton, *pcDestButton, *pcExpandButton, *pcStopButton;
    os::TextView *pcSourceText, *pcDestText, *curTextView;
    os::View *m_pcView;
    char *m_oldListPath, *m_pcStatusBuffer;

    // flags
    bool IsExpand, IsFileReq;
};

class ExpanderApp : public os::Application
{
    public:
        ExpanderApp(const char *pzParams);
        virtual bool OkToQuit();
        virtual ~ExpanderApp();
    private:
        ExpanderWindow *m_pcWind;
        struct hash_struct *elem, **hash_ptr;
        char *FileBuf;
};

// ExpanderWindow constructor
ExpanderWindow::ExpanderWindow(const os::Rect &aRect, const char *pzPath)
    : os::Window(aRect, "FileExpander", "FileExpander", os::WND_NOT_V_RESIZABLE | os::WND_NO_ZOOM_BUT),
      m_pcPasswString(""), shell_process(0), list_process(0), m_pcPrefWind(NULL), m_pcPasswWind(NULL),
      m_cExpandList(false), m_nPasswEnable(false), IsNotFullyListed(true), curTextView(NULL),
      m_pcStatusBuffer(StatusBuffer + STATUS_STRING), IsExpand(true), IsFileReq(false)
{
    os::ResStream *pcResStream;
    os::Rect rect = GetBounds();

    CWDPath = getcwd(NULL, 0);
    os::Rect cMenuRect = rect;
    cMenuRect.bottom = 18.0f;
    rect.top = 19.0f;

    // creating buffers
    int i = 0;
    for (; i < RULE_COUNT; i++)
        m_sysPath[i] = new char[COMMAND_MAX + 1];
    m_oldListPath = new char[COMMAND_MAX + 1];

    // open resources
    os::Resources pcFEResources(get_image_id());

    // creating dynamic objects
    pcResStream = pcFEResources.GetResourceStream("hide16x16.png");
    hideList = new os::MenuItem("Hide contents", new os::Message(M_MENU_FILE_LIST), "Ctrl+L", new os::BitmapImage(pcResStream));
    delete pcResStream;
    hideList->SetTarget(this);

    pcResStream = pcFEResources.GetResourceStream("stop16x16.png");
    stopMenuItem = new os::MenuItem("Stop", new os::Message(M_MENU_FILE_EXPAND), "Ctrl+S", new os::BitmapImage(pcResStream));
    delete pcResStream;
    stopMenuItem->SetTarget(this);

    // creating and attaching menu
    pcMenuBar = new os::Menu(cMenuRect, "expander_menu", os::ITEMS_IN_ROW, os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT);

    // adding menus with menu items
    struct g_sExpanderMenuItem *psMenuItem = g_asExpanderMenuItem;
    char **ExpMenu = ExpanderMenu;
    os::Menu **pcMenu = m_pcMenu, *tmpMenu;

    for (i = 0; *ExpMenu; psMenuItem++)
    {
        tmpMenu = new os::Menu(os::Rect(0, 0, 1, 1), *ExpMenu++, os::ITEMS_IN_COLUMN);
        while (psMenuItem->title)
        {
            switch (*(psMenuItem->title))
            {
                case 0:
                    tmpMenu->AddItem(new os::MenuSeparator);
                    break;
                default:
                {
                    pcResStream = psMenuItem->icon ? pcFEResources.GetResourceStream(psMenuItem->icon) : NULL;
                    m_pcMenuItem[i] = new os::MenuItem(psMenuItem->title, new os::Message(i), psMenuItem->shortcut,  pcResStream ? new os::BitmapImage(pcResStream) : NULL);
                    if (pcResStream)
                        delete pcResStream;
                    tmpMenu->AddItem(m_pcMenuItem[i++]);
                    break;
                }
            }
            psMenuItem++;
        }
        pcMenuBar->AddItem(*pcMenu++ = tmpMenu);
    }

    pcMenuBar->SetTargetForItems(this);
    AddChild(pcMenuBar);

    // creating Expander View
    m_pcView = new os::View(rect, "expander_view", os::CF_FOLLOW_ALL);
    AddChild(m_pcView);
    SetFocusChild(m_pcView);

    rect = m_pcView->GetBounds();

    // creating and attaching buttons
    pcSourceButton = new os::Button(os::Rect(10, 10, 80, 30), "source_button", "Source", new os::Message( M_MENU_FILE_SOURCE ), os::CF_FOLLOW_NONE);
    m_pcView->AddChild(pcSourceButton);
    pcDestButton = new os::Button(os::Rect(10, 40, 80, 60), "dest_button", "Destination", new os::Message( M_MENU_FILE_DEST ), os::CF_FOLLOW_NONE);
    m_pcView->AddChild(pcDestButton);
    pcExpandButton = new os::Button(os::Rect(10, 70, 80, 90), "expand_button", "Expand", new os::Message(M_MENU_FILE_EXPAND), os::CF_FOLLOW_NONE);
    pcStopButton = new os::Button(os::Rect(10, 70, 80, 90), "stop_button", "Stop", new os::Message(M_MENU_FILE_EXPAND), os::CF_FOLLOW_NONE);
    m_pcView->AddChild(pcExpandButton);

    // creating and attaching text views
    pcSourceText = new EtextView(os::Rect(100, 10, rect.right - 20, 30), "source_text", pzPath, os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_LEFT);
    pcSourceText->SetMessage(new os::Message(M_TEXTVIEW_SOURCE));
    pcSourceText->SetMaxUndoSize(0);
    m_pcView->AddChild(pcSourceText);

    pcDestText = new EtextView(os::Rect(100, 40, rect.right - 20, 60), "dest_text", "", os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_LEFT);
    pcDestText->SetMessage(new os::Message(M_TEXTVIEW_DEST));
    pcDestText->SetMaxUndoSize(0);
    m_pcView->AddChild(pcDestText);

    // creating and adding CheckBox object (list archive)
    m_pcList = new os::CheckBox(os::Rect(rect.right - 120, 75, rect.right - 15, 90), "archive_list", "  Show contents", new os::Message(M_CHECKBOX_LIST), os::CF_FOLLOW_RIGHT);
    m_pcView->AddChild(m_pcList);

    // creating StringView object
    pcExpandStatus = new os::StringView(os::Rect(100, 75, rect.right - 130, 90), "expand_status", "", os::ALIGN_LEFT, os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT);
    m_pcView->AddChild(pcExpandStatus);

    // creating and adding TextView object (list archive)
    pcListArchive = new EtextView(os::Rect(EXPANDER_EXTRA_BORDER, rect.bottom + 1, rect.right - EXPANDER_EXTRA_BORDER, rect.bottom + EXPANDER_EXTRA - EXPANDER_EXTRA_BORDER),
        "text_archive_list", "", os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_TOP);
    pcListArchive->SetReadOnly();
    pcListArchive->SetMultiLine();
    pcListArchive->SetMessage(new os::Message(M_TEXTVIEW_LIST));
    pcListArchive->SetMaxUndoSize(0);
    os::Font *pcListFont = new os::Font(DEFAULT_FONT_FIXED);
    pcListArchive->SetFont(pcListFont);
    m_pcView->AddChild(pcListArchive);

    // window icon
    pcResStream = pcFEResources.GetResourceStream("icon24x24.png");
    os::BitmapImage *pcBitmapImage = new os::BitmapImage(pcResStream);
    delete pcResStream;
    SetIcon(pcBitmapImage->LockBitmap());
    delete pcBitmapImage;

    // set destination path
    UpdateInfo();

    // filerequester dialogs
    pcResStream = pcFEResources.GetResourceStream("source24x24.png");
    pcBitmapImage = new os::BitmapImage(pcResStream);
    delete pcResStream;
    pcSetSource = new os::FileRequester(os::FileRequester::LOAD_REQ, new os::Messenger(this), NULL, os::FileRequester::NODE_FILE, false, new os::Message(M_FILEREQ_LOAD), NULL, true, true, "Open", "Cancel");
    pcSetSource->SetIcon(pcBitmapImage->LockBitmap());
    pcSetSource->Start();
    delete pcBitmapImage;

    pcResStream = pcFEResources.GetResourceStream("dest24x24.png");
    pcBitmapImage = new os::BitmapImage(pcResStream);
    delete pcResStream;
    pcSetDest = new os::FileRequester(os::FileRequester::LOAD_REQ, new os::Messenger(this), NULL, os::FileRequester::NODE_DIR, false, new os::Message(M_FILEREQ_SAVE), NULL, true, true, "Select", "Cancel");
    pcSetDest->SetIcon(pcBitmapImage->LockBitmap());
    pcSetDest->Start();
    delete pcBitmapImage;

    // getting some other bitmaps
    for (i = 0; i < FEBITMAP_COUNT; i++) {
        pcResStream = pcFEResources.GetResourceStream(g_apzFE_Bitmap[i]);
        pcBitmapImage = new os::BitmapImage(pcResStream);
        delete pcResStream;
        m_apcBitmap[i] = CopyBitmap(pcBitmapImage->LockBitmap());
        delete pcBitmapImage;
    }
}

// virtual method: OkToQuit
bool ExpanderWindow::OkToQuit()
{
    if (list_process || shell_process)
        ShowError(ERR_QUIT);

    else
    {
        // save settings
        os::Rect wRect = GetFrame();

        // opening settings file descriptor
        int dir_fd = open(getenv("HOME"), O_RDONLY);
        int fd = based_open(dir_fd, EXPANDER_SETTINGS, O_CREAT | O_TRUNC);
        close(dir_fd);

        if (fd >= 0)
        {
            // writing settings
            write(fd, &(wRect.left), sizeof(float));
            write(fd, &(wRect.right), sizeof(float));
            write(fd, &(wRect.top), sizeof(float));
            write(fd, &prefs_settings, sizeof(prefs_settings));
            write(fd, defDestPath, strlen(defDestPath));

            // close file descriptor
            close(fd);
        }
        else
            std::cerr << "Error writing settings!" << std::endl;

        os::Application::GetInstance()->PostMessage(os::M_QUIT);
    }
    return false;
}

// virtual method: HandleMessage
void ExpanderWindow::HandleMessage(os::Message *pcMessage)
{
    switch (pcMessage->GetCode())
    {
        case M_MENU_APPLICATION_QUIT:
            PostMessage(os::M_QUIT);
            break;

        case M_MENU_APPLICATION_ABOUT:
        {
            os::Alert *pcAbout = new os::Alert("About", "FileExpander " EXPANDER_VERSION "\nGUI files extractor for Syllable\n\nThis program is distributed under the\nterms of GNU GPL 2.0 (see COPYING file) \n\nCopyright (c) 2004 Ruslan Nickolaev\nE-mail: nruslan@hotbox.ru", CopyBitmap(m_apcBitmap[FEBITMAP_ABOUT32X32]), os::WND_NO_CLOSE_BUT | os::WND_NO_ZOOM_BUT | os::WND_NO_DEPTH_BUT | os::WND_NOT_RESIZABLE, "Great", NULL);
            pcAbout->SetIcon(m_apcBitmap[FEBITMAP_ABOUT24X24]);
            pcAbout->CenterInWindow(this);
            pcAbout->Go(new os::Invoker);
            break;
        }

        case M_MENU_FILE_SOURCE:
            if (!IsFileReq)
            {
                IsFileReq = true;
                pcSetSource->CenterInWindow(this);
                pcSetSource->Show();
                pcSetSource->MakeFocus();
            }
            break;

        case M_MENU_FILE_DEST:
            if (!IsFileReq)
            {
                IsFileReq = true;
                pcSetDest->CenterInWindow(this);
                pcSetDest->Show();
                pcSetDest->MakeFocus();
            }
            break;

        case M_FILEREQ_LOAD:
	    {
            const char *SourcePath;
            if (pcMessage->FindString("file/path", &SourcePath) == 0)
                pcSourceText->Set(SourcePath);
            IsFileReq = false;
            UpdateInfo();
            if (m_pcList->GetValue())
                m_pcList->SetValue(false, true);
            break;
        }

        case M_FILEREQ_SAVE:
        {
            const char *SourcePath;
            if (pcMessage->FindString("file/path", &SourcePath) == 0)
                pcDestText->Set(SourcePath);
            IsFileReq = false;
            pcExpandStatus->SetString("");
            break;
        }

        case os::M_FILE_REQUESTER_CANCELED:
            IsFileReq = false;
            break;

        case M_MENU_FILE_PASSW:
        {
            if (!m_pcPasswWind)
            {
                m_pcPasswWind = new ExpanderPassw(this);
                m_pcPasswWind->CenterInWindow(this);
                m_pcPasswWind->Show();
                m_pcPasswWind->MakeFocus();
            }
            break;
        }

        case M_MENU_FILE_EXPAND:
        {
            // updating Expand (or Stop) button
            if (IsExpand)
            {
                SwitchExpand();
                pcExpandStatus->SetString("");
                char *sourcePath = (char *)pcSourceText->GetBuffer()[0].c_str();
                char *BaseName = GetSource(sourcePath);
                if (BaseName)
                {
                    const char *DestPath = pcDestText->GetBuffer()[0].c_str();
                    mkdir(DestPath, 0777 & ~umask(0));
                    if (chdir(DestPath)) ShowError(ERR_DEST_NOT_FOUND);
                    else
                    {
                        // getting a command
                        GetCommand(m_sysPath[1], sourcePath, rule[1], m_nPasswEnable ? m_pcPasswString.c_str() : NULL);

                        // updating a status string
                        if (strlen(BaseName) <= NAME_MAX)
                            strcpy(m_pcStatusBuffer, BaseName);
                        else
                            *m_pcStatusBuffer = '\0';
                        pcExpandStatus->SetString(StatusBuffer);

                        // creating ExpanderErrors window
                        m_pcErrWind = new ExpanderErrors(os::Rect(0, 0, 400, 300), this);
                        m_pcErrWind->CenterInWindow(this);

                        // creating expander extract thread
                        thread_id extract_thread = spawn_thread("expander extract", (void *)ExpanderExtract, NORMAL_PRIORITY, 0, this);
                        resume_thread(extract_thread);

                        break;
                    }
                }
                SwitchExpand();
            }
            else
            {
                pid_t shellproc = -shell_process;
                shell_process = 0;
                kill(shellproc, SIGINT);
            }
            break;
        }

        case M_MENU_FILE_LIST:
            // we are simply reverse status of our CheckBox
            m_pcList->SetValue(!(m_pcList->GetValue()), true);
            break;

        case M_CHECKBOX_LIST:
        {
            pcExpandStatus->SetString("");
            bool IsList = m_pcList->GetValue();

            // resizing window
            os::Rect aRect = GetFrame();
            aRect.bottom = IsList ? aRect.bottom + EXPANDER_EXTRA : aRect.bottom - EXPANDER_EXTRA;
            SetFrame(aRect);

            // updating menu
            UpdatingMenu(m_pcMenu[M_MENU_FILE], IsList, hideList, m_pcMenuItem[M_MENU_FILE_LIST]);

            if (IsList)
            {
                ListUnLock(false);
                char *sourcePath = (char *)pcSourceText->GetBuffer()[0].c_str();
                if (IsNotFullyListed || strcmp(sourcePath, m_oldListPath))
                {
                    pcListArchive->Clear();

                    if (GetSource(sourcePath))
                    {
                        IsNotFullyListed = false;

                        // saving path
                        strcpy(m_oldListPath, sourcePath);

                        // getting a command
                        GetCommand(m_sysPath[0], sourcePath, rule[0], m_nPasswEnable ? m_pcPasswString.c_str() : NULL);

                        thread_id list_thread = spawn_thread("expander_list", (void *)ExpanderList, NORMAL_PRIORITY, 0, this);
                        resume_thread(list_thread);
                        break;
                    }
                    else
                    {
                        m_cExpandList = false;
                        IsNotFullyListed = true;
                    }
                }
                ListUnLock(true);
            }
            else if (list_process)
            {
                 IsNotFullyListed = true;
                 kill(-list_process, SIGINT);
            }
            break;
        }

        // little hack
        case M_TEXTVIEW_SOURCE:
            curTextView = pcSourceText;
            break;

        case M_TEXTVIEW_DEST:
            curTextView = pcDestText;
            break;

        case M_TEXTVIEW_LIST:
            curTextView = pcListArchive;
            break;

        case M_MENU_EDIT_CUT:
            if (curTextView && (curTextView != pcListArchive))
                curTextView->Cut();
            break;

        case M_MENU_EDIT_COPY:
            if (curTextView)
                curTextView->Copy();
            break;

        case M_MENU_EDIT_PASTE:
            if (curTextView && (curTextView != pcListArchive))
                curTextView->Paste();
            break;

        case M_MENU_EDIT_CLEAR:
            if (curTextView && (curTextView != pcListArchive))
                curTextView->Delete();
            break;

        case M_MENU_EDIT_SELECTALL:
            if (curTextView)
                curTextView->SelectAll();
            break;

        // Preferences
        case M_MENU_APPLICATION_PREFS:
            // Expander Preferences
            if (!m_pcPrefWind)
            {
               m_pcPrefWind = new ExpanderPreferences(os::Rect(200, 200, 500, 540), this);
               m_pcPrefWind->CenterInWindow(this);
               m_pcPrefWind->Show();
               m_pcPrefWind->MakeFocus();
            }
            break;

        default:
            os::Window::HandleMessage(pcMessage);
            break;
    }
}

// Getting a command
void ExpanderWindow::GetCommand(char *pzBuffer, const char *pzSource, char *pzCombRule, const char *pzPassw)
{
    char *pzFileName = (char *)malloc(strlen(pzSource) + 3);
    char *pzPassword;
    char pzRule[4096];
    char *pzRuleTmp = pzRule, *pzTmp, *pzTmp2;
    char nTmpSymbol;

    // a file name may contains spaces
    // and some other special symbols
    sprintf(pzFileName, "\"%s\"", pzSource);

    // the code below gets a normal rule (simular to rule for FileExpander v0.6)
    // including password (if you specify it)
    while (*pzCombRule)
    {
        if (*pzCombRule == '[')
        {
            pzTmp = ++pzCombRule;
            while (*pzCombRule && (*(pzCombRule++) != ']'));
            if (pzPassw)
            {
                pzTmp2 = pzCombRule - 1;
                nTmpSymbol = *pzTmp2;
                *pzTmp2 = '\0';
                pzPassword = (char *)malloc(strlen(pzPassw) + 3);
                sprintf(pzPassword, "\"%s\"", pzPassw);
                sprintf(pzRuleTmp, pzTmp, pzPassword);
                free(pzPassword);
                *pzTmp2 = nTmpSymbol;
                while (*pzRuleTmp)
                    pzRuleTmp++;
            }
        }
        else
            *(pzRuleTmp++) = *(pzCombRule++);
    }

    *pzRuleTmp = '\0';

    // now we can get a command easily...
    sprintf(pzBuffer, pzRule, pzFileName);

    free(pzFileName);
}

// Changing menu elements
void ExpanderWindow::UpdatingMenu(os::Menu *menuName, bool expr, os::MenuItem *trueItem, os::MenuItem *falseItem)
{
    os::MenuItem *oldItemList, *newItemList;
    if (expr)
    {
        oldItemList = falseItem;
        newItemList = trueItem;
    }
    else
    {
        oldItemList = trueItem;
        newItemList = falseItem;
    }
    int nIndex = menuName->GetIndexOf(oldItemList);
    menuName->RemoveItem(nIndex);
    menuName->AddItem(newItemList, nIndex);
}

// Updating window information
void ExpanderWindow::UpdateInfo()
{
    // clear expander status
    pcExpandStatus->SetString("");

    // updating destination folder path
    switch (prefs_settings & (DESTFOLDER_BIT1 | DESTFOLDER_BIT2))
    {
        case 0: // leave empty string
            pcDestText->Set("");
            break;
        case DESTFOLDER_BIT1:
        {
            char *dirBuf = strdup(pcSourceText->GetBuffer()[0].c_str());
            char *dirPath = dirname(dirBuf);
            if (!strcmp(dirPath, "."))
                pcDestText->Set("");
            else
                pcDestText->Set(dirPath);
            free(dirBuf);
            break;
        }
        default:
            pcDestText->Set(defDestPath);
            break;
    }
}

// Change elements status
void ExpanderWindow::SetFunctionsEnable(bool bStatus)
{
    m_pcMenuItem[M_MENU_FILE_SOURCE]->SetEnable(bStatus);
    m_pcMenuItem[M_MENU_FILE_DEST]->SetEnable(bStatus);
    m_pcMenuItem[M_MENU_FILE_PASSW]->SetEnable(bStatus);
    m_pcMenuItem[M_MENU_APPLICATION_PREFS]->SetEnable(bStatus);
    pcSourceButton->SetEnable(bStatus);
    pcDestButton->SetEnable(bStatus);
    pcSourceText->SetEnable(bStatus);
    pcDestText->SetEnable(bStatus);
}

// Expand switching function
void ExpanderWindow::SwitchExpand()
{
    os::Button *oldButton, *newButton;
    UpdatingMenu(m_pcMenu[M_MENU_FILE], IsExpand, stopMenuItem, m_pcMenuItem[M_MENU_FILE_EXPAND]);

    if (IsExpand)
    {
        oldButton = pcExpandButton;
        newButton = pcStopButton;
    }
    else
    {
        oldButton = pcStopButton;
        newButton = pcExpandButton;
    }

    m_pcView->RemoveChild(oldButton);
    m_pcView->AddChild(newButton);
    IsExpand = !IsExpand;
    m_pcList->SetEnable(IsExpand);

    if (m_pcList->GetValue())
        hideList->SetEnable(IsExpand);
    else
        m_pcMenuItem[M_MENU_FILE_LIST]->SetEnable(IsExpand);

    SetFunctionsEnable(IsExpand);
}

// ListUnLock - lock/unlock functions during listing archive
void ExpanderWindow::ListUnLock(bool anAction)
{
    pcExpandButton->SetEnable(anAction);
    m_pcMenuItem[M_MENU_FILE_EXPAND]->SetEnable(anAction);
    SetFunctionsEnable(anAction);
}

// GetSource - collecting source information
char *ExpanderWindow::GetSource(char *srcPath)
{
    struct stat stbuf;
    char *tmpSrcPath = srcPath, *BaseName = NULL;
    int fd;
    bool getres = false;

    fd = open(srcPath, O_RDONLY);
    if (fd < 0)
        ShowError(ERR_SOURCE_NOT_FOUND);
    else
    {
        fstat(fd, &stbuf);
        if (S_ISDIR(stbuf.st_mode))
            ShowError(ERR_SOURCE_IS_DIR);
        else
        {
            BaseName = srcPath;
            while (*tmpSrcPath)
                if (*tmpSrcPath++ == '/') BaseName = tmpSrcPath;

            struct attr_info mime_info;

            if (stat_attr(fd, "os::MimeType", &mime_info) >= 0)
            {
                char *mime_buf = new char[mime_info.ai_size + 1];
                if (read_attr(fd, "os::MimeType", ATTR_TYPE_STRING, mime_buf, 0, mime_info.ai_size) == mime_info.ai_size)
                {
                    mime_buf[mime_info.ai_size] = '\0'; // NULL-terminating
                    getres = GetRule(0, mime_buf);                
                }
                delete [] mime_buf;
            }

            if (!getres)
                for (tmpSrcPath = BaseName; *tmpSrcPath; tmpSrcPath++)
                    if ((*tmpSrcPath == '.') && (getres = GetRule(1, tmpSrcPath))) break;
            if (!getres)
            {
                ShowError(ERR_UNKNOWN_FORMAT);
                BaseName = NULL;
            }
        }
        close(fd);
    }
    return BaseName;
}

// Thread function: listing archive
void ExpanderList(void *pData)
{
    int aPipe[2];
    ExpanderWindow *expwin = (ExpanderWindow *)pData;

    pipe(aPipe);

    pid_t pid = fork();

    if (pid)
    {
        expwin->list_process = pid;
        int i = 0, g = 1, list_in = aPipe[0];
        char ch;
        os::TextView *pcListArchive = expwin->pcListArchive;
        close(aPipe[1]);

        do
        {
            if ((i == TEXTBUF_MAXINDEX) || ((g = read(list_in, &ch, 1)) != 1))
            {
                pzTextBuffer[i] = '\0';
                i = 0;
                expwin->Lock();
                pcListArchive->Insert(pzTextBuffer);
                expwin->Unlock();
            }
            else
                pzTextBuffer[i++] = ch;
        }
        while (g == 1);

        close(list_in);
        expwin->Lock();
        expwin->list_process = 0;
        expwin->ListUnLock(true);
        if (expwin->m_cExpandList)
        {
            expwin->m_cExpandList = false;
            os::Invoker *pcParentInvoker = new os::Invoker(new os::Message(ExpanderWindow::M_MENU_FILE_EXPAND), expwin);
            pcParentInvoker->Invoke();
        }
        expwin->Unlock();
    }
    else
    {
        setsid();
        close(aPipe[0]);
        dup2(aPipe[1], STDOUT_FILENO);
        execlp(SHELL, SHELL, "-c", expwin->m_sysPath[0], NULL);
        close(aPipe[1]);
        _exit(1);
    }
}

// Thread function: extract archive
void ExpanderExtract(void *pData)
{
    int aPipe[2];
    ExpanderWindow *expwin = (ExpanderWindow *)pData;

    pipe(aPipe);
    pid_t pid = fork();

    if (pid)
    {
        int i = 0, g = 1, unpack_in = aPipe[0];
        char ch;
        ExpanderErrors *errwin = expwin->m_pcErrWind;
        os::TextView *pcErrorText = errwin->m_pcErrorText;

        expwin->shell_process = pid;
        close(aPipe[1]);

        do
        {
            if ((i == TEXTBUF_MAXINDEX) || ((g = read(unpack_in, &ch, 1)) != 1))
            {
                pzTextBuffer[i] = '\0';
                i = 0;
                pcErrorText->Insert(pzTextBuffer);
            }
            else
                pzTextBuffer[i++] = ch;
        }
        while (g == 1);

        close(unpack_in);

        // open FileBrowser window
        if (prefs_settings & OPENFOLDER)
        {
            char *pzCWDPath = getcwd(NULL, 0), *pzFBPath = g_pzFileBrowser + FBROWSERLEN;
            strcpy(pzFBPath, pzCWDPath);
            strcpy(pzFBPath + strlen(pzCWDPath), " >>/dev/null &");
            free(pzCWDPath);
            system(g_pzFileBrowser);
        }

        const char *str_ptr;

        // error occured => open error window
        if (*pzTextBuffer)
        {
            errwin->Show();
            errwin->MakeFocus();
            errwin->Lock();
            errwin->AddErrorText();
            errwin->Unlock();
            str_ptr = ExpanderStatus[2];
        }
        // no errors => close error window
        else
        {
            errwin->Close();
            str_ptr = expwin->shell_process ? ExpanderStatus[1] : ExpanderStatus[0];
        }

        expwin->Lock();
        chdir(expwin->CWDPath);
        expwin->SwitchExpand();
        expwin->pcExpandStatus->SetString(str_ptr);
        expwin->shell_process = 0;
        expwin->Unlock();

        // if error occured we aren't closing any windows
        if (!(*pzTextBuffer) && (prefs_settings & CLOSEWIN))
        {
            os::Invoker *pcParentInvoker = new os::Invoker(new os::Message(os::M_QUIT), expwin);
            pcParentInvoker->Invoke();
        }
    }

    else
    {
        setsid();
        close(aPipe[0]);
        dup2(aPipe[1], STDERR_FILENO);
        execlp(SHELL, SHELL, "-c", expwin->m_sysPath[1], NULL);
        close(aPipe[1]);
        _exit(1);
    }
}

// Show Error message (nCode is code of the error)
void ExpanderWindow::ShowError(int nCode)
{
    os::Alert *pcError = new os::Alert("Error", ExpanderError[nCode], CopyBitmap(m_apcBitmap[FEBITMAP_ERROR32X32]), os::WND_NO_CLOSE_BUT | os::WND_NO_ZOOM_BUT | os::WND_NO_DEPTH_BUT | os::WND_NOT_RESIZABLE, "OK", NULL);
    pcError->SetIcon(m_apcBitmap[FEBITMAP_ERROR24X24]);
    pcError->CenterInWindow(this);
    pcError->Go(new os::Invoker);
}

// Getting Rule
bool ExpanderWindow::GetRule(unsigned int i, char *pzText)
{
    struct hash_struct *elem = hash_table[hash(pzText) + HASH_SIZE * i];
    while (elem)
    {
        if (!strcmp(elem->name, pzText))
        {
            rule = elem->rule;
            return true;
        }
        elem = elem->next;
    }
    return false;
}

// ExpanderWindow destructor
ExpanderWindow::~ExpanderWindow()
{
    // remove all bitmaps
    for (os::Bitmap **ppcBitmap = m_apcBitmap + FEBITMAP_COUNT - 1; ppcBitmap >= m_apcBitmap; ppcBitmap--)
        delete (*ppcBitmap);

    // remove CWD Path buffer
    free(CWDPath);

    // removing commans buffers
    for (int i = 0; i < RULE_COUNT; i++)
        delete [] m_sysPath[i];
    delete [] m_oldListPath;

    pcSetSource->Close();
    pcSetDest->Close();

    if (m_pcPrefWind)
        m_pcPrefWind->Close();
}

// ExpanderPreferences constructor
ExpanderPreferences::ExpanderPreferences(const os::Rect &cFrame, ExpanderWindow *parentWindow)
    : os::Window(cFrame, "expander_prefwindow", "Preferences", os::WND_NO_ZOOM_BUT | os::WND_NO_DEPTH_BUT | os::WND_NOT_RESIZABLE),
      m_pcParent(parentWindow), IsFileReq(false)
{
    // Creating Pref View
    m_pcView = new os::View(GetBounds(), "expender_prefview", os::CF_FOLLOW_NONE);
    AddChild(m_pcView);

    // Creating FrameView rectangle
    os::Rect aRect = m_pcView->GetBounds();
    aRect.left += 10;
    aRect.right -= 10;
    aRect.top += 10;
    aRect.bottom -= 50;

    // Creating Pref FrameView
    m_pcFrameView = new os::FrameView(aRect, "pref_frameview", "Preferences", os::CF_FOLLOW_NONE);
    m_pcView->AddChild(m_pcFrameView);

    // Expansion
    m_pcExpansionString = new os::StringView(os::Rect(15, 30, 100, 40), "expansion_string", "Expansion:", os::ALIGN_LEFT, os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcExpansionString);
    m_pcAutoExpand = new os::CheckBox(os::Rect(20, 50, 200, 65), "auto_expand", "Automatically expand files", new os::Message(M_PREF_AUTO_EXPAND), os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcAutoExpand);
    m_pcCloseWindow = new os::CheckBox(os::Rect(20, 70, 250, 85), "close_win", "Close window when done expanding", new os::Message(M_PREF_CLOSE_WIN), os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcCloseWindow);

    // Destination Folders
    m_pcDestination = new os::StringView(os::Rect(15, 105, 150, 115), "dest_folders", "Destination Folders:", os::ALIGN_LEFT, os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcDestination);
    m_pcLeaveEmpty = new os::RadioButton(os::Rect(20, 125, 250, 140), "leave_empty", "Leave empty (current directory)", new os::Message(M_PREF_SAME_LEAVE_DIR), os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcLeaveEmpty);
    m_pcSameDir = new os::RadioButton(os::Rect(20, 145, 250, 160), "same_dir", "Same directory as source (archive) file", new os::Message(M_PREF_SAME_LEAVE_DIR), os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcSameDir);
    m_pcUseDir = new os::RadioButton(os::Rect(20, 165, 65, 180), "use_dir", "Use: ", new os::Message(M_PREF_USE_DIR), os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcUseDir);
    m_pcDirText = new EtextView(os::Rect(70, 163, 210, 182), "dir_text", defDestPath, os::CF_FOLLOW_NONE);
    m_pcDirText->SetMaxUndoSize(0);
    m_pcFrameView->AddChild(m_pcDirText);
    m_pcSelectButton = new os::Button(os::Rect(220, 163, 265, 182), "select_button", "Select", new os::Message(M_PREF_SELECT), os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcSelectButton);

    // Other
    m_pcOtherString = new os::StringView(os::Rect(15, 200, 70, 210), "other", "Other:", os::ALIGN_LEFT, os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcOtherString);
    m_pcOpenDistExtr = new os::CheckBox(os::Rect(20, 220, 250, 235), "open_dist_extr", "Open destination folder after extracting", new os::Message(M_PREF_OPEN_DIST_EXTR), os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcOpenDistExtr);
    m_pcAutoContents = new os::CheckBox(os::Rect(20, 240, 250, 255), "auto_contents", "Automatically show contents listing", new os::Message(M_PREF_AUTO_CONTENTS), os::CF_FOLLOW_NONE);
    m_pcFrameView->AddChild(m_pcAutoContents);

    // Updating rectangle
    aRect.top = aRect.bottom + 15;
    aRect.bottom = aRect.bottom + 35;

    // Creating and attaching "Save" button
    m_pcSaveButton = new os::Button(os::Rect(205, aRect.top, 270, aRect.bottom), "save_button", "Save", new os::Message(M_PREF_SAVE), os::CF_FOLLOW_NONE); 
    m_pcView->AddChild(m_pcSaveButton);

    // Creating and attaching "Cancel" button
    m_pcCancelButton = new os::Button(os::Rect(130, aRect.top, 195, aRect.bottom), "cancel_button", "Cancel", new os::Message(M_PREF_CANCEL), os::CF_FOLLOW_NONE); 
    m_pcView->AddChild(m_pcCancelButton);

    // load settings
    switch (prefs_settings & (DESTFOLDER_BIT1 | DESTFOLDER_BIT2))
    {
        case 0:
            m_pcLeaveEmpty->SetValue(1, true);
            break;
        case DESTFOLDER_BIT1:
            m_pcSameDir->SetValue(1, true);
            break;
        default:
            m_pcUseDir->SetValue(1, true);
    }

    m_pcAutoExpand->SetValue(prefs_settings & AUTOEXPAND, true);
    m_pcCloseWindow->SetValue(prefs_settings & CLOSEWIN, true);
    m_pcOpenDistExtr->SetValue(prefs_settings & OPENFOLDER, true);
    m_pcAutoContents->SetValue(prefs_settings & AUTOLISTING, true);

    // filerequester dialog
    m_pcFileReq = new os::FileRequester(os::FileRequester::LOAD_REQ, new os::Messenger(this), NULL, os::FileRequester::NODE_DIR, false, NULL, NULL, true, true, "Select", "Cancel");
    m_pcFileReq->Start();

    // setting default button
    SetDefaultButton(m_pcSaveButton);

    // set icon
    SetIcon(m_pcParent->m_apcBitmap[ExpanderWindow::FEBITMAP_PREFS24X24]);
}

// ExpanderPreferences destructor
ExpanderPreferences::~ExpanderPreferences()
{
    m_pcFileReq->Close();
    m_pcParent->m_pcPrefWind = NULL;
}

// setting up preferences bit
void ExpanderPreferences::SetPrefBit(bool nValue, int nBit)
{
    if (nValue)
        prefs_settings |= nBit;
    else
        prefs_settings &= ~nBit;
}

// virtual method: HandleMessage
void ExpanderPreferences::HandleMessage(os::Message *pcMessage)
{
    switch (pcMessage->GetCode())
    {
        case M_PREF_CANCEL:
            PostMessage(os::M_QUIT);
            break;

        case M_PREF_SAME_LEAVE_DIR:
        {
            m_pcDirText->SetEnable(false);
            m_pcSelectButton->SetEnable(false);
            break;
        }

        case M_PREF_USE_DIR:
        {
            m_pcDirText->SetEnable(true);
            m_pcSelectButton->SetEnable(true);
            SetFocusChild(m_pcDirText);
            break;
        }

        case M_PREF_SELECT:
            if (!IsFileReq)
            {
                IsFileReq = true;
                m_pcFileReq->CenterInWindow(this);
                m_pcFileReq->Show();
                m_pcFileReq->MakeFocus();
            }
            break;

        case os::M_LOAD_REQUESTED:
        {
            const char *SelectPath;
            if (pcMessage->FindString("file/path",&SelectPath) == 0)
            m_pcDirText->Set(SelectPath);
            IsFileReq = false;
            break;
        }

        case os::M_FILE_REQUESTER_CANCELED:
            IsFileReq = false;
            break;

        case M_PREF_SAVE:
        {
            if (m_pcLeaveEmpty->GetValue())
                 SetPrefBit(0, DESTFOLDER_BIT1 | DESTFOLDER_BIT2);
            else if (m_pcSameDir->GetValue())
            {
                 SetPrefBit(1, DESTFOLDER_BIT1);
                 SetPrefBit(0, DESTFOLDER_BIT2);
            }
            else
            {
                SetPrefBit(0, DESTFOLDER_BIT1);
                SetPrefBit(1, DESTFOLDER_BIT2);
            }

            SetPrefBit(m_pcAutoExpand->GetValue(), AUTOEXPAND);
            SetPrefBit(m_pcCloseWindow->GetValue(), CLOSEWIN);
            SetPrefBit(m_pcOpenDistExtr->GetValue(), OPENFOLDER);
            SetPrefBit(m_pcAutoContents->GetValue(), AUTOLISTING);

            // getting default path
            const char *dirPath = m_pcDirText->GetBuffer()[0].c_str();
            if (strlen(dirPath) <= PATH_MAX)
                strcpy(defDestPath, dirPath);
            else
                *defDestPath = '\0'; // cleaning string

            m_pcParent->Lock();
            m_pcParent->pcExpandStatus->SetString("");
            m_pcParent->Unlock();

            PostMessage(os::M_QUIT);
            break;
        }

        default:
            os::Window::HandleMessage(pcMessage);
            break;
    }
}

// ExpanderErrors constructor
ExpanderErrors::ExpanderErrors(const os::Rect &cFrame, ExpanderWindow *parentWindow)
 : os::Window(cFrame, "expander_error", "Errors", os::WND_NO_ZOOM_BUT | os::WND_NOT_V_RESIZABLE),
   m_pcParent(parentWindow)
{
    os::Rect cRect = GetBounds();
    SetSizeLimits(os::Point(cRect.right, cRect.bottom), os::Point(os::COORD_MAX, cRect.bottom));
    m_pcView = new os::View(cRect, "error_view", os::CF_FOLLOW_ALL);
    AddChild(m_pcView);

    float bottom = cRect.bottom - 20;
    cRect.top +=20;
    cRect.bottom = cRect.top + 20;
    cRect.left += 20;
    cRect.right -= 20;

    os::StringView *pcString = new os::StringView(cRect, "error_string", "Error occurred. File may be damaged! See log below...", os::ALIGN_CENTER, os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT);
    os::Font *pcStringFont = new os::Font(DEFAULT_FONT_BOLD);
    pcString->SetFont(pcStringFont);
    m_pcView->AddChild(pcString);

    cRect.top += 40;
    cRect.bottom = bottom - 40;

    m_pcErrorText = new EtextView(cRect, "error_log", "", os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_LEFT);
    m_pcErrorText->SetReadOnly();
    m_pcErrorText->SetMultiLine();
    m_pcErrorText->SetMaxUndoSize(0);
    os::Font *pcTextFont = new os::Font(DEFAULT_FONT_FIXED);
    m_pcErrorText->SetFont(pcTextFont);

    cRect.top = cRect.bottom + 20;
    cRect.bottom = bottom;
    cRect.right -= 20;
    cRect.left = cRect.right - 60;
    os::Button *pcButton = new os::Button(cRect, "err_close", "Close", new os::Message(os::M_QUIT), os::CF_FOLLOW_RIGHT);
    m_pcView->AddChild(pcButton);
    SetDefaultButton(pcButton);

    // set icon
    SetIcon(m_pcParent->m_apcBitmap[ExpanderWindow::FEBITMAP_ERROR24X24]);
}

// AddErrorText - attaching error text view
void ExpanderErrors::AddErrorText()
{
    m_pcView->AddChild(m_pcErrorText);
}

// ExpanderErrors destructor
ExpanderErrors::~ExpanderErrors()
{
    m_pcParent->m_pcErrWind = NULL;
}

// Hash function
unsigned int hash(char *p)
{
    unsigned int h = 0;
    unsigned char *ptr = (unsigned char *)p;
    while (*ptr)
        h = HASH_MULTIPLIER * h + *ptr++;
    return h % HASH_SIZE;
}

// Copy bitmap function
os::Bitmap *CopyBitmap(os::Bitmap *pcSrcIcon)
{
    os::Bitmap *pcDestIcon = new os::Bitmap((int)pcSrcIcon->GetBounds().Width() + 1,
      (int)pcSrcIcon->GetBounds().Height() + 1, pcSrcIcon->GetColorSpace(), os::Bitmap::SHARE_FRAMEBUFFER);
    memcpy(pcDestIcon->LockRaster(), pcSrcIcon->LockRaster(), pcSrcIcon->GetBytesPerRow() * (int)(pcSrcIcon->GetBounds().Height() + 1));
    return pcDestIcon;
}

// ExpanderPassw ("Set password..." window) constructor
ExpanderPassw::ExpanderPassw(ExpanderWindow *pcParent)
  : os::Window(os::Rect(0, 0, 200, 100), "set_passw", "Password settings", os::WND_NO_ZOOM_BUT | os::WND_NO_DEPTH_BUT | os::WND_NOT_RESIZABLE),
    m_pcParent(pcParent)
{
    os::Rect pcRect = GetBounds();
    const float vCenterX = pcRect.right / 2;
    const float vBottomBorder = pcRect.bottom - 10.0f;

    m_pcView = new os::View(pcRect, "set_passw_view", os::CF_FOLLOW_NONE);
    AddChild(m_pcView);

    // add string "Password:"
    m_pcText = new os::StringView(os::Rect(10.0f, 15.0f, 70.0f, 35.0f), "string_view", "Password:");
    m_pcView->AddChild(m_pcText);

    // add password text view
    m_pcPassw = new EtextView(os::Rect(75.0f, 15.0f, pcRect.right - 10.0f, 35.0f), "passw_view", m_pcParent->m_pcPasswString.c_str(), os::CF_FOLLOW_NONE);
    m_pcPassw->SetPasswordMode();
    m_pcPassw->SetMaxUndoSize(0);
    m_pcView->AddChild(m_pcPassw);

    // add "Use password for archive" check box
    m_pcUsePassw = new os::CheckBox(os::Rect(10.0f, 40.0f, pcRect.right, 65.0f), "use_passw", "Use password for archive", new os::Message(M_PASSW_USEPASSW), os::CF_FOLLOW_NONE);
    m_pcView->AddChild(m_pcUsePassw);
    m_pcUsePassw->SetValue(m_pcParent->m_nPasswEnable, true);

    // add buttons
    m_pcAccept = new os::Button(os::Rect(vCenterX + 5.0f, 70.0f, vCenterX + 70.0f, vBottomBorder), "accept_button", "Accept", new os::Message(M_PASSW_ACCEPT), os::CF_FOLLOW_NONE);
    m_pcView->AddChild(m_pcAccept);
    SetDefaultButton(m_pcAccept);

    m_pcDiscard = new os::Button(os::Rect(vCenterX - 70.0f, 70.0f, vCenterX - 5.0f, vBottomBorder), "discard_button", "Discard", new os::Message(M_PASSW_DISCARD), os::CF_FOLLOW_NONE);
    m_pcView->AddChild(m_pcDiscard);

    // set icon
    SetIcon(m_pcParent->m_apcBitmap[ExpanderWindow::FEBITMAP_PASSW24X24]);
}

// virtual method: HandleMessage
void ExpanderPassw::HandleMessage(os::Message *pcMessage)
{
    switch(pcMessage->GetCode())
    {
        case M_PASSW_USEPASSW:
        {
            bool nEnable = m_pcUsePassw->GetValue();
            m_pcPassw->SetEnable(nEnable);
            if (nEnable)
                SetFocusChild(m_pcPassw);
            break;
        }

        case M_PASSW_ACCEPT:
        {
            m_pcParent->Lock();
            m_pcParent->pcExpandStatus->SetString("");
            m_pcParent->m_pcPasswString = m_pcPassw->GetBuffer()[0];
            m_pcParent->m_nPasswEnable = m_pcUsePassw->GetValue();
            m_pcParent->IsNotFullyListed = true;
            if (m_pcParent->m_pcList->GetValue())
                m_pcParent->m_pcList->SetValue(false, true);
            m_pcParent->Unlock();
            PostMessage(os::M_QUIT);
            break;
        }

        case M_PASSW_DISCARD:
            PostMessage(os::M_QUIT);
            break;
            
        default:
            os::Window::HandleMessage(pcMessage);
            break;
    }
}

// ExpanderPassw destructor
ExpanderPassw::~ExpanderPassw()
{
    m_pcParent->m_pcPasswWind = NULL;
}

// ExpanderApp constructor
ExpanderApp::ExpanderApp(const char *pzParams)
  : Application("application/x-vnd.syllable-FileExpander")
{
    // variables
    struct stat stbuf;
    unsigned int j, st_size;
    int fd, dir_fd;
    char *tmpFileBuf, *maxFileBuf;
    float winpos[3] = { 0, 0, 0 };
    const unsigned int winpos_size = 3u * sizeof(float);
    const unsigned int min_st_size = winpos_size + sizeof(prefs_settings);
    const float yOffSet = 125.0f;

    m_pcWind = NULL;

    // opening file descriptor
    fd = open("/etc/FileExpander.rules", O_RDONLY);

    if (fstat(fd, &stbuf) < 0)
    {
        close(fd);
        os::Resources pcFEResources(get_image_id());
        os::ResStream *pcResStream = pcFEResources.GetResourceStream("error32x32.png");
        os::BitmapImage *pcBitmapImage = new os::BitmapImage(pcResStream);
        delete pcResStream;
        os::Alert *pcError = new os::Alert("Fatal", "FileExpander rules file not found!\nThis file may be damaged or lost.\nPlease reinstall program and try again. ", CopyBitmap(pcBitmapImage->LockBitmap()), os::WND_NO_CLOSE_BUT | os::WND_NO_ZOOM_BUT | os::WND_NO_DEPTH_BUT | os::WND_NOT_RESIZABLE, "Quit", NULL);
        delete pcBitmapImage;
        pcBitmapImage = new os::BitmapImage(pcFEResources.GetResourceStream("error24x24.png"));
        pcError->SetIcon(pcBitmapImage->LockBitmap());
        delete pcBitmapImage;
        pcError->CenterInScreen();
        pcError->Go();
        PostMessage(os::M_QUIT);
        return;
    }

    // creating and reading file buffer
    st_size = stbuf.st_size;
    FileBuf = new char[st_size + 1];
    read(fd, FileBuf, st_size);
    close(fd);
    maxFileBuf = FileBuf + st_size;
    *maxFileBuf = '\n';

    // creating text buffer
    pzTextBuffer = new char[TEXTBUF_MAXINDEX + 1];

    // creating hash-tables
    hash_table = new hash_struct*[HASH_FULL_SIZE];
    for (hash_ptr = hash_table + HASH_FULL_SIZE; hash_ptr != hash_table; hash_ptr--)
        *hash_ptr = NULL;

    tmpFileBuf = FileBuf;
    while (tmpFileBuf < maxFileBuf)
    {
        while (*tmpFileBuf == ' ') tmpFileBuf++;
        if (*tmpFileBuf == '\"')
        {
            rule = new char*[RULE_COUNT];
            for (j = 0; j < RULE_COUNT; j++)
            {
                while (*tmpFileBuf++ != '\"');
                rule[j] = tmpFileBuf;
                while (*tmpFileBuf != '\"') tmpFileBuf++;
                *tmpFileBuf = '\0';
            }
            for (j = 0; j < HASH_FULL_SIZE; j += HASH_SIZE)
            {
                while (*tmpFileBuf++ != '\"');
                elem = new hash_struct;
                elem->name = tmpFileBuf;
                elem->rule = rule;
                while (*tmpFileBuf != '\"') tmpFileBuf++;
                *tmpFileBuf = '\0';
                hash_ptr = hash_table + hash(elem->name) + j;
                elem->next = *hash_ptr;
                *hash_ptr = elem;
            }
        }
        while (*tmpFileBuf++ != '\n');
    }

    os::Desktop *pcDesk = new os::Desktop;
    os::Point deskPoint(pcDesk->GetResolution());
    delete pcDesk;

    // opening settings file descriptor
    dir_fd = open(getenv("HOME"), O_RDONLY);
    fd = based_open(dir_fd, EXPANDER_SETTINGS, O_RDONLY);
    close(dir_fd);

    // creating destination path buffer
    defDestPath = new char[PATH_MAX + 1];

    if (fstat(fd, &stbuf) < 0 || ((st_size = stbuf.st_size) < min_st_size))
    {
        std::cerr << "Settings file not found or incorrect" << std::endl;
        prefs_settings = 024;
        *defDestPath = '\0';
    }

    else
    {
        // load window position
        read(fd, winpos, winpos_size);

        // load prefs
        read(fd, &prefs_settings, sizeof(prefs_settings));

        // getting destination path
        st_size -= min_st_size;
        read(fd, defDestPath, st_size);
        defDestPath[st_size] = '\0';
    }

    // close file descriptor
    close(fd);

    // checking window position
    if ((winpos[0] >= winpos[1]) || (winpos[0] > deskPoint.x) || (winpos[2] > deskPoint.y))
    {
        std::cerr << "Window position information isn't correct!" << std::endl;
        winpos[0] = 20.0f; winpos[1] = 450.0f; winpos[2] = 100.0f;
    }

    // creating window
    m_pcWind = new ExpanderWindow(os::Rect(winpos[0], winpos[2], winpos[1], winpos[2] + yOffSet), pzParams);
    m_pcWind->SetSizeLimits(os::Point(430.0f, yOffSet), os::Point(os::COORD_MAX, yOffSet + EXPANDER_EXTRA));
    m_pcWind->Show();
    m_pcWind->MakeFocus();

    if (*pzParams)
    {
        const int autoexpand = prefs_settings & AUTOEXPAND, autolisting = prefs_settings & AUTOLISTING;

        if (autoexpand && autolisting)
            m_pcWind->m_cExpandList = true;
        else if (autoexpand) {
            os::Invoker *pcParentInvoker = new os::Invoker(new os::Message(ExpanderWindow::M_MENU_FILE_EXPAND), m_pcWind);
            pcParentInvoker->Invoke();
        }

        if (autolisting) {
            os::Invoker *pcParentInvoker = new os::Invoker(new os::Message(ExpanderWindow::M_MENU_FILE_LIST), m_pcWind);
            pcParentInvoker->Invoke();
        }
    }
}

// virtual method: OkToQuit
bool ExpanderApp::OkToQuit()
{
    // (if FileExpander.rules presents then object should be created)
    if (m_pcWind)
    {
        struct hash_struct *tmpelem;

        m_pcWind->Close();

        // removing objects
        for (hash_ptr = hash_table + HASH_FULL_SIZE; hash_ptr != hash_table; hash_ptr--)
        {
            elem = *hash_ptr;
            while (elem)
            {
                delete [] elem->rule;
                tmpelem = elem->next;
                delete elem;
                elem = tmpelem;
            }
        }

        delete [] hash_table;
        delete [] FileBuf;
        delete [] pzTextBuffer;
        delete [] defDestPath;
    }
    return true;
}

// ExpanderApp destructor
ExpanderApp::~ExpanderApp()
{
}

// Main function
int main(int argc, char *argv[])
{
    ExpanderApp *pcExpApp = new ExpanderApp(argc > 1 ? argv[1] : "");
    pcExpApp->Run();
    return 0;
}
