#include "Presentation3D.hpp"
#include "implot.h"
#include <algorithm>
#include <cstring>
#include <string_view>
#include <unordered_map>
namespace Seis3D::Curvilinear::Ui {
namespace {
int selectedLanguage = 1, selectedTheme = 3;
const Theme3D themes[]{{"Paper",
                        {.96f, .945f, .91f, 1},
                        {.995f, .985f, .96f, 1},
                        {.92f, .905f, .865f, 1},
                        {.16f, .20f, .19f, 1},
                        {.36f, .41f, .38f, 1},
                        {.10f, .40f, .32f, 1},
                        {.68f, .70f, .63f, 1},
                        true},
                       {"Arctic",
                        {.935f, .955f, .975f, 1},
                        {.995f, .998f, 1, 1},
                        {.91f, .945f, .975f, 1},
                        {.10f, .17f, .26f, 1},
                        {.34f, .43f, .53f, 1},
                        {.13f, .34f, .66f, 1},
                        {.65f, .74f, .83f, 1},
                        true},
                       {"Soft Jade",
                        {.94f, .97f, .94f, 1},
                        {.98f, .995f, .97f, 1},
                        {.87f, .93f, .87f, 1},
                        {.13f, .23f, .17f, 1},
                        {.34f, .45f, .37f, 1},
                        {.12f, .43f, .28f, 1},
                        {.65f, .76f, .67f, 1},
                        true},
                       {"CUDA Green",
                        {.035f, .055f, .052f, 1},
                        {.065f, .095f, .09f, 1},
                        {.10f, .17f, .14f, 1},
                        {.91f, .96f, .93f, 1},
                        {.55f, .68f, .62f, 1},
                        {.46f, .86f, .27f, 1},
                        {.22f, .4f, .29f, 1},
                        false},
                       {"Ocean Blue",
                        {.035f, .052f, .08f, 1},
                        {.06f, .09f, .135f, 1},
                        {.09f, .16f, .23f, 1},
                        {.91f, .95f, .99f, 1},
                        {.54f, .66f, .79f, 1},
                        {.22f, .68f, .96f, 1},
                        {.2f, .36f, .52f, 1},
                        false},
                       {"Graphite",
                        {.065f, .07f, .08f, 1},
                        {.105f, .115f, .13f, 1},
                        {.17f, .18f, .20f, 1},
                        {.93f, .94f, .96f, 1},
                        {.6f, .63f, .68f, 1},
                        {.72f, .77f, .84f, 1},
                        {.31f, .34f, .39f, 1},
                        false},
                       {"Scientific Light",
                        {.93f, .95f, .97f, 1},
                        {.99f, .995f, 1, 1},
                        {.88f, .92f, .96f, 1},
                        {.12f, .18f, .24f, 1},
                        {.35f, .43f, .51f, 1},
                        {.035f, .36f, .64f, 1},
                        {.65f, .74f, .82f, 1},
                        true},
                       {"Midnight Violet",
                        {.045f, .04f, .08f, 1},
                        {.085f, .075f, .135f, 1},
                        {.16f, .14f, .245f, 1},
                        {.95f, .93f, .99f, 1},
                        {.65f, .61f, .77f, 1},
                        {.69f, .54f, .97f, 1},
                        {.32f, .27f, .45f, 1},
                        false},
                       {"Teal Studio",
                        {.025f, .065f, .075f, 1},
                        {.045f, .105f, .12f, 1},
                        {.08f, .19f, .205f, 1},
                        {.91f, .97f, .97f, 1},
                        {.54f, .71f, .73f, 1},
                        {.16f, .80f, .76f, 1},
                        {.18f, .40f, .42f, 1},
                        false},
                       {"Warm Ivory",
                        {.97f, .95f, .92f, 1},
                        {1, .99f, .97f, 1},
                        {.96f, .935f, .89f, 1},
                        {.22f, .18f, .15f, 1},
                        {.44f, .38f, .32f, 1},
                        {.64f, .31f, .12f, 1},
                        {.76f, .69f, .59f, 1},
                        true}};
struct Entry {
    const char *english;
    const char *chinese;
};
const Entry entries[]{
#include "Translations3D.inc"
};
const std::unordered_map<std::string_view, const char *> &dictionary() {
    static const auto values = [] {
        std::unordered_map<std::string_view, const char *> m;
        for (const auto &e : entries)
            m.emplace(e.english, e.chinese);
        return m;
    }();
    return values;
}
ImVec4 blend(ImVec4 a, ImVec4 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1};
}
} // namespace
const Theme3D &palette3D() {
    return themes[selectedTheme];
}
const char *themeName3D(int i) {
    return tr3D(themes[std::clamp(i, 0, themeCount3D - 1)].name);
}
void setLanguage3D(int language) {
    selectedLanguage = std::clamp(language, 0, 1);
}
int language3D() {
    return selectedLanguage;
}
int themeIndex3D() {
    return selectedTheme;
}
const char *tr3D(const char *source) {
    if (!source || selectedLanguage == 0)
        return source;
    const auto found = dictionary().find(source);
    return found == dictionary().end() ? source : found->second;
}
const char *label3D(const char *source) {
    if (!source || source[0] == '#')
        return source;
    static std::unordered_map<std::string, std::string> labels[2];
    auto &cache = labels[selectedLanguage];
    auto found = cache.find(source);
    if (found != cache.end())
        return found->second.c_str();
    return cache.emplace(source, std::string(tr3D(source)) + "###" + source).first->second.c_str();
}
const char *choices3D(const char *source) {
    static std::unordered_map<std::string, std::string> choices[2];
    std::string key;
    for (const char *p = source; *p; p += std::strlen(p) + 1) {
        key += p;
        key += '\n';
    }
    auto &cache = choices[selectedLanguage];
    auto found = cache.find(key);
    if (found != cache.end())
        return found->second.c_str();
    std::string result;
    for (const char *p = source; *p; p += std::strlen(p) + 1) {
        result += tr3D(p);
        result += '\0';
    }
    result += '\0';
    return cache.emplace(key, std::move(result)).first->second.c_str();
}
bool validateCatalog3D(std::string &error) {
    std::unordered_map<std::string, std::string> seen;
    for (const auto &e : entries) {
        if (!*e.english || !*e.chinese || !seen.emplace(e.english, e.chinese).second) {
            error = "Empty or duplicate translation entry";
            return false;
        }
        // Format tokens must be byte-for-byte equal, including argument order and length modifiers.
        const auto formats = [](const char *p) {
            std::string out;
            while (*p) {
                if (*p++ != '%')
                    continue;
                out += '%';
                while (*p) {
                    const char c = *p++;
                    out += c;
                    if (std::strchr("diuoxXfFeEgGaAcsp%", c))
                        break;
                }
                out += '|';
            }
            return out;
        };
        if (formats(e.english) != formats(e.chinese)) {
            error = std::string("Translation format mismatch: ") + e.english;
            return false;
        }
    }
    return true;
}
ImU32 color3D(ImVec4 c, float alpha) {
    c.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}
