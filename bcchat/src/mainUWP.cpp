#include <ppltasks.h>
#include <d3d11_3.h>
#include <dxgi1_4.h>
#include <wrl.h>

#include <iostream>
#include <cmath>

#include "app.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "globals.h"

using namespace concurrency;
using namespace Microsoft::WRL;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Gaming::Input;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::ViewManagement;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;

namespace DX
{
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            // Set a breakpoint on this line to catch Win32 API errors.
            throw Platform::Exception::CreateException(hr);
        }
    }
}

// Code allowing log to be printed in the output window
class debugStreambuf : public std::streambuf
{
public:
    virtual int_type overflow(int_type c = EOF)
    {
        if (c != EOF) {
            if (buffi == 8191)
            {
                buf[buffi] = 0;
                OutputDebugString(buf);
                buffi = 0;
            }
            buf[buffi++] = c;
            return c;
        }
        buf[buffi] = 0;
        OutputDebugString(buf);
        buffi = 0;
        return c;
    }

    TCHAR buf[8192];
    int buffi = 0;
};

class Cout2VisualStudioDebugOutput
{
    debugStreambuf dbgstream;
    std::streambuf *default_stream;

public:
    Cout2VisualStudioDebugOutput()
    {
        default_stream = std::cout.rdbuf(&dbgstream);
    }

    ~Cout2VisualStudioDebugOutput()
    {
        std::cout.rdbuf(default_stream);
    }
};

Cout2VisualStudioDebugOutput cout2VisualStudioDebugOutput;

inline float ConvertDipsToPixels(float dips, float dpi)
{
    static const float dipsPerInch = 96.0f;
    return floorf(dips * dpi / dipsPerInch + 0.5f); // Round to nearest integer.
}

// Application
namespace BCChat
{
    // Main entry point for our app. Connects the app with the Windows shell and handles application lifecycle events.
    ref class App sealed : public Windows::ApplicationModel::Core::IFrameworkView
    {
    public:
        App() :
            m_windowClosed(false),
            m_windowVisible(true),
            m_scroll(0.0f),
            m_showKeyboard(false),
            m_screenViewport(),
            m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
            m_d3dRenderTargetSize(),
            m_size(),
            m_nativeOrientation(DisplayOrientations::None),
            m_currentOrientation(DisplayOrientations::None),
            m_dpi(-1.0f)
        {
            memset(m_keyUps, 0, sizeof(m_keyUps));
        }

        // The first method called when the IFrameworkView is being created.
        virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView)
        {
            // Register event handlers for app lifecycle. This example includes Activated, so that we
            // can make the CoreWindow active and start rendering on the window.
            applicationView->Activated +=
                ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

            CoreApplication::Suspending +=
                ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);

            CoreApplication::Resuming +=
                ref new EventHandler<Platform::Object^>(this, &App::OnResuming);

            // At this point we have access to the device. 
            // We can create the device-dependent resources.

            // This flag adds support for surfaces with a different color channel ordering
            // than the API default. It is required for compatibility with Direct2D.
            UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

            // This array defines the set of DirectX hardware feature levels this app will support.
            // Note the ordering should be preserved.
            // Don't forget to declare your application's minimum required feature level in its
            // description.  All applications are assumed to support 9.1 unless otherwise stated.
            D3D_FEATURE_LEVEL featureLevels[] =
            {
                D3D_FEATURE_LEVEL_12_1,
                D3D_FEATURE_LEVEL_12_0,
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0,
                D3D_FEATURE_LEVEL_9_3,
                D3D_FEATURE_LEVEL_9_2,
                D3D_FEATURE_LEVEL_9_1
            };

            // Create the Direct3D 11 API device object and a corresponding context.
            ComPtr<ID3D11Device> device;
            ComPtr<ID3D11DeviceContext> context;

            HRESULT hr = D3D11CreateDevice(
                nullptr,					// Specify nullptr to use the default adapter.
                D3D_DRIVER_TYPE_HARDWARE,	// Create a device using the hardware graphics driver.
                0,							// Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
                creationFlags,				// Set debug and Direct2D compatibility flags.
                featureLevels,				// List of feature levels this app can support.
                ARRAYSIZE(featureLevels),	// Size of the list above.
                D3D11_SDK_VERSION,			// Always set this to D3D11_SDK_VERSION for Windows Store apps.
                &device,					// Returns the Direct3D device created.
                &m_d3dFeatureLevel,			// Returns feature level of device created.
                &context					// Returns the device immediate context.
            );

