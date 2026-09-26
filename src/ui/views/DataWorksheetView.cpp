#include "DataWorksheetView.hpp"
#include "../components/TelemetryImportModal.hpp" 
#include <imgui.h>
#include <algorithm>
#include <cstdio>

std::string DataWorksheetView::FormatTime(float seconds) {
    if (seconds <= 0) return "--.---";
    int m = (int)seconds / 60;
    float s = seconds - m * 60;
    char buf[32];
    if (m > 0) snprintf(buf, sizeof(buf), "%d:%06.3f", m, s);
    else snprintf(buf, sizeof(buf), "%.3f", s);
    return std::string(buf);
}

std::string DataWorksheetView::FormatGap(float seconds) {
    if (seconds <= 0.001f) return "-";
    char buf[32];
    snprintf(buf, sizeof(buf), "+%.3f", seconds);
    return std::string(buf);
}

std::string DataWorksheetView::FormatGapStr(const DriverResult& current, const DriverResult& leader, bool isRace) {
    if (isRace) {
        int lapDiff = leader.lapsCompleted - current.lapsCompleted;
        if (lapDiff > 0) return "+" + std::to_string(lapDiff) + " Laps";
        else return FormatGap(current.totalTime - leader.totalTime);
    } else {
        if (current.bestLap == 0.0f || leader.bestLap == 0.0f) return "N/A";
        return FormatGap(current.bestLap - leader.bestLap);
    }
}

