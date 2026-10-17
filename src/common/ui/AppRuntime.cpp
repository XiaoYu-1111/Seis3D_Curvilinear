#include "AppRuntime.hpp"
#include "Presentation3D.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>
#endif
namespace SeisWave::Ui {
namespace {
float interfaceFont = 20;
struct WindowState {
    int width{1600}, height{1000}, x{}, y{};
    bool saved{}, maximized{};
} windowState;
} // namespace
float interfaceFontSize() {
    return interfaceFont;
}
void saveAppearancePreferences() {
    const auto folder = outputRoot() / "ui";
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec)
        return;
    const auto file = folder / "preferences.cfg";
    std::map<std::string, std::string> values;
    std::ifstream in(file);
    std::string key, value;
    while (in >> key >> value)
        values[key] = value;
    in.close();
    values["font"] = std::to_string(interfaceFontSize());
    values["language"] = std::to_string(Seis3D::Curvilinear::Ui::language3D());
    values["theme"] = std::to_string(Seis3D::Curvilinear::Ui::themeIndex3D());
    std::ofstream out(file);
    for (const auto &[name, setting] : values)
        out << name << ' ' << setting << '\n';
}
void setInterfaceFontSize(float size) {
    if (std::isfinite(size))
        interfaceFont = std::clamp(size, 16.f, 28.f);
}
std::filesystem::path outputRoot() {
    return SEIS3D_OUTPUT_ROOT;
}
bool AppRuntime::open(const char *title, bool hidden, std::string &error) {
    if (!glfwInit()) {
        error = "GLFW initialization failed.";
        return false;
    }
    glfw_ = true;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_MAXIMIZED, windowState.maximized ? GLFW_TRUE : GLFW_FALSE);
    window_ = glfwCreateWindow(windowState.width, windowState.height, title, nullptr, nullptr);
    if (!window_) {
        error = "OpenGL 4.3 window creation failed.";
        return false;
    }
    glfwSetWindowSizeLimits(window_, 960, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);
    if (windowState.saved && !windowState.maximized)
        glfwSetWindowPos(window_, windowState.x, windowState.y);
#ifdef _WIN32
    if (windowState.saved) {
        // rcNormalPosition survives maximization, unlike glfwGetWindowSize/Pos.
        WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
        if (GetWindowPlacement(glfwGetWin32Window(window_), &placement)) {
            placement.rcNormalPosition = {windowState.x, windowState.y, windowState.x + windowState.width,
                                          windowState.y + windowState.height};
            placement.showCmd = SW_HIDE;
            SetWindowPlacement(glfwGetWin32Window(window_), &placement);
        }
    }
#endif
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(hidden ? 0 : 1);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        error = "GLEW initialization failed.";
        return false;
    }
    while (glGetError() != GL_NO_ERROR) {
    }
#ifdef _WIN32
    for (int size : {ICON_BIG, ICON_SMALL}) {
        const auto icon = static_cast<HICON>(
            LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON,
                       GetSystemMetrics(size == ICON_BIG ? SM_CXICON : SM_CXSMICON),
                       GetSystemMetrics(size == ICON_BIG ? SM_CYICON : SM_CYSMICON), LR_SHARED));
        if (icon)
            SendMessageW(glfwGetWin32Window(window_), WM_SETICON, size, reinterpret_cast<LPARAM>(icon));
    }
#endif
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imgui_ = true;
    ImPlot::CreateContext();
    implot_ = true;
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;
    bool loaded = false;
#ifdef _WIN32
    std::vector<wchar_t> directory(32768);
    if (GetWindowsDirectoryW(directory.data(), static_cast<UINT>(directory.size()))) {
        const auto font = std::filesystem::path(directory.data()) / "Fonts/msyh.ttc";
        const auto name = font.u8string();
        if (std::filesystem::exists(font))
            loaded =
                io.Fonts->AddFontFromFileTTF(std::string(name.begin(), name.end()).c_str(), 20) != nullptr;
    }