            if (FAILED(hr))
            {
                // If the initialization fails, fall back to the WARP device.
                // For more information on WARP, see: 
                // https://go.microsoft.com/fwlink/?LinkId=286690
                DX::ThrowIfFailed(
                    D3D11CreateDevice(
                        nullptr,
                        D3D_DRIVER_TYPE_WARP, // Create a WARP device instead of a hardware device.
                        0,
                        creationFlags,
                        featureLevels,
                        ARRAYSIZE(featureLevels),
                        D3D11_SDK_VERSION,
                        &device,
                        &m_d3dFeatureLevel,
                        &context
                    )
                );
            }

            // Store pointers to the Direct3D 11.3 API device and immediate context.
            DX::ThrowIfFailed(
                device.As(&m_d3dDevice)
            );

            DX::ThrowIfFailed(
                context.As(&m_d3dContext)
            );

            // Setup Dear ImGui binding
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.KeyMap[ImGuiKey_Tab] = VK_TAB;
            io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
            io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
            io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
            io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
            io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
            io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
            io.KeyMap[ImGuiKey_Home] = VK_HOME;
            io.KeyMap[ImGuiKey_End] = VK_END;
            io.KeyMap[ImGuiKey_Insert] = VK_INSERT;
            io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
            io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
            io.KeyMap[ImGuiKey_Space] = VK_SPACE;
            io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
            io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
            io.KeyMap[ImGuiKey_A] = 'A';
            io.KeyMap[ImGuiKey_C] = 'C';
            io.KeyMap[ImGuiKey_V] = 'V';
            io.KeyMap[ImGuiKey_X] = 'X';
            io.KeyMap[ImGuiKey_Y] = 'Y';
            io.KeyMap[ImGuiKey_Z] = 'Z';
            io.MouseDrawCursor = true;
            ImGui_ImplDX11_Init(m_d3dDevice.Get(), m_d3dContext.Get());
            ImGui::StyleColorsDark();
        }

        // Called when the CoreWindow object is created (or re-created).
        virtual void SetWindow(Windows::UI::Core::CoreWindow^ window)
        {
            window->SizeChanged += ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &App::OnWindowSizeChanged);
            window->VisibilityChanged += ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &App::OnVisibilityChanged);
            window->Closed += ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &App::OnWindowClosed);
            window->PointerMoved += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerMoved);
            window->PointerPressed += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerPressed);
            window->PointerReleased += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerReleased);
            window->PointerWheelChanged += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &App::OnPointerWheelChanged);
            window->CharacterReceived += ref new TypedEventHandler<CoreWindow^, CharacterReceivedEventArgs^>(this, &App::OnCharacterReceived);
            window->KeyDown += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &App::OnKeyDown);
            window->KeyUp += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &App::OnKeyUp);
            InputPane::GetForCurrentView()->Showing += ref new TypedEventHandler<InputPane^, InputPaneVisibilityEventArgs^>(this, &App::OnKeyboardShowing);
            InputPane::GetForCurrentView()->Hiding += ref new TypedEventHandler<InputPane^, InputPaneVisibilityEventArgs^>(this, &App::OnKeyboardHiding);

            DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

            currentDisplayInformation->DpiChanged +=
                ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDpiChanged);

            currentDisplayInformation->OrientationChanged +=
                ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnOrientationChanged);

            DisplayInformation::DisplayContentsInvalidated +=
                ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDisplayContentsInvalidated);

            m_window = window;
            m_size = Windows::Foundation::Size(window->Bounds.Width, window->Bounds.Height);
            m_nativeOrientation = currentDisplayInformation->NativeOrientation;
            m_currentOrientation = currentDisplayInformation->CurrentOrientation;
            m_dpi = currentDisplayInformation->LogicalDpi;
            m_size.Width = ConvertDipsToPixels(m_size.Width, m_dpi);
            m_size.Height = ConvertDipsToPixels(m_size.Height, m_dpi);
            width = m_size.Width;
            height = m_size.Height;

            ImGuiIO& io = ImGui::GetIO();
            io.MousePos.x = (float)(m_size.Width / 2);
            io.MousePos.y = (float)(m_size.Height / 2);

            CreateWindowSizeDependentResources();
        }

        // Initializes scene resources, or loads a previously saved app state.
        virtual void Load(Platform::String^ entryPoint)
        {
        }

        // This method is called after the window becomes active.
        virtual void Run()
        {
            ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
            auto lastTime = std::chrono::steady_clock::now();

            while (!m_windowClosed)
            {
                auto curTime = std::chrono::steady_clock::now();

                if (m_windowVisible)
                {
                    CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

                    // Setup style
                    switch (theme)
                    {
                        case 0: ImGui::StyleColorsClassic(); break;
                        case 1: ImGui::StyleColorsDark(); break;
                        case 2: ImGui::StyleColorsLight(); break;
                    }
                    ImGui_ImplDX11_NewFrame();

                    ImGuiIO& io = ImGui::GetIO();

                    if (io.WantTextInput)
                    {
                        // Show virtual keyboard
                        m_showKeyboard |= InputPane::GetForCurrentView()->TryShow();
                    }
                    else if (m_showKeyboard)
                    {
                        InputPane::GetForCurrentView()->TryHide();
                    }
                    if (m_showKeyboard)
                    {
                        height = m_size.Height - ConvertDipsToPixels(InputPane::GetForCurrentView()->OccludedRect.Height, m_dpi);
                    }
                    else
                    {
                        height = m_size.Height;
                    }

                    // Setup display size (every frame to accommodate for window resizing)
                    io.DisplaySize = ImVec2((float)(m_size.Width), (float)(m_size.Height));

                    // Setup time step
                    io.DeltaTime = (float)((double)std::chrono::duration_cast<std::chrono::microseconds>(curTime - lastTime).count() / 1000000.0);

                    // Update gamepad mouse movement
                    if (Gamepad::Gamepads->Size > 0 && !m_showKeyboard)
                    {
                        auto pGamePad = Gamepad::Gamepads->GetAt(0);
                        auto reading = pGamePad->GetCurrentReading();

                        float leftStickX = (float)reading.LeftThumbstickX;   // returns a value between -1.0 and +1.0
                        float leftStickY = (float)reading.LeftThumbstickY;   // returns a value between -1.0 and +1.0

                        const float deadzoneRadius = 0.1f;             // choose a deadzone -- readings inside this radius are ignored.
                        const float deadzoneSquared = deadzoneRadius * deadzoneRadius;

                        // Pythagorean theorem -- for a right triangle, hypotenuse^2 = (opposite side)^2 + (adjacent side)^2
                        auto oppositeSquared = leftStickY * leftStickY;
                        auto adjacentSquared = leftStickX * leftStickX;

                        // accept and process input if true; otherwise, reject and ignore it.
                        if ((oppositeSquared + adjacentSquared) > deadzoneSquared)
                        {
                            // input accepted, process it
                            auto len = std::sqrtf(oppositeSquared + adjacentSquared);
                            leftStickX /= len;
                            leftStickY /= len;
                            len -= deadzoneRadius;
                            leftStickX *= len;
                            leftStickY *= len;
                            io.MousePos.x += (float)leftStickX * io.DisplaySize.x * .6f * io.DeltaTime;
                            io.MousePos.y -= (float)leftStickY * io.DisplaySize.y * .6f * io.DeltaTime;
                            io.MousePos.x = std::max(0.0f, std::min(io.DisplaySize.x, io.MousePos.x));
                            io.MousePos.y = std::max(0.0f, std::min(io.DisplaySize.y, io.MousePos.y));
                        }

                        io.MouseDown[0] = ((int)reading.Buttons & (int)GamepadButtons::A) ? true : false;

                        // Wheel
                        auto scrollStick = (float)reading.RightThumbstickY;
                        if (scrollStick > deadzoneRadius)
                        {
                            m_scroll += scrollStick * 10.0f * io.DeltaTime;
                            while (m_scroll >= 1.0f)
                            {
                                m_scroll -= 1.0f;
                                io.MouseWheel += 1.0f;
                            }
                        }
                        else if (scrollStick < -deadzoneRadius)
                        {
                            m_scroll += scrollStick * 10.0f * io.DeltaTime;
                            while (m_scroll <= -1.0f)
                            {
                                m_scroll += 1.0f;
                                io.MouseWheel -= 1.0f;
                            }
                        }
                        else
                        {
                            m_scroll = 0.0f;
                        }
                    }

                    ImGui::NewFrame();
                    //ImGui::ShowDemoWindow();
                    app_update();
                    ImGui::Render();

                    auto pRenderTargetView = m_d3dRenderTargetView.Get();
                    ID3D11RenderTargetView *const targets[1] = { pRenderTargetView };
                    m_d3dContext->OMSetRenderTargets(1, targets, NULL);
                    m_d3dContext->ClearRenderTargetView(pRenderTargetView, (float*)&clear_color);
                    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                    present();

                    for (int i = 0; i < 512; ++i)
                    {
                        if (m_keyUps[i])
                        {
                            io.KeysDown[i] = false;
                        }
                    }
                    memset(m_keyUps, 0, sizeof(m_keyUps));
                }
                else
                {
                    CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
                }

                lastTime = curTime;
            }
        }

        // Required for IFrameworkView.
        // Terminate events do not cause Uninitialize to be called. It will be called if your IFrameworkView
        // class is torn down while the app is in the foreground.
        virtual void Uninitialize()
        {
        }

    protected:
        // Application lifecycle event handlers.
        void OnActivated(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args)
        {
            // Run() won't start until the CoreWindow is activated.
            CoreWindow::GetForCurrentThread()->Activate();
        }

        void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ args)
        {
            // Save app state asynchronously after requesting a deferral. Holding a deferral
            // indicates that the application is busy performing suspending operations. Be
            // aware that a deferral may not be held indefinitely. After about five seconds,
            // the app will be forced to exit.
            SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();

            create_task([this, deferral]()
            {
                // Call this method when the app suspends. It provides a hint to the driver that the app 
                // is entering an idle state and that temporary buffers can be reclaimed for use by other apps.
                ComPtr<IDXGIDevice3> dxgiDevice;
                m_d3dDevice.As(&dxgiDevice);
                dxgiDevice->Trim();

                deferral->Complete();
            });
        }

        void OnResuming(Platform::Object^ sender, Platform::Object^ args)
        {
            // Restore any data or state that was unloaded on suspend. By default, data
            // and state are persisted when resuming from suspend. Note that this event
            // does not occur if the app was previously terminated.

            // Insert your code here.
        }

        // Window event handlers.
        void OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args)
        {
            auto logicalSize = Size(sender->Bounds.Width, sender->Bounds.Height);
            if (m_size != logicalSize)
            {
                m_size = logicalSize;
                m_size.Width = ConvertDipsToPixels(m_size.Width, m_dpi);
                m_size.Height = ConvertDipsToPixels(m_size.Height, m_dpi);
                CreateWindowSizeDependentResources();
            }
            //m_main->CreateWindowSizeDependentResources();
        }

        void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args)
        {
            m_windowVisible = args->Visible;
        }

        void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args)
        {
            m_windowClosed = true;
        }

        void OnPointerMoved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
        {
            ImGuiIO& io = ImGui::GetIO();
            auto pointerPosition = m_window->PointerPosition;
            pointerPosition.X -= m_window->Bounds.X;
            pointerPosition.Y -= m_window->Bounds.Y;
            io.MouseDrawCursor = false;
            io.MousePos = ImVec2((float)pointerPosition.X, (float)pointerPosition.Y);

        }

        void OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDown[0] = true;
        }

        void OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDown[0] = false;
        }

        void OnPointerWheelChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseWheel += args->CurrentPoint->Properties->MouseWheelDelta / (float)WHEEL_DELTA;
        }

        void OnCharacterReceived(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CharacterReceivedEventArgs^ args)
        {
            ImGuiIO& io = ImGui::GetIO();
            auto key = (ImWchar)args->KeyCode;
            io.AddInputCharacter(key);
        }

        void OnKeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args)
        {
            ImGuiIO& io = ImGui::GetIO();
            auto vkey = (int)args->VirtualKey;
            if (vkey < 256)
                io.KeysDown[vkey] = true;
        }

        void OnKeyUp(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args)
        {
            auto vkey = (int)args->VirtualKey;
            if (vkey < 256)
                m_keyUps[vkey] = true;
        }

        void OnKeyboardShowing(InputPane^ sender, InputPaneVisibilityEventArgs^ args)
        {
            m_showKeyboard = true;
        }

        void OnKeyboardHiding(InputPane^ sender, InputPaneVisibilityEventArgs^ args)
        {
            m_showKeyboard = false;
            ImGui::ClearActiveID();
        }

        // DisplayInformation event handlers.
        void OnDpiChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args)
        {
        }

        void OnOrientationChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args)
        {
            //m_deviceResources->SetCurrentOrientation(sender->CurrentOrientation);
            //m_main->CreateWindowSizeDependentResources();
        }

        void OnDisplayContentsInvalidated(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args)
        {
            //m_deviceResources->ValidateDevice();
        }

    private:
        void present()
        {
            // The first argument instructs DXGI to block until VSync, putting the application
            // to sleep until the next VSync. This ensures we don't waste any cycles rendering
            // frames that will never be displayed to the screen.
            DXGI_PRESENT_PARAMETERS parameters = { 0 };
            HRESULT hr = m_swapChain->Present1(1, 0, &parameters);

            // Discard the contents of the render target.
            // This is a valid operation only when the existing contents will be entirely
            // overwritten. If dirty or scroll rects are used, this call should be removed.
            m_d3dContext->DiscardView1(m_d3dRenderTargetView.Get(), nullptr, 0);

            // Discard the contents of the depth stencil.
            m_d3dContext->DiscardView1(m_d3dDepthStencilView.Get(), nullptr, 0);

            // If the device was removed either by a disconnection or a driver upgrade, we 
            // must recreate all device resources.
            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
            {
                HandleDeviceLost();
            }
            else
            {
                DX::ThrowIfFailed(hr);
            }
        }

        void HandleDeviceLost()
        {
        }

        // These resources need to be recreated every time the window size is changed.
        void CreateWindowSizeDependentResources()
        {
            // Clear the previous window size specific context.
            ID3D11RenderTargetView* nullViews[] = { nullptr };
            m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
            m_d3dRenderTargetView = nullptr;
            m_d3dDepthStencilView = nullptr;
            m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

            m_d3dRenderTargetSize.Width = m_size.Width;
            m_d3dRenderTargetSize.Height = m_size.Height;

            if (m_swapChain != nullptr)
            {
                // If the swap chain already exists, resize it.
                HRESULT hr = m_swapChain->ResizeBuffers(
                    2, // Double-buffered swap chain.
                    lround(m_d3dRenderTargetSize.Width),
                    lround(m_d3dRenderTargetSize.Height),
                    DXGI_FORMAT_B8G8R8A8_UNORM,
                    0
                );

                if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
                {
                    // If the device was removed for any reason, a new device and swap chain will need to be created.
                    HandleDeviceLost();

                    // Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method 
                    // and correctly set up the new device.
                    return;
                }
                else
                {
                    DX::ThrowIfFailed(hr);
                }
            }
            else
            {
                // Otherwise, create a new one using the same adapter as the existing Direct3D device.
                DXGI_SCALING scaling = DXGI_SCALING_NONE;
                DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };

                swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// Match the size of the window.
                swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
                swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// This is the most common swap chain format.
                swapChainDesc.Stereo = false;
                swapChainDesc.SampleDesc.Count = 1;								// Don't use multi-sampling.
                swapChainDesc.SampleDesc.Quality = 0;
                swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                swapChainDesc.BufferCount = 2;									// Use double-buffering to minimize latency.
                swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// All Windows Store apps must use this SwapEffect.
                swapChainDesc.Flags = 0;
                swapChainDesc.Scaling = scaling;
                swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

                // This sequence obtains the DXGI factory that was used to create the Direct3D device above.
                ComPtr<IDXGIDevice3> dxgiDevice;
                DX::ThrowIfFailed(
                    m_d3dDevice.As(&dxgiDevice)
                );

                ComPtr<IDXGIAdapter> dxgiAdapter;
                DX::ThrowIfFailed(
                    dxgiDevice->GetAdapter(&dxgiAdapter)
                );

                ComPtr<IDXGIFactory4> dxgiFactory;
                DX::ThrowIfFailed(
                    dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
                );

                ComPtr<IDXGISwapChain1> swapChain;
                DX::ThrowIfFailed(
                    dxgiFactory->CreateSwapChainForCoreWindow(
                        m_d3dDevice.Get(),
                        reinterpret_cast<IUnknown*>(m_window.Get()),
                        &swapChainDesc,
                        nullptr,
                        &swapChain
                    )
                );
                DX::ThrowIfFailed(
                    swapChain.As(&m_swapChain)
                );

                // Ensure that DXGI does not queue more than one frame at a time. This both reduces latency and
                // ensures that the application will only render after each VSync, minimizing power consumption.
                DX::ThrowIfFailed(
                    dxgiDevice->SetMaximumFrameLatency(1)
                );
            }

            // Create a render target view of the swap chain back buffer.
            ComPtr<ID3D11Texture2D1> backBuffer;
            DX::ThrowIfFailed(
                m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
            );

            DX::ThrowIfFailed(
                m_d3dDevice->CreateRenderTargetView1(
                    backBuffer.Get(),
                    nullptr,
                    &m_d3dRenderTargetView
                )
            );

            // Create a depth stencil view for use with 3D rendering if needed.
            CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
                DXGI_FORMAT_D24_UNORM_S8_UINT,
                lround(m_d3dRenderTargetSize.Width),
                lround(m_d3dRenderTargetSize.Height),
                1, // This depth stencil view has only one texture.
                1, // Use a single mipmap level.
                D3D11_BIND_DEPTH_STENCIL
            );

            ComPtr<ID3D11Texture2D1> depthStencil;
            DX::ThrowIfFailed(
                m_d3dDevice->CreateTexture2D1(
                    &depthStencilDesc,
                    nullptr,
                    &depthStencil
                )
            );

            CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
            DX::ThrowIfFailed(
                m_d3dDevice->CreateDepthStencilView(
                    depthStencil.Get(),
                    &depthStencilViewDesc,
                    &m_d3dDepthStencilView
                )
            );

            // Set the 3D rendering viewport to target the entire window.
            m_screenViewport = CD3D11_VIEWPORT(
                0.0f,
                0.0f,
                m_d3dRenderTargetSize.Width,
                m_d3dRenderTargetSize.Height
            );

            m_d3dContext->RSSetViewports(1, &m_screenViewport);
        }

        bool m_windowClosed;
        bool m_windowVisible;
        float m_scroll;
        bool m_showKeyboard;
        bool m_keyUps[512];

        // Direct3D objects.
        Microsoft::WRL::ComPtr<ID3D11Device3>			m_d3dDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext3>	m_d3dContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain3>			m_swapChain;

        // Direct3D rendering objects. Required for 3D.
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView1>	m_d3dRenderTargetView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	m_d3dDepthStencilView;
        D3D11_VIEWPORT									m_screenViewport;

        // Cached reference to the Window.
        Platform::Agile<Windows::UI::Core::CoreWindow> m_window;

        // Cached device properties.
        D3D_FEATURE_LEVEL								m_d3dFeatureLevel;
        Windows::Foundation::Size						m_d3dRenderTargetSize;
        Windows::Foundation::Size						m_size;
        Windows::Graphics::Display::DisplayOrientations	m_nativeOrientation;
        Windows::Graphics::Display::DisplayOrientations	m_currentOrientation;
        float											m_dpi;
    };
}

ref class Direct3DApplicationSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
    virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView()
    {
        return ref new BCChat::App();
    }
};

// The main function is only used to initialize our IFrameworkView class.
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
    auto direct3DApplicationSource = ref new Direct3DApplicationSource();
    CoreApplication::Run(direct3DApplicationSource);
    return 0;
}
