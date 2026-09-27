#include "OpenShadersWind.h"
#include "Settings.h"

#include <new>
#include <chrono>
#include <cmath>
#include <optional>

namespace OpenShadersWind {
    namespace {
        Interface* windInterface = nullptr;
    }

    void Connect() {
        Message message;
        const auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging || !messaging->Dispatch(InterfaceMessageType, &message, sizeof(message), "CommunityShaders") ||
            !message.GetApiFunction) {
            logger::info("Open Shaders wind provider is unavailable; using Dynamic Wind's procedural wind");
            return;
        }

        windInterface = static_cast<Interface*>(message.GetApiFunction(InterfaceRevision));
        if (windInterface)
            logger::info("Connected to Open Shaders wind API build {}", windInterface->getBuildNumber());
        else
            logger::info("Open Shaders does not provide wind API revision {}", InterfaceRevision);
    }

    bool IsAvailable() { return windInterface != nullptr; }

    bool IsEnabled() { return IsAvailable() && Config::GetSingleton()->OpenShadersIntegrationEnabled; }

    bool SamplePositions(std::span<const RE::NiPoint3> positions, std::vector<SampleWithHavokExclusion>& samples) {
        if (!IsEnabled() || positions.size() > MaximumBatchSize) return false;
        thread_local std::vector<Vector> apiPositions;
        try {
            samples.resize(positions.size());
            apiPositions.clear();
            apiPositions.reserve(positions.size());
        } catch (const std::bad_alloc&) {
            return false;
        }
        for (const auto& position : positions) apiPositions.push_back({position.x, position.y, position.z});
        const bool sampled = windInterface->SampleWindExcludingHavokImpulses(apiPositions.data(), samples.data(),
                                                                  static_cast<uint32_t>(apiPositions.size()));
        thread_local std::optional<bool> previousResult;
        thread_local std::chrono::steady_clock::time_point lastReport{};
        thread_local float peakTransient = 0.0f;
        thread_local bool hadTransient = false;
        bool hasTransient = false;
        if (sampled) {
            for (std::size_t index = 0; index < samples.size(); ++index) {
                const auto& sample = samples[index];
                const float strength = std::hypot(sample.wind.transientVelocity.x, sample.wind.transientVelocity.y,
                                                   sample.wind.transientVelocity.z);
                peakTransient = std::max(peakTransient, strength);
                hasTransient |= strength > 0.0f;
                if (!hadTransient && strength > 0.0f) {
                    logger::info("Open Shaders transient reached sample {} at ({}, {}, {}): visual=({}, {}, {}), windExcludingHavokImpulses=({}, {}, {})",
                                 index, positions[index].x, positions[index].y, positions[index].z,
                                 sample.wind.finalVelocity.x, sample.wind.finalVelocity.y, sample.wind.finalVelocity.z,
                                 sample.windExcludingHavokImpulses.x, sample.windExcludingHavokImpulses.y, sample.windExcludingHavokImpulses.z);
                }
            }
        }
        hadTransient = hasTransient;
        const auto now = std::chrono::steady_clock::now();
        if (previousResult != sampled || now - lastReport >= std::chrono::seconds(10)) {
            previousResult = sampled;
            lastReport = now;
            if (!sampled) {
                logger::warn("Open Shaders wind sampling failed for {} positions; using procedural fallback", positions.size());
            } else {
                float visualStrength = 0.0f;
                float windExcludingHavokImpulsesStrength = 0.0f;
                float transientStrength = 0.0f;
                for (const auto& sample : samples) {
                    visualStrength = std::max(visualStrength, std::hypot(sample.wind.finalVelocity.x, sample.wind.finalVelocity.y));
                    windExcludingHavokImpulsesStrength = std::max(windExcludingHavokImpulsesStrength, std::hypot(sample.windExcludingHavokImpulses.x, sample.windExcludingHavokImpulses.y));
                    transientStrength = std::max(transientStrength, std::hypot(sample.wind.transientVelocity.x, sample.wind.transientVelocity.y));
                }
                logger::info("Open Shaders wind sample: positions={}, frame={}, max visual={}, max windExcludingHavokImpulses={}, max transient={}, peak transient since report={}",
                             positions.size(), samples.empty() ? 0 : samples.front().wind.frameId,
                             visualStrength, windExcludingHavokImpulsesStrength, transientStrength, peakTransient);
            }
            peakTransient = 0.0f;
        }
        return sampled;
    }
}
