#include "RecordView3D.hpp"
#include "Presentation3D.hpp"
#include "RecordPalettes3D.hpp"
#include "implot.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
namespace Seis3D::Curvilinear::Ui {
namespace {
constexpr const char* styles="Wiggle\0Variable area\0Density image\0Density + wiggle\0Signed lobes\0Density + signed lobes\0";
constexpr const char* components="Vz\0Vx\0Vy\0Velocity magnitude\0";
double valueAt(const ReceiverTrace3D& trace,int c,std::size_t sample) {
    if(c==0)return trace.samples.at(sample);
    if(c==1)return trace.samplesVx.at(sample);
    if(c==2)return trace.samplesVy.at(sample);
    return std::hypot(trace.samplesVx.at(sample),trace.samplesVy.at(sample),trace.samples.at(sample));
}
ImPlotColormap colormap(int palette,float opacity=1) {
    const int alpha=std::clamp(static_cast<int>(std::lround(opacity*100)),5,100);
    const auto name="CurvilinearRecords"+std::to_string(palette)+"Alpha"+std::to_string(alpha);
    const auto cached=ImPlot::GetColormapIndex(name.c_str());if(cached>=0)return cached;
    ImVec4 stops[5];const auto& colors=RecordPalettes::wavePalette(palette);
    for(int i=0;i<5;++i)stops[i]={colors[i][0],colors[i][1],colors[i][2],alpha*.01f};
    return ImPlot::AddColormap(name.c_str(),stops,5,false);
}
ImVec4 polarity(int palette,bool positive) {const auto& c=RecordPalettes::wavePalette(palette)[positive?4:0];return {c[0],c[1],c[2],1};}
struct Curve {const TraceProcessing::TracePreview* data{};double baseline{},scale{1};int lobe{};bool single{};};
ImPlotPoint curve(int i,void* pointer) {
    const auto& p=*static_cast<Curve*>(pointer);const double v=p.data->values[i];
    const double x=(p.lobe>0?std::max(0.0,v):p.lobe<0?std::min(0.0,v):v)*p.scale;
    return p.single?ImPlotPoint(p.data->times[i],x):ImPlotPoint(p.baseline+x,p.data->times[i]);
}
ImPlotPoint baseline(int i,void* pointer) {const auto& p=*static_cast<Curve*>(pointer);return p.single?ImPlotPoint(p.data->times[i],0):ImPlotPoint(p.baseline,p.data->times[i]);}
void releaseAutoFit(bool& automatic,bool& fit,ImVec2 size) {
    const auto at=ImGui::GetCursorScreenPos(),mouse=ImGui::GetIO().MousePos;
    const bool inside=mouse.x>=at.x&&mouse.x<=at.x+size.x&&mouse.y>=at.y&&mouse.y<=at.y+size.y;
    if(inside&&ImGui::IsWindowHovered()&&(ImGui::GetIO().MouseWheel!=0||ImGui::IsMouseDragging(ImGuiMouseButton_Left)||ImGui::IsMouseDragging(ImGuiMouseButton_Right))){automatic=false;fit=false;}
}
void plotColors() {
    ImPlot::PushStyleColor(ImPlotCol_FrameBg,palette3D().background);ImPlot::PushStyleColor(ImPlotCol_PlotBg,palette3D().background);ImPlot::PushStyleColor(ImPlotCol_PlotBorder,palette3D().border);
    ImPlot::PushStyleColor(ImPlotCol_AxisText,palette3D().text);ImPlot::PushStyleColor(ImPlotCol_AxisGrid,color3D(palette3D().muted,.18f));
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotMinSize,ImVec2(120,60));
}
void finishPlotColors(){ImPlot::PopStyleVar();ImPlot::PopStyleColor(5);}
}
void RecordView3D::options(int total) {
    if(ImGui::Button(label3D("Options"),{-1,0}))ImGui::OpenPopup("RecordOptions3D");
    const auto* vp=ImGui::GetMainViewport();
    ImGui::SetNextWindowSizeConstraints({std::min(420.0f,vp->WorkSize.x*.8f),0},{std::min(650.0f,vp->WorkSize.x*.9f),vp->WorkSize.y*.8f});
    if(!ImGui::BeginPopup("RecordOptions3D"))return;
    const auto old=settings;
    const auto field=[](const char* label){ImGui::TextUnformatted(tr3D(label));ImGui::SetNextItemWidth(-1);};
    field("Record style");ImGui::Combo("##style",&settings.style,choices3D(styles));
    field("Record palette");ImGui::Combo("##palette",&settings.palette,choices3D(RecordPalettes::wavePaletteNames));
    field("Trace gain");ImGui::SliderFloat("##gain",&settings.gain,.1f,20,"%.2fx",ImGuiSliderFlags_Logarithmic);
    field("Heatmap range");ImGui::SliderFloat("##range",&settings.heatmapRange,.05f,1,"%.2f");
    field("Density opacity");ImGui::SliderFloat("##opacity",&settings.opacity,.05f,1,"%.2f");
    ImGui::Checkbox(label3D("Normalize each trace"),&settings.normalizeEach);
    ImGui::TextWrapped(tr3D(settings.normalizeEach?"Individual scales: relative channel amplitudes are not comparable.":"Shared scale: relative amplitudes of visible channels are preserved."));
    ImGui::Checkbox(label3D("Automatic gain compensation"),&settings.agc.enabled);
    if(settings.agc.enabled) {
        field("AGC window (ms)");ImGui::SliderFloat("##agcWindow",&settings.agc.windowMs,5,2000,"%.0f");
        field("AGC strength");ImGui::SliderFloat("##agcStrength",&settings.agc.strength,0,1,"%.2f");
        field("Maximum AGC boost");ImGui::SliderFloat("##agcMax",&settings.agc.maxBoost,1,200,"%.0fx");
        field("Noise floor (percent of raw peak)");ImGui::SliderFloat("##agcFloor",&settings.agc.noiseFloorPercent,0,2,"%.3f%%");
    }
    ImGui::Checkbox(label3D("Show all traces"),&settings.showAll);
    if(!settings.showAll) {
        int window[2]{settings.first+1,settings.visible};field("First / visible traces");
        if(ImGui::InputInt2("##traceWindow",window)){settings.first=std::clamp(window[0]-1,0,total-1);settings.visible=std::clamp(window[1],1,total-settings.first);}
    }
    ImGui::Checkbox(label3D("Follow full record"),&settings.autoFit);
    if(ImGui::Button(label3D("Fit records")))fit_=true;
    ImGui::SameLine();if(ImGui::Button(label3D("Reset record display"))){settings=RecordSettings3D{};fit_=true;}
    ImGui::TextWrapped(tr3D("Display gain, AGC and normalization affect preview copies only. Raw CSV samples remain unchanged."));
    if(!(settings==old))fit_=true;
    ImGui::EndPopup();
}
void RecordView3D::refresh(const ISimulationBackend3D& s,int selected) {
    const int selectedKey=settings.view==1?selected:-1;
    if(revision_==s.revision()&&cachedSettings_==settings&&cachedSelected_==selectedKey)return;
    const auto& traces=s.receivers();const int total=static_cast<int>(traces.size());
    first_=settings.showAll?0:std::clamp(settings.first,0,total-1);count_=settings.showAll?total:std::clamp(settings.visible,1,total-first_);
    const auto n=traces.front().samples.size();dt_=s.dt();endTime_=std::max(dt_,static_cast<double>(n-1)*dt_);
    std::vector<double> times(n);for(std::size_t t=0;t<n;++t)times[t]=t*dt_;
    series_.clear();rawPeak_=commonPeak_=0;
    const int seriesCount=settings.view==1?3:count_;
    for(int r=0;r<seriesCount;++r) {
        Series series;series.receiver=settings.view==1?selected:first_+r;series.component=settings.view==1?r:settings.component;
        std::vector<double> values(n);for(std::size_t t=0;t<n;++t){values[t]=valueAt(traces[series.receiver],series.component,t);series.rawPeak=std::max(series.rawPeak,std::abs(values[t]));}
        rawPeak_=std::max(rawPeak_,series.rawPeak);
        TraceProcessing::applyAgc(values,times,settings.agc);series.preview=TraceProcessing::reduceExtrema(values,times);
        commonPeak_=std::max(commonPeak_,series.preview.peak);series_.push_back(std::move(series));
    }
    densityRows_=static_cast<int>(std::min<std::size_t>(1024,std::max<std::size_t>(2,n)));
    density_.clear();
    if(settings.view==0) density_=TraceProcessing::buildTraceDensity(count_,densityRows_,0,endTime_,commonPeak_,settings.normalizeEach,
        [&](int i)->const auto&{return series_[i].preview.times;},[&](int i)->const auto&{return series_[i].preview.values;},[&](int i){return series_[i].preview.peak;});
    cachedSettings_=settings;cachedSelected_=selectedKey;revision_=s.revision();
}
void RecordView3D::section(const ISimulationBackend3D& s,int& selected) {
    const bool density=settings.style==2||settings.style==3||settings.style==5;
    const bool wiggle=settings.style!=2,fill=settings.style==1||settings.style==3||settings.style==4||settings.style==5;
    const bool signedFill=settings.style==4||settings.style==5;
    const auto available=ImGui::GetContentRegionAvail();const bool scaleBar=density&&available.x>650&&available.y>140;
    const float scaleWidth=ImGui::GetFontSize()*5+30;
    const ImVec2 size{available.x-(scaleBar?scaleWidth+ImGui::GetStyle().ItemSpacing.x:0),std::max(60.0f,available.y)};
    releaseAutoFit(settings.autoFit,fit_,size);plotColors();
    const auto limitsFlags=size.y<160?ImPlotAxisFlags_NoLabel:ImPlotAxisFlags_None;
    if(ImPlot::BeginPlot("##ReceiverSection3D",size,ImPlotFlags_NoTitle|ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxes(tr3D("Receiver ID"),tr3D("Time (s)"),limitsFlags,ImPlotAxisFlags_Invert|limitsFlags);
        ImPlot::SetupAxisFormat(ImAxis_X1,"R%.0f");
        if(fit_||settings.autoFit){ImPlot::SetupAxisLimits(ImAxis_X1,first_+.4,first_+count_+.6,ImPlotCond_Always);ImPlot::SetupAxisLimits(ImAxis_Y1,0,endTime_,ImPlotCond_Always);}
        if(density&&!density_.empty()) {
            ImPlot::PushColormap(colormap(settings.palette,settings.opacity));
            const double range=settings.heatmapRange/settings.gain;
            ImPlot::PlotHeatmap("##Density",density_.data(),densityRows_,count_,-range,range,nullptr,{first_+.5,endTime_},{first_+count_+.5,0});ImPlot::PopColormap();
        }
        if(wiggle)for(auto& series:series_) {
            const double peak=settings.normalizeEach?series.preview.peak:commonPeak_;
            Curve p{&series.preview,series.receiver+1.0,.44*settings.gain/std::max(peak,1e-30),1,false};
            char label[40];std::snprintf(label,sizeof(label),"R%d",series.receiver+1);
            if(fill){ImPlot::SetNextFillStyle(polarity(settings.palette,true),.35f);ImPlot::PlotShadedG(label,curve,&p,baseline,&p,static_cast<int>(series.preview.values.size()));
                if(signedFill){p.lobe=-1;ImPlot::SetNextFillStyle(polarity(settings.palette,false),.35f);ImPlot::PlotShadedG((std::string(label)+"##negative").c_str(),curve,&p,baseline,&p,static_cast<int>(series.preview.values.size()));}}
            const auto& neutral=RecordPalettes::wavePalette(settings.palette)[2];
            const bool lightDensity=.2126f*neutral[0]+.7152f*neutral[1]+.0722f*neutral[2]>.5f;
            const auto ink=density?(lightDensity?ImVec4(.06f,.08f,.1f,1):ImVec4(.94f,.96f,.98f,1)):palette3D().text;
            p.lobe=0;ImPlot::SetNextLineStyle(series.receiver==selected?palette3D().accent:ink,series.receiver==selected?1.8f:1.0f);
            ImPlot::PlotLineG(label,curve,&p,static_cast<int>(series.preview.values.size()));
        }
        if(ImPlot::IsPlotHovered()) {
            const auto mouse=ImPlot::GetPlotMousePos();const int receiver=std::clamp(static_cast<int>(std::lround(mouse.x))-1,first_,first_+count_-1);
            const auto sample=static_cast<std::size_t>(std::clamp(std::llround(mouse.y/dt_),0LL,static_cast<long long>(s.currentStep())));
            ImGui::SetTooltip(tr3D("R%d | t %.5f s | raw %.5e m/s"),receiver+1,sample*dt_,valueAt(s.receivers()[receiver],settings.component,sample));
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))selected=receiver;
        }
        ImPlot::EndPlot();
    }
    if(scaleBar){ImGui::SameLine();const double range=settings.heatmapRange/settings.gain;ImPlot::ColormapScale(tr3D("Normalized amplitude"),-range,range,{scaleWidth,size.y},"%.2g",ImPlotColormapScaleFlags_None,colormap(settings.palette));}
    finishPlotColors();
}
void RecordView3D::single(const ISimulationBackend3D& s,int selected) {
    const auto size=ImGui::GetContentRegionAvail();const ImVec2 plotSize{size.x,std::max(60.0f,size.y)};
    releaseAutoFit(settings.autoFit,fit_,plotSize);plotColors();
    if(ImPlot::BeginPlot("##SingleReceiver3D",plotSize,ImPlotFlags_NoTitle)) {
        const auto flags=size.y<160?ImPlotAxisFlags_NoLabel:ImPlotAxisFlags_None;
        ImPlot::SetupAxes(tr3D("Time (s)"),tr3D(settings.normalizeEach?"Normalized amplitude":settings.agc.enabled?"Display amplitude (AGC)":"Velocity (m/s)"),flags,flags);
        ImPlot::SetupAxisFormat(ImAxis_Y1,"%.2g");ImPlot::SetupLegend(ImPlotLocation_NorthEast,ImPlotLegendFlags_Horizontal);
        if(fit_||settings.autoFit){const double peak=settings.normalizeEach?1:std::max(commonPeak_,1e-20);ImPlot::SetupAxisLimits(ImAxis_X1,0,endTime_,ImPlotCond_Always);ImPlot::SetupAxisLimits(ImAxis_Y1,-1.15*peak/settings.gain,1.15*peak/settings.gain,ImPlotCond_Always);}
        const char* names[]{"Vz","Vx","Vy"};
        const bool light=palette3D().light;
        const ImVec4 colors[]{light?ImVec4(.12f,.34f,.75f,1):ImVec4(.35f,.65f,1,1),
            light?ImVec4(.80f,.28f,.04f,1):ImVec4(1,.60f,.25f,1),
            light?ImVec4(.02f,.48f,.29f,1):ImVec4(.30f,.85f,.55f,1)};
        for(int c=0;c<3;++c){auto& series=series_[c];Curve p{&series.preview,0,settings.normalizeEach?1/std::max(series.preview.peak,1e-30):1,0,true};ImPlot::SetNextLineStyle(colors[c],1.6f);ImPlot::PlotLineG(names[c],curve,&p,static_cast<int>(series.preview.values.size()));}
        if(ImPlot::IsPlotHovered()) {
            const auto mouse=ImPlot::GetPlotMousePos();const auto sample=static_cast<std::size_t>(std::clamp(std::llround(mouse.x/dt_),0LL,static_cast<long long>(s.currentStep())));
            const auto& t=s.receivers()[selected];ImGui::SetTooltip(tr3D("R%d | t %.5f s\nVx %.5e | Vy %.5e | Vz %.5e m/s"),selected+1,sample*dt_,t.samplesVx[sample],t.samplesVy[sample],t.samples[sample]);
        }
        ImPlot::EndPlot();
    }
    finishPlotColors();
}
void RecordView3D::draw(const ISimulationBackend3D& s,int& selected,bool* open) {
    if(open&&!*open)return;
    if(!ImGui::Begin(label3D("Receiver records"),open)){ImGui::End();return;}
    const auto& receivers=s.receivers();const int total=static_cast<int>(receivers.size());
    if(total<1){ImGui::TextUnformatted(tr3D("No receivers in the active model."));ImGui::End();return;}
    selected=std::clamp(selected,0,total-1);
    if(ImGui::BeginTable("RecordToolbar3D",4,ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("view",ImGuiTableColumnFlags_WidthStretch,1.25f);ImGui::TableSetupColumn("component",ImGuiTableColumnFlags_WidthStretch,.7f);
        ImGui::TableSetupColumn("receiver",ImGuiTableColumnFlags_WidthStretch,1);ImGui::TableSetupColumn("options",ImGuiTableColumnFlags_WidthStretch,1.1f);
        ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);if(ImGui::Combo("##view",&settings.view,choices3D("Receiver section\0Single (3C)\0")))fit_=true;
        ImGui::TableNextColumn();ImGui::BeginDisabled(settings.view==1);ImGui::SetNextItemWidth(-1);if(ImGui::Combo("##component",&settings.component,choices3D(components)))fit_=true;ImGui::EndDisabled();
        ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);int receiver=selected+1;if(ImGui::SliderInt("##receiver",&receiver,1,total,"R%d"))selected=receiver-1;
        if(ImGui::IsItemHovered()){const auto& p=receivers[selected].position;ImGui::SetTooltip(tr3D("R%d XYZ: %.1f, %.1f, %.1f m"),selected+1,p[0],p[1],p[2]);}
        ImGui::TableNextColumn();options(total);ImGui::EndTable();
    }
    refresh(s,selected);
    if(ImGui::GetContentRegionAvail().x<1000)ImGui::TextDisabled(tr3D("%d traces | %zu samples | dt %.3f ms"),total,receivers.front().samples.size(),s.dt()*1000);
    else ImGui::TextDisabled(tr3D("%d traces | %zu samples | dt %.4f ms | raw peak %.2e m/s"),total,receivers.front().samples.size(),s.dt()*1000,rawPeak_);
    if(ImGui::IsItemHovered())ImGui::SetTooltip(tr3D("%d traces | %zu samples | dt %.4f ms | raw peak %.2e m/s"),total,receivers.front().samples.size(),s.dt()*1000,rawPeak_);
    if(settings.agc.enabled||settings.normalizeEach) {
        ImGui::TextColored(palette3D().accent,"%s",tr3D(settings.agc.enabled?"AGC preview; exported samples remain raw.":"Per-trace normalization; relative amplitudes are not comparable."));
    }
    if(ImGui::GetContentRegionAvail().y>=60){if(settings.view==0)section(s,selected);else single(s,selected);}
    fit_=false;ImGui::End();
}
}
