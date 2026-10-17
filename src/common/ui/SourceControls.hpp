#pragma once
#include "Presentation3D.hpp"
#include "SeisCommon/SourceMechanism.hpp"
#include "imgui.h"
#include "imgui_internal.h"
namespace SeisWave::Ui {
inline bool sourceControls(SourceMechanism &source, double &force, double &moment, bool planar,
                           ImRect *mechanismRect = nullptr, ImRect *momentRect = nullptr) {
    using namespace Seis3D::Curvilinear::Ui;
    const auto text = [](const char *en, const char *zh) { return language3D() ? zh : en; };
    bool changed = false;
    int kind = static_cast<int>(source.kind);
    ImGui::TextUnformatted(text("Source mechanism", "震源激发类型"));
    ImGui::SetNextItemWidth(-1);
    const char *choices[]{text("Directional force", "定向体力源"),
                          text("Isotropic explosion", "各向同性爆炸源")};
    const bool open = ImGui::BeginCombo("##sourceMechanism", kind >= 0 && kind < 2 ? choices[kind] : "?");
    if (mechanismRect)
        *mechanismRect = {ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
    if (open) {
        for (int i = 0; i < 2; ++i) {
            if (ImGui::Selectable(choices[i], kind == i)) {
                source.kind = static_cast<SourceKind>(i);
                changed = true;
            }
            if (i == 1 && momentRect)
                *momentRect = {ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
            if (kind == i)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    const bool explosion = source.kind == SourceKind::IsotropicMoment;
    ImGui::TextUnformatted(explosion ? (planar ? text("Moment per Y length (N)", "单位 Y 长度矩幅值（N）")
                                               : text("Diagonal moment amplitude (N m)", "对角矩幅值（N·m）"))
                                     : (planar ? text("Force per Y length (N/m)", "单位 Y 长度力幅值（N/m）")
                                               : text("Force amplitude (N)", "体力合力幅值（N）")));
    ImGui::SetNextItemWidth(-1);
    changed |= ImGui::InputDouble("##sourceAmplitude", explosion ? &moment : &force, 0, 0, "%.6g");
    if (!explosion) {
        const char *axes[]{"X", "Y", "Z"};
        for (int a = 0; a < 3; ++a) {
            if (planar && a == 1)
                continue;
            if (a)
                ImGui::SameLine();
            if (ImGui::Button(axes[a])) {
                source.direction = {0, 0, 0};
                source.direction[a] = 1;
                changed = true;
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", text("Cartesian direction", "物理坐标方向"));
        if (ImGui::TreeNode(text("Custom direction (normalized)", "自定义方向（自动单位化）"))) {
            ImGui::SetNextItemWidth(-1);
            if (planar) {
                double v[]{source.direction[0], source.direction[2]};
                if (ImGui::InputScalarN("##sourceDirection", ImGuiDataType_Double, v, 2, nullptr, nullptr,
                                        "%.3f")) {
                    source.direction = {v[0], 0, v[1]};
                    changed = true;
                }
                ImGui::TextDisabled("X / Z");
            } else {
                changed |= ImGui::InputScalarN("##sourceDirection", ImGuiDataType_Double,
                                               source.direction.data(), 3, nullptr, nullptr, "%.3f");
                ImGui::TextDisabled("X / Y / Z");
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::TextWrapped("%s", text("Equal diagonal moments, zero shear moment; center must be five nodes "
                                      "inside each face. Apply / rebuild to activate.",
                                      "等对角矩、零剪切矩；源心需距各边界至少 5 个节点。重置/应用后生效。"));
    }
    return changed;
}
} // namespace SeisWave::Ui
