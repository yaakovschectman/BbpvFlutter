#ifndef RUNNER_FLUTTER_WINDOW_H_
#define RUNNER_FLUTTER_WINDOW_H_

#include <flutter/dart_project.h>
#include <flutter/flutter_view_controller.h>

#include <memory>

#include "win32_window.h"

// WebView2 code

#include "windows.h"
#include "wrl.h"
#include "wil/com.h"
#include "WebView2.h"

#define WEBVIEW_CLASSNAME L"webview_control"

// Flutter code

// A window that does nothing but host a Flutter view.
class FlutterWindow : public Win32Window {
 public:
  // Creates a new FlutterWindow hosting a Flutter view running |project|.
  explicit FlutterWindow(const flutter::DartProject& project);
  virtual ~FlutterWindow();

  HWND GetWndParent();

 protected:
  // Win32Window:
  bool OnCreate() override;
  void OnDestroy() override;
  LRESULT MessageHandler(HWND window, UINT const message, WPARAM const wparam,
                         LPARAM const lparam) noexcept override;

 private:
  // The project to run.
  flutter::DartProject project_;

  // The Flutter instance hosted by this window.
  std::unique_ptr<flutter::FlutterViewController> flutter_controller_;
};

LRESULT CALLBACK WebviewWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

#endif  // RUNNER_FLUTTER_WINDOW_H_
