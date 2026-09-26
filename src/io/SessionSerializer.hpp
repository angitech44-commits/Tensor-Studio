#pragma once
#include <string>
#include "../core/Session.hpp"

class SessionSerializer {
public:
    // Nuova funzione per ottenere il percorso sicuro per le sessioni salvate
    static std::string GetSessionsDirectory();

    // Salva l'intera sessione, la pista e la telemetria in un file binario .tss
    static bool SaveSession(const Session& session, const std::string& filepath);
    
    // Carica il file .tss, ricrea la sessione e inietta la telemetria nella cache
    static bool LoadSession(const std::string& filepath, Session& outSession);
};