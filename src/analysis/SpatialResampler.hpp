#pragma once
#include <vector>
#include "../core/TelemetryData.hpp"

class SpatialResampler {
private:
    float Lerp(float a, float b, float t);
    double LerpDouble(double a, double b, float t);

    // Filtro asimmetrico a fase zero: permette di usare filtri diversi per X e Y
    void ApplyZeroPhaseEMA(std::vector<TelemetryFrame>& frames, float alphaX, float alphaY, float alphaZ);

public:
    std::vector<TelemetryFrame> Resample(const std::vector<IMUFrame>& imuData, const std::vector<GNSSFrame>& gnssData);
};