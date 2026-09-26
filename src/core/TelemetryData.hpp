#pragma once
#include <vector>

struct IMUFrame {
    double time; 
    float accX, accY, accZ;
    float gyroX, gyroY, gyroZ;
    float temp;
};

struct GNSSFrame {
    double time;
    double lat;  
    double lon;  
    float alt;
    float speed;
    float head;
    float hacc;
    float sacc;
    int sat;
    int fixType;
};

struct ErrorFrame {
    double time;
    int error;
};

struct TelemetryFrame {
    float time = 0.0f; // Il tempo normalizzato per la UI può stare in float (secondi)
    
    float accX = 0.0f, accY = 0.0f, accZ = 0.0f;
    float gyroX = 0.0f, gyroY = 0.0f, gyroZ = 0.0f;
    float temp = 0.0f;
    
    // CORRETTO: Devono essere a 64-bit per non distruggere le coordinate GPS
    double lat = 0.0, lon = 0.0; 
    
    float alt = 0.0f;
    float speed = 0.0f, head = 0.0f;
    float hacc = 0.0f, sacc = 0.0f;
    int sat = 0, fixType = 0;
};