#endif
    if (!loaded) {
        ImFontConfig config;
        config.SizePixels = 20;
        io.Fonts->AddFontDefault(&config);
    }
    Seis3D::Curvilinear::Ui::applyTheme3D(Seis3D::Curvilinear::Ui::themeIndex3D());
    glfwBackend_ = ImGui_ImplGlfw_InitForOpenGL(window_, true);
    glBackend_ = ImGui_ImplOpenGL3_Init("#version 430");
    if (!glfwBackend_ || !glBackend_) {
        error = "ImGui backend initialization failed.";
        return false;
    }
    if (!hidden) {
        glfwShowWindow(window_);
        if (windowState.maximized)
            glfwMaximizeWindow(window_);
    }
    error.clear();
    return true;
}
AppRuntime::~AppRuntime() {
    if (window_) {
        windowState.maximized = glfwGetWindowAttrib(window_, GLFW_MAXIMIZED) == GLFW_TRUE;
#ifdef _WIN32
        WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
        if (GetWindowPlacement(glfwGetWin32Window(window_), &placement)) {
            const auto r = placement.rcNormalPosition;
            windowState.x = r.left;
            windowState.y = r.top;
            windowState.width = std::max(960L, r.right - r.left);
            windowState.height = std::max(640L, r.bottom - r.top);
            windowState.maximized |=
                placement.showCmd == SW_SHOWMAXIMIZED ||
                ((placement.showCmd == SW_SHOWMINIMIZED || placement.showCmd == SW_MINIMIZE ||
                  placement.showCmd == SW_SHOWMINNOACTIVE) &&
                 (placement.flags & WPF_RESTORETOMAXIMIZED) != 0);
            windowState.saved = true;
        }
#else
        if (!windowState.maximized && !glfwGetWindowAttrib(window_, GLFW_ICONIFIED)) {
            glfwGetWindowPos(window_, &windowState.x, &windowState.y);
            glfwGetWindowSize(window_, &windowState.width, &windowState.height);
            windowState.saved = true;
        }
#endif
    }
    if (glBackend_)
        ImGui_ImplOpenGL3_Shutdown();
    if (glfwBackend_)
        ImGui_ImplGlfw_Shutdown();
    if (implot_)
        ImPlot::DestroyContext();
    if (imgui_)
        ImGui::DestroyContext();
    if (window_)
        glfwDestroyWindow(window_);
    if (glfw_)
        glfwTerminate();
}
void validateWindowState() {
    std::string error;
    {
        AppRuntime first;
        if (!first.open("Window state test", true, error))
            throw std::runtime_error(error);
        glfwSetWindowSize(first.window(), 1280, 820);
        glfwSetWindowPos(first.window(), 120, 90);
        glfwPollEvents();
    }
    const auto normal = windowState;
    {
        AppRuntime second;
        if (!second.open("Window state test", true, error))
            throw std::runtime_error(error);
        int w{}, h{};
        glfwGetWindowSize(second.window(), &w, &h);
        if (w != 1280 || h != 820)
            throw std::runtime_error("Normal window size changed during routing");
#ifdef _WIN32
        const auto hwnd = glfwGetWin32Window(second.window());
        // Emulate maximized native state without showing a window on the desktop.
        SetWindowLongPtrW(hwnd, GWL_STYLE, GetWindowLongPtrW(hwnd, GWL_STYLE) | WS_MAXIMIZE);
#endif
    }
#ifdef _WIN32
    if (!windowState.maximized)
        throw std::runtime_error("Maximized window state was lost");
    for (int route = 0; route < 3; ++route) {
        AppRuntime next;
        if (!next.open("Window state test", true, error))
            throw std::runtime_error(error);
        if (!glfwGetWindowAttrib(next.window(), GLFW_MAXIMIZED))
            throw std::runtime_error("Reopened window is not maximized");
    }
    if (!windowState.maximized || windowState.width != normal.width || windowState.height != normal.height ||
        windowState.x != normal.x || windowState.y != normal.y)
        throw std::runtime_error("Maximized routing changed the restore rectangle");
#endif
}
void AppRuntime::begin() {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::PushFont(nullptr, interfaceFontSize());
}
void AppRuntime::present() {
    ImGui::PopFont();
    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    glViewport(0, 0, w, h);
    const auto p = Seis3D::Curvilinear::Ui::palette3D().background;
    glClearColor(p.x, p.y, p.z, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}
bool capture(const std::filesystem::path &path, int width, int height) {
    std::filesystem::create_directories(path.parent_path());
    std::vector<unsigned char> data(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y)
        out.write(reinterpret_cast<const char *>(data.data() + static_cast<std::size_t>(y) * width * 3),
                  width * 3);
    return out.good();
}
} // namespace SeisWave::Ui