void DataWorksheetView::Render(Session& session, const std::vector<SessionSplit>& splits, int& activeSplitIndex, std::vector<std::string>& openGraphLayouts) {
    float scale = ImGui::GetIO().FontGlobalScale;

    if (splits.empty()) {
        ImGui::TextDisabled("No splits configured for this session.");
        return;
    }

    // --- 1. CHILD WINDOW PER I DATI SCROLLABILI ---
    // Lasciamo uno spazio fisso di 60px in basso per la barra con il bottone
    ImGui::BeginChild("DataScrollRegion", ImVec2(0, -60.0f * scale), false, ImGuiWindowFlags_None);

    if (splits.size() > 1) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        
        if (ImGui::BeginChild("SplitNavBar", ImVec2(0, 65.0f * scale), true, ImGuiWindowFlags_NoScrollbar)) {
            float totalWidth = ImGui::GetContentRegionAvail().x;
            float spacing = 10.0f * scale;
            
            float maxButtonWidth = 250.0f * scale;
            float totalButtonsWidth = (maxButtonWidth * splits.size()) + (spacing * (splits.size() - 1));
            float buttonWidth = maxButtonWidth;

            if (totalButtonsWidth > totalWidth - (20.0f * scale)) {
                buttonWidth = (totalWidth - (20.0f * scale) - (spacing * (splits.size() - 1))) / splits.size();
                totalButtonsWidth = (buttonWidth * splits.size()) + (spacing * (splits.size() - 1));
            }
            
            float startX = (totalWidth - totalButtonsWidth) * 0.5f;
            ImGui::SetCursorPos(ImVec2(startX, 12.0f * scale));
            
            for (size_t i = 0; i < splits.size(); ++i) {
                std::string typeStr = (splits[i].type == SplitType::FreePractice) ? "Free Practice" : 
                                      (splits[i].type == SplitType::Qualifying) ? "Qualifying" : "Race";
                std::string splitName = "Split " + std::to_string(i + 1) + " : " + typeStr;
                
                bool isActive = (activeSplitIndex == i);
                if (isActive) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.4f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.45f, 1.0f));
                }
                
                if (ImGui::Button(splitName.c_str(), ImVec2(buttonWidth, 40.0f * scale))) {
                    activeSplitIndex = i;
                }
                ImGui::PopStyleColor(3);
                ImGui::SameLine(0, spacing);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 10.0f * scale));
    }
    
    if (activeSplitIndex >= splits.size()) activeSplitIndex = 0;
    const auto& activeSplit = splits[activeSplitIndex];
    bool isRace = (activeSplit.type == SplitType::Race);
    
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "SESSION RESULTS  |  REFERENCE LAP %d TO %d", activeSplit.startLap, activeSplit.endLap);
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10.0f * scale));

    std::vector<DriverResult> results;
    for (const auto& d : session.drivers) {
        DriverResult r;
        r.name = d.name;
        r.bestLap = 99999.0f;
        r.bestS1 = 99999.0f;
        r.bestS2 = 99999.0f;
        r.bestS3 = 99999.0f;

        if (!d.telemetryPath.empty() && GlobalTelemetryCache.count(d.telemetryPath)) {
            const auto& cache = GlobalTelemetryCache[d.telemetryPath];
            
            for (const auto& lap : cache.laps) {
                double lapAbsCenter = cache.baseTime + ((lap.startTime + lap.endTime) / 2.0);
                
                if (lapAbsCenter >= activeSplit.absoluteStartTime && lapAbsCenter <= activeSplit.absoluteEndTime) {
                    r.lapsCompleted++;
                    r.totalTime += lap.timeSeconds;
                    if (lap.timeSeconds > 0 && lap.timeSeconds < r.bestLap) r.bestLap = lap.timeSeconds;
                    if (lap.sectorTimes.size() > 0 && lap.sectorTimes[0] > 0 && lap.sectorTimes[0] < r.bestS1) r.bestS1 = lap.sectorTimes[0];
                    if (lap.sectorTimes.size() > 1 && lap.sectorTimes[1] > 0 && lap.sectorTimes[1] < r.bestS2) r.bestS2 = lap.sectorTimes[1];
                    if (lap.sectorTimes.size() > 2 && lap.sectorTimes[2] > 0 && lap.sectorTimes[2] < r.bestS3) r.bestS3 = lap.sectorTimes[2];
                }
            }
        }
        
        if (r.bestLap == 99999.0f) r.bestLap = 0.0f;
        if (r.bestS1 == 99999.0f) r.bestS1 = 0.0f;
        if (r.bestS2 == 99999.0f) r.bestS2 = 0.0f;
        if (r.bestS3 == 99999.0f) r.bestS3 = 0.0f;

        results.push_back(r);
    }

    if (isRace) {
        std::sort(results.begin(), results.end(), [](const auto& a, const auto& b){ 
            if (a.lapsCompleted != b.lapsCompleted) return a.lapsCompleted > b.lapsCompleted;
            return a.totalTime < b.totalTime; 
        });
    } else {
        std::sort(results.begin(), results.end(), [](const auto& a, const auto& b){ 
            if (a.bestLap == 0.0f) return false;
            if (b.bestLap == 0.0f) return true;
            return a.bestLap < b.bestLap; 
        });
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    if (ImGui::BeginChild("PodiumArea", ImVec2(0, 150.0f * scale), true)) {
        
        float availW = ImGui::GetContentRegionAvail().x;
        float boxW = 220.0f * scale;
        float cx = availW / 2.0f;
        
        auto drawPodiumBox = [&](const DriverResult& driver, int pos, float xPos, float yPos, ImU32 color, float h) {
            ImGui::SetCursorPos(ImVec2(xPos, yPos));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(color));
            if (ImGui::BeginChild((std::string("P") + std::to_string(pos)).c_str(), ImVec2(boxW, h), true, ImGuiWindowFlags_NoScrollbar)) {
                ImGui::SetCursorPosY(10.0f * scale);
                std::string pStr = "P" + std::to_string(pos);
                ImGui::SetWindowFontScale(1.8f);
                float tW = ImGui::CalcTextSize(pStr.c_str()).x;
                ImGui::SetCursorPosX((boxW - tW) * 0.5f);
                ImGui::Text("%s", pStr.c_str());
                ImGui::SetWindowFontScale(1.0f);

                float nameW = ImGui::CalcTextSize(driver.name.c_str()).x;
                ImGui::SetCursorPosX((boxW - nameW) * 0.5f);
                ImGui::TextDisabled("%s", driver.name.c_str());
                
                std::string timeStr = isRace ? FormatTime(driver.totalTime) : FormatTime(driver.bestLap);
                float timeW = ImGui::CalcTextSize(timeStr.c_str()).x;
                ImGui::SetCursorPosX((boxW - timeW) * 0.5f);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", timeStr.c_str());
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        };

        if (results.size() > 1) drawPodiumBox(results[1], 2, cx - boxW * 1.5f - 15.0f * scale, 35.0f * scale, IM_COL32(50, 50, 55, 255), 105.0f * scale);
        if (results.size() > 0) drawPodiumBox(results[0], 1, cx - boxW * 0.5f, 15.0f * scale, IM_COL32(70, 60, 20, 255), 125.0f * scale); 
        if (results.size() > 2) drawPodiumBox(results[2], 3, cx + boxW * 0.5f + 15.0f * scale, 55.0f * scale, IM_COL32(55, 40, 30, 255), 85.0f * scale); 
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    
    ImGui::Dummy(ImVec2(0, 10.0f * scale));

    int cols = isRace ? 9 : 8; 
    if (ImGui::BeginTable("ResultsTable", cols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Pos", ImGuiTableColumnFlags_WidthFixed, 40.0f * scale);
        ImGui::TableSetupColumn("Driver", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Laps", ImGuiTableColumnFlags_WidthFixed, 60.0f * scale);
        if (isRace) ImGui::TableSetupColumn("Total Time", ImGuiTableColumnFlags_WidthFixed, 100.0f * scale);
        ImGui::TableSetupColumn("Gap", ImGuiTableColumnFlags_WidthFixed, 80.0f * scale);
        ImGui::TableSetupColumn("Best Lap", ImGuiTableColumnFlags_WidthFixed, 100.0f * scale);
        ImGui::TableSetupColumn("Best S1", ImGuiTableColumnFlags_WidthFixed, 70.0f * scale);
        ImGui::TableSetupColumn("Best S2", ImGuiTableColumnFlags_WidthFixed, 70.0f * scale);
        ImGui::TableSetupColumn("Best S3", ImGuiTableColumnFlags_WidthFixed, 70.0f * scale);
        ImGui::TableHeadersRow();

        for(size_t r = 0; r < results.size(); ++r) {
            ImGui::TableNextRow();
            int colIdx = 0;
            
            ImGui::TableSetColumnIndex(colIdx++);
            ImGui::Text("%zu", r + 1);
            
            ImGui::TableSetColumnIndex(colIdx++);
            ImGui::Text("%s", results[r].name.c_str());

            ImGui::TableSetColumnIndex(colIdx++);
            ImGui::Text("%d", results[r].lapsCompleted);

            if (isRace) {
                ImGui::TableSetColumnIndex(colIdx++);
                if (results[r].totalTime > 0.0f) {
                    ImGui::Text("%s", FormatTime(results[r].totalTime).c_str());
                } else {
                    ImGui::Text("DNF");
                }
            }

            ImGui::TableSetColumnIndex(colIdx++);
            if (!results.empty() && results[r].lapsCompleted > 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "%s", FormatGapStr(results[r], results[0], isRace).c_str());
            } else {
                ImGui::Text("-");
            }
            
            ImGui::TableSetColumnIndex(colIdx++);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", FormatTime(results[r].bestLap).c_str());
            
            ImGui::TableSetColumnIndex(colIdx++);
            ImGui::Text("%s", FormatTime(results[r].bestS1).c_str());
            
            ImGui::TableSetColumnIndex(colIdx++);
            ImGui::Text("%s", FormatTime(results[r].bestS2).c_str());
            
            ImGui::TableSetColumnIndex(colIdx++);
            ImGui::Text("%s", FormatTime(results[r].bestS3).c_str());
        }
        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0, 30.0f * scale));
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "LAP BY LAP ANALYSIS");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10.0f * scale));

    for (const auto& d : session.drivers) {
        if (d.telemetryPath.empty() || GlobalTelemetryCache.find(d.telemetryPath) == GlobalTelemetryCache.end()) continue;
        
        const auto& cache = GlobalTelemetryCache[d.telemetryPath];
        std::vector<ParsedLap> validLaps;
        float personalBest = 99999.0f;
        
        for (const auto& lap : cache.laps) {
            double lapAbsCenter = cache.baseTime + ((lap.startTime + lap.endTime) / 2.0);
            if (lapAbsCenter >= activeSplit.absoluteStartTime && lapAbsCenter <= activeSplit.absoluteEndTime) {
                validLaps.push_back(lap);
                if (lap.timeSeconds > 0 && lap.timeSeconds < personalBest) {
                    personalBest = lap.timeSeconds;
                }
            }
        }
        
        if (validLaps.empty()) continue;
        
        std::string selKey = std::string(session.sessionName) + "::" + d.name;
        
        bool allSelected = true;
        for (const auto& lap : validLaps) {
            if (!GlobalLapSelection[selKey][lap.lapNumber]) {
                allSelected = false;
                break;
            }
        }
        
        ImGui::PushID(d.name.c_str());
        bool masterToggle = allSelected;
        
        if (ImGui::Checkbox("##master", &masterToggle)) {
            for (const auto& lap : validLaps) {
                GlobalLapSelection[selKey][lap.lapNumber] = masterToggle;
            }
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", d.name.c_str());
        
        if (ImGui::BeginTable(("LapTable_" + d.name).c_str(), 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(0, 250.0f * scale))) {
            ImGui::TableSetupScrollFreeze(1, 1); 
            ImGui::TableSetupColumn("Lap", ImGuiTableColumnFlags_WidthFixed, 60.0f * scale);
            ImGui::TableSetupColumn("Lap Time", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Gap to PB", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Sector 1", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Sector 2", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Sector 3", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            
            for (const auto& lap : validLaps) {
                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                bool isSel = GlobalLapSelection[selKey][lap.lapNumber];
                if (ImGui::Checkbox((std::to_string(lap.lapNumber) + "##sel").c_str(), &isSel)) {
                    GlobalLapSelection[selKey][lap.lapNumber] = isSel;
                }
                
                ImGui::TableSetColumnIndex(1);
                if (lap.timeSeconds == personalBest) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.8f, 1.0f), "%s", FormatTime(lap.timeSeconds).c_str()); 
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", FormatTime(lap.timeSeconds).c_str());
                }
                
                ImGui::TableSetColumnIndex(2);
                if (lap.timeSeconds == personalBest) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.8f, 1.0f), "PB");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "%s", FormatGap(lap.timeSeconds - personalBest).c_str());
                }
                
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", lap.sectorTimes.size() > 0 ? FormatTime(lap.sectorTimes[0]).c_str() : "-");
                
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%s", lap.sectorTimes.size() > 1 ? FormatTime(lap.sectorTimes[1]).c_str() : "-");
                
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%s", lap.sectorTimes.size() > 2 ? FormatTime(lap.sectorTimes[2]).c_str() : "-");
            }
            ImGui::EndTable();
        }
        ImGui::PopID();
        ImGui::Dummy(ImVec2(0, 15.0f * scale));
    }
    ImGui::EndChild(); // Chiude la zona scrollabile

    // --- 2. BARRA FISSA IN BASSO PER IL BOTTONE ---
    ImGui::Dummy(ImVec2(0, 10.0f * scale));
    
    float btnWidth = 160.0f * scale;
    float btnHeight = 40.0f * scale;
    
    // Lo allineiamo a destra
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - btnWidth - 20.0f * scale);
    
    // Bottone pulito, senza override di colore, con tema nativo
    if (ImGui::Button("Graph Analysis", ImVec2(btnWidth, btnHeight))) {
        ImGui::OpenPopup("GraphLayoutPopup");
    }

    // Posizioniamo il popup esattamente sopra il bottone
    // Posizioniamo il popup esattamente sopra il bottone
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y - 130.0f * scale), ImGuiCond_Appearing); 
    if (ImGui::BeginPopup("GraphLayoutPopup")) {
        ImGui::TextDisabled("PREDEFINED LAYOUTS");
        ImGui::Separator();
        
        // ORA CHIAMIAMO IL LAYOUT REALE
        if (ImGui::Selectable("Lap Comparison")) {
            if (std::find(openGraphLayouts.begin(), openGraphLayouts.end(), "Lap Comparison") == openGraphLayouts.end()) {
                openGraphLayouts.push_back("Lap Comparison");
            }
        }
        
        ImGui::Separator();
        ImGui::TextDisabled("USER LAYOUTS");
        
        if (ImGui::Selectable("+ Create New Layout...")) {
            // Setup future user layout 
        }
        
        ImGui::EndPopup();
    }
    
    // --- 3. NATIVE OS WINDOW RENDERER ---
    for (size_t i = 0; i < openGraphLayouts.size(); ++i) {
        bool isWindowOpen = true;
        std::string wndTitle = openGraphLayouts[i] + " - " + session.sessionName + "###GraphLayout" + std::to_string(i);
        
        // Creiamo la finestra staccabile per il grafico
        ImGui::SetNextWindowSize(ImVec2(1000.0f * scale, 700.0f * scale), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(wndTitle.c_str(), &isWindowOpen)) {
            
            // Smistiamo il rendering in base al nome del layout richiesto
            if (openGraphLayouts[i] == "Lap Comparison") {
                // IL PASSAGGIO DEL NULLPTR È IMPORTANTE, il renderer SDL non ci serve più qui su desktop
                lapCompLayout.Render(session, nullptr);
            } else {
                ImGui::Text("Layout %s not implemented yet.", openGraphLayouts[i].c_str());
            }

        }
        ImGui::End();

        if (!isWindowOpen) {
            openGraphLayouts.erase(openGraphLayouts.begin() + i);
            i--;
        }
    }
}