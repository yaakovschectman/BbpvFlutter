#include "flutter_window.h"

#include <iostream>
#include <optional>

#include "flutter/generated_plugin_registrant.h"

// WebView2 code

// Flutter code

FlutterWindow::FlutterWindow(const flutter::DartProject& project)
    : project_(project) {}

FlutterWindow::~FlutterWindow() {}

bool FlutterWindow::OnCreate() {
  if (!Win32Window::OnCreate()) {
    return false;
  }

  RECT frame = GetClientArea();

  // The size here must match the window dimensions to avoid unnecessary surface
  // creation / destruction in the startup path.
  flutter_controller_ = std::make_unique<flutter::FlutterViewController>(
      frame.right - frame.left, frame.bottom - frame.top, project_);
  // Ensure that basic setup of the controller was successful.
  if (!flutter_controller_->engine() || !flutter_controller_->view()) {
    return false;
  }
  RegisterPlugins(flutter_controller_->engine());
  SetChildContent(flutter_controller_->view()->GetNativeWindow());

  flutter_controller_->engine()->SetNextFrameCallback([&]() {
    this->Show();
  });

  // Flutter can complete the first frame before the "show window" callback is
  // registered. The following call ensures a frame is pending to ensure the
  // window is shown. It is a no-op if the first frame hasn't completed yet.
  flutter_controller_->ForceRedraw();

  return true;
}

void FlutterWindow::OnDestroy() {
  if (flutter_controller_) {
    flutter_controller_ = nullptr;
  }

  Win32Window::OnDestroy();
}

static bool init_button = false;

// Pointer to WebViewController
static wil::com_ptr<ICoreWebView2Controller> webviewController;

// Pointer to WebView window
static wil::com_ptr<ICoreWebView2> webview;

void MakeButton(HWND parent) {
    HWND group = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 400, 200, 200, parent, NULL, NULL, NULL);
    HWND button = CreateWindow(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE, 0, 0, 50, 25, group, NULL, NULL, NULL);

    HWND edit = CreateWindow(L"EDIT", L"Edit", WS_CHILD | WS_VISIBLE, 0, 100, 200, 50, group, NULL, NULL, NULL);
    (void)(edit);

    HWND hWndPar = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 50, 20, 800, 400, parent, NULL, NULL, NULL);
    HWND hWnd = CreateWindow(WEBVIEW_CLASSNAME, L"WebView", WS_CHILD | WS_VISIBLE, 0, 0, 800, 400, hWndPar, NULL, NULL, NULL);
	CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
		Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[hWnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {

				// Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
				env->CreateCoreWebView2Controller(hWnd, Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
					[hWnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
						if (controller != nullptr) {
							webviewController = controller;
							webviewController->get_CoreWebView2(&webview);
						}

						// Add a few settings for the webview
						// The demo step is redundant since the values are the default settings
						wil::com_ptr<ICoreWebView2Settings> settings;
						webview->get_Settings(&settings);
						settings->put_IsScriptEnabled(TRUE);
						settings->put_AreDefaultScriptDialogsEnabled(TRUE);
						settings->put_IsWebMessageEnabled(TRUE);

						// Resize WebView to fit the bounds of the parent window
						RECT bounds;
						GetClientRect(hWnd, &bounds);
						webviewController->put_Bounds(bounds);

						// Schedule an async task to navigate to Bing
						webview->Navigate(L"https://www.google.com/");

						// <NavigationEvents>
						// Step 4 - Navigation events
						// register an ICoreWebView2NavigationStartingEventHandler to cancel any non-https navigation
						EventRegistrationToken token;
						webview->add_NavigationStarting(Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
							[](ICoreWebView2* webview_, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
								wil::unique_cotaskmem_string uri;
								args->get_Uri(&uri);
								std::wstring source(uri.get());
								if (source.substr(0, 5) != L"https") {
									args->put_Cancel(true);
								}
								return S_OK;
							}).Get(), &token);
						// </NavigationEvents>

						// <Scripting>
						// Step 5 - Scripting
						// Schedule an async task to add initialization script that freezes the Object object
						// webview->AddScriptToExecuteOnDocumentCreated(L"Object.freeze(Object);", nullptr);
						// Schedule an async task to get the document URL
						webview->ExecuteScript(L"window.document.URL;", Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
							[](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
								LPCWSTR URL = resultObjectAsJson;
                (void)(URL);
								//doSomethingWithURL(URL);
								return S_OK;
							}).Get());
						// </Scripting>

						// <CommunicationHostWeb>
						// Step 6 - Communication between host and web content
						// Set an event handler for the host to return received message back to the web content
						webview->add_WebMessageReceived(Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
							[](ICoreWebView2* webview_, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
								wil::unique_cotaskmem_string message;
								args->TryGetWebMessageAsString(&message);
								// processMessage(&message);
								webview_->PostWebMessageAsString(message.get());
								return S_OK;
							}).Get(), &token);

						// Schedule an async task to add initialization script that
						// 1) Add an listener to print message from the host
						// 2) Post document URL to the host
						/*webview->AddScriptToExecuteOnDocumentCreated(
							L"window.chrome.webview.addEventListener(\'message\', event => alert(event.data));" \
							L"window.chrome.webview.postMessage(window.document.URL);",
							nullptr);*/
						// </CommunicationHostWeb>

						return S_OK;
					}).Get());
				return S_OK;
			}).Get());

    LONG style = GetWindowLong(parent, GWL_STYLE);
    style |= WS_CLIPCHILDREN;
    SetWindowLong(parent, GWL_STYLE, style);
    if (button) init_button = true;
}

LRESULT
FlutterWindow::MessageHandler(HWND hwnd, UINT const message,
                              WPARAM const wparam,
                              LPARAM const lparam) noexcept {
  // Give Flutter, including plugins, an opportunity to handle window messages.
  if (flutter_controller_) {
    std::optional<LRESULT> result =
        flutter_controller_->HandleTopLevelWindowProc(hwnd, message, wparam,
                                                      lparam);
    if (result) {
      return *result;
    }
  }

  switch (message) {
    case WM_FONTCHANGE:
      flutter_controller_->engine()->ReloadSystemFonts();
      break;
    case WM_MOVE: {
      if (init_button) break;
      HWND flutter_view = flutter_controller_->view()->GetNativeWindow();
      if (!flutter_view) break;
      MakeButton(flutter_view);
      break;
    }
  }

  return Win32Window::MessageHandler(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK WebviewWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_SIZE:
      if (webviewController != nullptr) {
        RECT bounds;
        GetClientRect(hwnd, &bounds);
        webviewController->put_Bounds(bounds);
      }
      break;
    default:
      return DefWindowProc(hwnd, msg, wparam, lparam);
  }
  return 0;
}
