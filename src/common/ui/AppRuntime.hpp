#pragma once
#include <filesystem>
#include <string>
struct GLFWwindow;
namespace SeisWave::Ui {
inline constexpr int returnHome = 100;
std::filesystem::path outputRoot();
std::filesystem::path elevationExampleRoot();
float interfaceFontSize();
void setInterfaceFontSize(float);
void saveAppearancePreferences();
void validateWindowState();
class AppRuntime {
  public:
    AppRuntime() = default;
    ~AppRuntime();
    bool open(const char *title, bool hidden, std::string &error);
    void begin();
    void present();
    GLFWwindow *window() const {
        return window_;
    }
    AppRuntime(const AppRuntime &) = delete;

  private:
    GLFWwindow *window_{};
    bool glfw_{}, imgui_{}, implot_{}, glfwBackend_{}, glBackend_{};
};
bool capture(const std::filesystem::path &, int, int);
} // namespace SeisWave::Ui
