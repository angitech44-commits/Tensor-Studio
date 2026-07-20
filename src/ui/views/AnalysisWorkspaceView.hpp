#pragma once
#include "SessionSetupModal.hpp"
#include "../../core/Session.hpp"

class AnalysisWorkspaceView {
private:
    SessionSetupModal setupModal;

public:
    void Render(Session& session, bool& isProjectOpen, int& activeTab);
};