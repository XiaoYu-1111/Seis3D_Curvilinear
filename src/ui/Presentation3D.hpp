#pragma once
#include "imgui.h"
#include <string>
namespace Seis3D::Curvilinear::Ui {
struct Theme3D {const char* name;ImVec4 background,panel,input,text,muted,accent,border;bool light;};
constexpr int themeCount3D=10;
enum class FunctionRole3D {Run,Model,Display,Export,Warning,Danger};
ImVec4 functionColor3D(FunctionRole3D);
void pushFunctionStyle3D(FunctionRole3D);
void popFunctionStyle3D();
bool functionButton3D(const char*,FunctionRole3D,ImVec2 size={0,0});
const Theme3D& palette3D();
const char* themeName3D(int);
void applyTheme3D(int);
void setLanguage3D(int); // 0 English, 1 Simplified Chinese; labels retain their original ImGui IDs.
int language3D();
const char* tr3D(const char*);
const char* label3D(const char*);
const char* choices3D(const char*);
bool validateCatalog3D(std::string&);
ImU32 color3D(ImVec4,float alpha=1);
}