ImVec4 functionColor3D(FunctionRole3D role) {
    const bool light = palette3D().light;
    switch (role) {
    case FunctionRole3D::Run:
        return light ? ImVec4(.20f, .46f, .23f, 1) : ImVec4(.40f, .78f, .43f, 1);
    case FunctionRole3D::Model:
        return light ? ImVec4(.24f, .39f, .64f, 1) : ImVec4(.42f, .63f, .93f, 1);
    case FunctionRole3D::Display:
        return light ? ImVec4(.12f, .46f, .45f, 1) : ImVec4(.25f, .76f, .73f, 1);
    case FunctionRole3D::Export:
        return light ? ImVec4(.48f, .30f, .63f, 1) : ImVec4(.76f, .55f, .94f, 1);
    case FunctionRole3D::Warning:
        return light ? ImVec4(.66f, .42f, .12f, 1) : ImVec4(.95f, .70f, .28f, 1);
    case FunctionRole3D::Danger:
        return light ? ImVec4(.70f, .25f, .19f, 1) : ImVec4(.95f, .42f, .35f, 1);
    }
    return palette3D().accent;
}
void pushFunctionStyle3D(FunctionRole3D role) {
    const auto c = functionColor3D(role), base = palette3D().panel;
    ImGui::PushStyleColor(ImGuiCol_Button, blend(base, c, .19f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, blend(base, c, .33f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, blend(base, c, .48f));
    ImGui::PushStyleColor(ImGuiCol_Header, blend(base, c, .16f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, blend(base, c, .29f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, blend(base, c, .42f));
    ImGui::PushStyleColor(ImGuiCol_Border, blend(base, c, .60f));
}
void popFunctionStyle3D() {
    ImGui::PopStyleColor(7);
}
bool functionButton3D(const char *text, FunctionRole3D role, ImVec2 size) {
    pushFunctionStyle3D(role);
    const bool clicked = ImGui::Button(label3D(text), size);
    popFunctionStyle3D();
    return clicked;
}
void applyTheme3D(int theme) {
    if (ImPlot::GetCurrentContext())
        ImPlot::StyleColorsAuto();
    selectedTheme = std::clamp(theme, 0, themeCount3D - 1);
    if (!ImGui::GetCurrentContext())
        return;
    const auto &p = palette3D();
    if (p.light)
        ImGui::StyleColorsLight();
    else
        ImGui::StyleColorsDark();
    auto &s = ImGui::GetStyle();
    s.WindowPadding = {12, 10};
    s.FramePadding = {8, 5};
    s.ItemSpacing = {8, 7};
    s.FrameRounding = 4;
    s.TabRounding = 4;
    s.GrabRounding = 4;
    s.ChildRounding = 5;
    s.PopupRounding = 6;
    s.FrameBorderSize = 1;
    s.DisabledAlpha = .65f;
    auto *c = s.Colors;
    c[ImGuiCol_Text] = p.text;
    c[ImGuiCol_TextDisabled] = p.muted;
    c[ImGuiCol_WindowBg] = p.panel;
    c[ImGuiCol_ChildBg] = p.panel;
    c[ImGuiCol_PopupBg] = p.panel;
    c[ImGuiCol_Border] = p.border;
    c[ImGuiCol_FrameBg] = p.input;
    c[ImGuiCol_FrameBgHovered] = blend(p.input, p.accent, .2f);
    c[ImGuiCol_FrameBgActive] = blend(p.input, p.accent, .35f);
    c[ImGuiCol_Button] = blend(p.input, p.accent, .12f);
    c[ImGuiCol_ButtonHovered] = blend(p.input, p.accent, .3f);
    c[ImGuiCol_ButtonActive] = blend(p.input, p.accent, .5f);
    c[ImGuiCol_Header] = blend(p.panel, p.accent, .15f);
    c[ImGuiCol_HeaderHovered] = blend(p.panel, p.accent, .3f);
    c[ImGuiCol_HeaderActive] = blend(p.panel, p.accent, .45f);
    c[ImGuiCol_Tab] = p.input;
    c[ImGuiCol_TabSelected] = blend(p.panel, p.accent, .22f);
    c[ImGuiCol_TabHovered] = blend(p.panel, p.accent, .4f);
    c[ImGuiCol_TabDimmed] = p.panel;
    c[ImGuiCol_TabDimmedSelected] = p.input;
    c[ImGuiCol_TabSelectedOverline] = p.accent;
    c[ImGuiCol_TitleBg] = p.background;
    c[ImGuiCol_TitleBgActive] = p.input;
    c[ImGuiCol_MenuBarBg] = p.background;
    c[ImGuiCol_CheckMark] = p.accent;
    c[ImGuiCol_SliderGrab] = p.accent;
    c[ImGuiCol_SliderGrabActive] = blend(p.accent, p.text, .2f);
    c[ImGuiCol_PlotHistogram] = p.accent;
    c[ImGuiCol_Separator] = p.border;
    c[ImGuiCol_DockingPreview] = {p.accent.x, p.accent.y, p.accent.z, .35f};
    c[ImGuiCol_DockingEmptyBg] = p.background;
    c[ImGuiCol_ScrollbarBg] = p.background;
    c[ImGuiCol_ScrollbarGrab] = p.border;
    c[ImGuiCol_TextSelectedBg] = {p.accent.x, p.accent.y, p.accent.z, .3f};
}
} // namespace Seis3D::Curvilinear::Ui
