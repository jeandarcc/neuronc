#include "InstallerUI.h"
#include <atomic>
#include <commctrl.h>
#include <iostream>
#include <shlobj.h>
#include <thread>
#include <windows.h>


#pragma comment(lib, "comctl32.lib")

namespace neuron {
namespace installer {

class Win32InstallerUI : public InstallerUI {
private:
  HWND hwnd = NULL;
  HFONT hFontTitle = NULL;
  HFONT hFontBody = NULL;

  // Controls
  HWND hTitle = NULL;
  HWND hSubtitle = NULL;
  HWND hBtnNext = NULL;
  HWND hBtnBack = NULL;
  HWND hBtnCancel = NULL;

  // Directory step
  HWND hDirEdit = NULL;
  HWND hBtnBrowse = NULL;

  // Progress step
  HWND hProgressBar = NULL;
  HWND hProgressText = NULL;

  // Complete step
  HWND hCheckLaunch = NULL;

  DialogResult currentResult = DialogResult::Cancel;
  bool actionTaken = false;
  bool isProgressing = false;

  std::string *outDirStr = nullptr;
  bool *outLaunchApp = nullptr;

  const int ID_NEXT = 101;
  const int ID_BACK = 102;
  const int ID_CANCEL = 103;
  const int ID_BROWSE = 104;

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                  LPARAM lParam) {
    Win32InstallerUI *ui = nullptr;
    if (msg == WM_NCCREATE) {
      CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
      ui = reinterpret_cast<Win32InstallerUI *>(cs->lpCreateParams);
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ui));
    } else {
      ui = reinterpret_cast<Win32InstallerUI *>(
          GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (ui) {
      return ui->handleMessage(msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }

  LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
      if (isProgressing)
        return 0; // Disable buttons during install

      int id = LOWORD(wParam);
      if (id == ID_NEXT) {
        if (outDirStr && hDirEdit) {
          char buf[MAX_PATH];
          GetWindowTextA(hDirEdit, buf, MAX_PATH);
          *outDirStr = buf;
        }
        if (outLaunchApp && hCheckLaunch) {
          *outLaunchApp =
              (SendMessage(hCheckLaunch, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        currentResult = DialogResult::Next;
        actionTaken = true;
      } else if (id == ID_BACK) {
        currentResult = DialogResult::Back;
        actionTaken = true;
      } else if (id == ID_CANCEL) {
        currentResult = DialogResult::Cancel;
        actionTaken = true;
      } else if (id == ID_BROWSE) {
        BROWSEINFOA bi = {0};
        bi.hwndOwner = hwnd;
        bi.lpszTitle = "Select Installation Directory";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
        if (pidl != 0) {
          char path[MAX_PATH];
          if (SHGetPathFromIDListA(pidl, path)) {
            SetWindowTextA(hDirEdit, path);
          }
          CoTaskMemFree(pidl);
        }
      }
      return 0;
    }
    case WM_CTLCOLORSTATIC: {
      HDC hdcStatic = (HDC)wParam;
      SetBkColor(hdcStatic, RGB(40, 40, 40));
      SetTextColor(hdcStatic, RGB(255, 255, 255));
      return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_USER + 1: { // Update progress text
      char *text = reinterpret_cast<char *>(lParam);
      SetWindowTextA(hProgressText, text);
      delete[] text;
      return 0;
    }
    case WM_USER + 2: { // Update progress bar
      int percent = static_cast<int>(wParam);
      SendMessage(hProgressBar, PBM_SETPOS, percent, 0);
      return 0;
    }
    case WM_USER + 3: { // Install done
      isProgressing = false;
      currentResult = DialogResult::Next;
      actionTaken = true;
      return 0;
    }
    case WM_CLOSE:
      if (!isProgressing) {
        currentResult = DialogResult::Cancel;
        actionTaken = true;
      }
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }

  void hideAll() {
    ShowWindow(hDirEdit, SW_HIDE);
    ShowWindow(hBtnBrowse, SW_HIDE);
    ShowWindow(hProgressBar, SW_HIDE);
    ShowWindow(hProgressText, SW_HIDE);
    ShowWindow(hCheckLaunch, SW_HIDE);
    EnableWindow(hBtnNext, TRUE);
    EnableWindow(hBtnBack, TRUE);
    EnableWindow(hBtnCancel, TRUE);
  }

  DialogResult pump() {
    actionTaken = false;
    MSG msg;
    while (!actionTaken && GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    return currentResult;
  }

public:
  Win32InstallerUI() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);
    CoInitialize(NULL);
  }

  ~Win32InstallerUI() override {
    if (hFontTitle)
      DeleteObject(hFontTitle);
    if (hFontBody)
      DeleteObject(hFontBody);
    if (hwnd)
      DestroyWindow(hwnd);
    CoUninitialize();
  }

  bool initialize(const InstallManifest &manifest) override {
    const char *className = "NeuronInstallerClass";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = className;
    wc.hbrBackground = CreateSolidBrush(RGB(40, 40, 40)); // Dark theme
    RegisterClassA(&wc);

    std::string winTitle = manifest.productName + " Setup";
    hwnd = CreateWindowExA(
        0, className, winTitle.c_str(),
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, CW_USEDEFAULT,
        CW_USEDEFAULT, 600, 400, NULL, NULL, GetModuleHandle(NULL), this);

    if (!hwnd)
      return false;

    hFontTitle =
        CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    hFontBody =
        CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    // Static texts
    hTitle = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 20, 20, 500, 30,
                           hwnd, NULL, NULL, NULL);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

    hSubtitle = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE, 20, 60, 540,
                              60, hwnd, NULL, NULL, NULL);
    SendMessage(hSubtitle, WM_SETFONT, (WPARAM)hFontBody, TRUE);

    // Buttons
    hBtnBack = CreateWindowA("BUTTON", "< Back", WS_CHILD | WS_VISIBLE, 300,
                             320, 80, 25, hwnd, (HMENU)ID_BACK, NULL, NULL);
    hBtnNext = CreateWindowA("BUTTON", "Next >", WS_CHILD | WS_VISIBLE, 390,
                             320, 80, 25, hwnd, (HMENU)ID_NEXT, NULL, NULL);
    hBtnCancel = CreateWindowA("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE, 480,
                               320, 80, 25, hwnd, (HMENU)ID_CANCEL, NULL, NULL);

    SendMessage(hBtnBack, WM_SETFONT, (WPARAM)hFontBody, TRUE);
    SendMessage(hBtnNext, WM_SETFONT, (WPARAM)hFontBody, TRUE);
    SendMessage(hBtnCancel, WM_SETFONT, (WPARAM)hFontBody, TRUE);

    // Directory controls
    hDirEdit =
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | ES_AUTOHSCROLL,
                        20, 140, 440, 25, hwnd, NULL, NULL, NULL);
    SendMessage(hDirEdit, WM_SETFONT, (WPARAM)hFontBody, TRUE);
    hBtnBrowse = CreateWindowA("BUTTON", "Browse...", WS_CHILD, 470, 140, 80,
                               25, hwnd, (HMENU)ID_BROWSE, NULL, NULL);
    SendMessage(hBtnBrowse, WM_SETFONT, (WPARAM)hFontBody, TRUE);

    // Progress controls
    hProgressBar =
        CreateWindowExA(0, PROGRESS_CLASS, NULL, WS_CHILD | PBS_SMOOTH, 20, 150,
                        540, 20, hwnd, NULL, NULL, NULL);
    hProgressText =
        CreateWindowA("STATIC", "Preparing...", WS_CHILD | SS_LEFTNOWORDWRAP,
                      20, 130, 540, 20, hwnd, NULL, NULL, NULL);
    SendMessage(hProgressText, WM_SETFONT, (WPARAM)hFontBody, TRUE);

    // Complete controls
    hCheckLaunch = CreateWindowA("BUTTON", "Launch Application",
                                 WS_CHILD | BS_AUTOCHECKBOX, 20, 140, 200, 20,
                                 hwnd, NULL, NULL, NULL);
    SendMessage(hCheckLaunch, WM_SETFONT, (WPARAM)hFontBody, TRUE);
    SendMessage(hCheckLaunch, BM_SETCHECK, BST_CHECKED, 0);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return true;
  }

  DialogResult showWelcome(const InstallManifest &manifest) override {
    hideAll();
    outDirStr = nullptr;
    outLaunchApp = nullptr;

    SetWindowTextA(hTitle,
                   ("Welcome to " + manifest.productName + " Setup").c_str());
    std::string desc =
        "This wizard will guide you through the installation of " +
        manifest.productName + " " + manifest.productVersion +
        ".\n\nPublisher: " + manifest.publisher;
    desc += "\n\nClick Next to continue, or Cancel to exit.";
    SetWindowTextA(hSubtitle, desc.c_str());

    EnableWindow(hBtnBack, FALSE);
    SetWindowTextA(hBtnNext, "Next >");

    return pump();
  }

  DialogResult showDirectorySelect(const InstallManifest &manifest,
                                   std::string &installDir) override {
    hideAll();
    outDirStr = &installDir;
    outLaunchApp = nullptr;

    SetWindowTextA(hTitle, "Choose Install Location");
    SetWindowTextA(hSubtitle,
                   "Choose the folder in which to install the application.");

    SetWindowTextA(hDirEdit, installDir.c_str());
    ShowWindow(hDirEdit, SW_SHOW);
    ShowWindow(hBtnBrowse, SW_SHOW);

    SetWindowTextA(hBtnNext, "Install");

    return pump();
  }

  DialogResult
  showProgress(const InstallManifest &manifest, const std::string &installDir,
               std::function<bool(InstallerUI *)> installStep) override {
    hideAll();
    outDirStr = nullptr;
    outLaunchApp = nullptr;
    isProgressing = true;

    SetWindowTextA(hTitle, "Installing...");
    std::string desc =
        "Please wait while " + manifest.productName + " is being installed.";
    SetWindowTextA(hSubtitle, desc.c_str());

    ShowWindow(hProgressBar, SW_SHOW);
    ShowWindow(hProgressText, SW_SHOW);

    EnableWindow(hBtnNext, FALSE);
    EnableWindow(hBtnBack, FALSE);

    // Run extraction logic in a background thread
    std::thread([this, installStep]() {
      bool success = installStep(this);
      if (success) {
        PostMessage(hwnd, WM_USER + 3, 0, 0); // Done signal
      } else {
        // If it failed, we just leave it hanging or close. Error message is
        // shown via showError.
        PostMessage(hwnd, WM_CLOSE, 0, 0);
      }
    }).detach();

    return pump();
  }

  void setProgressText(const std::string &text) override {
    // Must copy text for async cross-thread delivery
    char *buf = new char[text.length() + 1];
    strcpy(buf, text.c_str());
    PostMessage(hwnd, WM_USER + 1, 0, reinterpret_cast<LPARAM>(buf));
  }

  void setProgressPercent(int percent) override {
    PostMessage(hwnd, WM_USER + 2, static_cast<WPARAM>(percent), 0);
  }

  DialogResult showComplete(const InstallManifest &manifest,
                            bool &outLaunchAppRef) override {
    hideAll();
    outDirStr = nullptr;
    outLaunchApp = &outLaunchAppRef;

    SetWindowTextA(hTitle, "Installation Complete");
    SetWindowTextA(
        hSubtitle,
        "Setup has finished installing the application on your computer.");

    ShowWindow(hCheckLaunch, SW_SHOW);

    SetWindowTextA(hBtnNext, "Finish");
    EnableWindow(hBtnBack, FALSE);
    EnableWindow(hBtnCancel, FALSE); // Already done

    return pump();
  }

  void showError(const std::string &title,
                 const std::string &message) override {
    MessageBoxA(hwnd, message.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
  }
};

InstallerUI *createInstallerUI() { return new Win32InstallerUI(); }

} // namespace installer
} // namespace neuron
