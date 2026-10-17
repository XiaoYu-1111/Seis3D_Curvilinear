#pragma once
namespace SeisWave::Ui {
enum class Route {
    Quit,
    Home,
    TwoD,
    ThreeD
};
Route runHomeScreen(bool smoke = false, int automatedChoice = 0);
} // namespace SeisWave::Ui
