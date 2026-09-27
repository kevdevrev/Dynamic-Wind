#pragma once

#include "Utils.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

struct AnimationConfig {
    RE::FormID formID{0};
    float speedMin{0.0f};
    float speedMax{1.0f};
    float headingRotation{0.0f};
    float angleFactor{0.0f};
    std::string filePath;
};

class AnimationHandler{
public:
    AnimationHandler() {
        for (auto& cfg : LoadAllConfigs("Data\\SKSE\\Plugins\\DynamicWind\\Animation")) {
            _configs[cfg.formID] = cfg;
        }
        logger::info("Loaded {} Animation Configs", _configs.size());
    }

    std::unordered_map<RE::FormID, AnimationConfig>& GetConfigs() { return _configs; }

    bool HasConfig(RE::FormID formID) { return _configs.contains(formID); }

    void AddNewConfig(const RE::FormID formID, float speedMin, float speedMax, float headingRotation,
                      float angleFactor) {
        AnimationConfig cfg = {formID, speedMin, speedMax, headingRotation, angleFactor};
        _configs[cfg.formID] = cfg;

        SaveConfigToFile(cfg.formID, cfg);
    }

    void RemoveConfig(RE::FormID formID) {
        auto it = _configs.find(formID);
        if (it != _configs.end()) {
            std::string path = _configs[formID].filePath;
            _configs.erase(formID);
            if (std::filesystem::exists(path)) {
                std::filesystem::remove(path);
                logger::info("Removed AnimationConfig file for {:08X}", formID);
            }
        }
    }

    void Apply(RE::TESObjectREFR* ref, float windStrength, float windAngle, std::optional<float> windFieldDeltaTime = std::nullopt) {
        auto* node = ref->Get3D();
        if (!node) return;

        auto it = _configs.find(ref->GetFormID());
        if (it == _configs.end()) {
            it = _configs.find(ref->GetBaseObject()->GetFormID());
            if (it == _configs.end()) return;
        }

        const auto& cfg = it->second;

        float objectAngle = ref->GetAngle().z + cfg.headingRotation;
        float delta = windAngle + objectAngle;
        while (delta > M_PI) delta -= M_PI * 2.0f;
        while (delta < -M_PI) delta += M_PI * 2.0f;
        float alignment = std::cos(delta);

        float finalFactor = 1.0f;
        if (cfg.angleFactor > 0.0f) {
            finalFactor = 1.0f - cfg.angleFactor + (alignment * cfg.angleFactor);
        }

        float speed = cfg.speedMin + ((cfg.speedMax - cfg.speedMin) * windStrength * finalFactor);

        // Only sampled wind-field input uses smoothing; the original Dynamic Wind path applies speed directly.
        if (windFieldDeltaTime) ApplyWindFieldRate(ref, speed, *windFieldDeltaTime);
        else Utils::ApplySpeedToNode(node, speed);
    }

    // WindField update lifecycle, called around the framework's sampled animation batch.
    /// Marks responses for removal unless their reference is updated this frame.
    void BeginWindFieldUpdate() { for (auto& [id, state] : _windFieldAnimations) state.updated = false; }

    /// Restores authored rates for references no longer handled by the wind provider.
    void EndWindFieldUpdate() {
        for (auto it = _windFieldAnimations.begin(); it != _windFieldAnimations.end();) {
            if (!it->second.updated) {
                RestoreWindFieldRates(it->second);
                it = _windFieldAnimations.erase(it);
            } else ++it;
        }
    }

    /// Releases all controlled animations without restarting their clocks.
    void ClearWindFieldAnimations() {
        for (auto& [id, state] : _windFieldAnimations) RestoreWindFieldRates(state);
        _windFieldAnimations.clear();
    }

private:
    // Configuration file loading and persistence.
    bool LoadConfig(const std::string& path, AnimationConfig& outConfig) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j;
        file >> j;

        if (!j.contains("FormID")) {
            logger::error("file dont contain FormID");
            return false;
        }

        RE::FormID formID = Utils::ParseForm(j["FormID"].get<std::string>());
        if (formID == 0) {
            logger::error("Failed to parse FormID {} '{}'", j["FormID"].get<std::string>(), outConfig.formID);
            return false;
        }

        outConfig.formID = formID;
        if (outConfig.formID == 0) {
            logger::error("Failed to parse FormID {} '{}'", j["FormID"].get<std::string>(), outConfig.formID);
            return false;
        }
        if (j.contains("speedMin")) outConfig.speedMin = j["speedMin"].get<float>();
        if (j.contains("speedMax")) outConfig.speedMax = j["speedMax"].get<float>();
        if (j.contains("headingRotation")) outConfig.headingRotation = j["headingRotation"].get<float>();
        if (j.contains("angleFactor")) outConfig.angleFactor = j["angleFactor"].get<float>();
        outConfig.filePath = path;
        return true;
    }

    void SaveConfigToFile(const RE::FormID formID, const AnimationConfig& cfg) {
        std::filesystem::create_directories("Data\\SKSE\\Plugins\\DynamicWind\\Animation");
        std::string path = std::format("Data\\SKSE\\Plugins\\DynamicWind\\Animation\\{:08X}.json", formID);

            logger::info("Saving AnimationConfig for {:08X}", formID);
        
        nlohmann::json j;
        j["FormID"] = Utils::FormIDToString(formID);
        j["speedMin"] = cfg.speedMin;
        j["speedMax"] = cfg.speedMax;
        j["headingRotation"] = cfg.headingRotation;
        j["angleFactor"] = cfg.angleFactor;
        std::ofstream file(path);
        if (!file.is_open()) {
            logger::error("Failed to open file for writing: {}", path);
            return;
        }
        file << j.dump(4);
    }

    std::vector<AnimationConfig> LoadAllConfigs(const std::string& folder) {
        std::vector<AnimationConfig> configs;

        if (!std::filesystem::exists(folder) || !std::filesystem::is_directory(folder)) {
            logger::error("{} not exists or is not directory", folder);
            return configs;
        }

        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            if (entry.path().extension() != ".json") {
                continue;
            }

            logger::info("Reading {}", entry.path().string());

            AnimationConfig cfg;
            if (LoadConfig(entry.path().string(), cfg)) {
                configs.push_back(cfg);
            }
        }
        return configs;
    }

    // WindField playback smoothing and controller-rate restoration.
    static constexpr float WindFieldResponseTime = 0.08f;
    static constexpr float WindFieldMaximumTimeStep = 0.1f;
    static constexpr float WindFieldMinimumSynchronizedRate = 0.001f;

    template<class T> struct WindFieldOriginalRate {
        RE::NiPointer<T> object;
        float frequency;
    };
    struct WindFieldAnimationState {
        RE::NiPointer<RE::NiAVObject> root;
        std::vector<WindFieldOriginalRate<RE::NiControllerSequence>> sequences;
        std::vector<WindFieldOriginalRate<RE::NiTimeController>> controllers;
        float rate{};
        bool updated{};
    };

    /// Drives existing NIF sequences/controllers in place; no reference or mesh replacement.
    void ApplyWindFieldRate(RE::TESObjectREFR* ref, float rate, float deltaTime) {
        auto* root = ref->Get3D();
        if (!root || !std::isfinite(rate) || !std::isfinite(deltaTime) || deltaTime < 0.0f) return;
        auto& state = _windFieldAnimations[ref->GetFormID()];
        if (state.root.get() != root) {
            RestoreWindFieldRates(state);
            state = {};
            state.root = RE::NiPointer<RE::NiAVObject>(root);
            state.rate = rate;
        }
        state.updated = true;
        const float blend = -std::expm1(-std::min(deltaTime, WindFieldMaximumTimeStep) / WindFieldResponseTime);
        // Preserve signed rates so configured reverse playback also works with WindField smoothing.
        state.rate += (rate - state.rate) * blend;
        const auto visit = [&](auto&& self, RE::NiAVObject* node) -> void {
            if (!node) return;
            for (auto* controller = node->GetControllers(); controller; controller = controller->GetNext()) {
                if (auto* manager = controller->AsNiControllerManager()) {
                    for (auto* sequence : manager->activeSequences)
                        if (sequence) {
                            SetWindFieldRate(state.sequences, sequence, state.rate);
                            // Partner synchronization divides by frequency, so synchronized sequences cannot stop completely.
                            if (sequence->partnerSequence && std::abs(sequence->frequency) < WindFieldMinimumSynchronizedRate)
                                sequence->frequency = std::copysign(WindFieldMinimumSynchronizedRate, sequence->frequency);
                        }
                } else if (!controller->flags.any(RE::NiTimeController::Flag::kManagerControlled)) {
                    SetWindFieldRate(state.controllers, controller, state.rate);
                }
            }
            if (auto* parent = node->AsNode())
                for (auto& child : parent->GetChildren()) self(self, child.get());
        };
        visit(visit, root);
    }

    template<class T>
    static void SetWindFieldRate(std::vector<WindFieldOriginalRate<T>>& originals, T* object, float rate) {
        auto it = std::find_if(originals.begin(), originals.end(), [&](const auto& entry) { return entry.object.get() == object; });
        if (it == originals.end()) {
            originals.push_back({RE::NiPointer<T>(object), object->frequency});
        }
        // The engine integrates frequency into weightedLastTime; retaining its clock preserves phase.
        object->frequency = rate;
    }

    static void RestoreWindFieldRates(WindFieldAnimationState& state) {
        for (auto& entry : state.sequences) entry.object->frequency = entry.frequency;
        for (auto& entry : state.controllers) entry.object->frequency = entry.frequency;
    }

    // Saved configuration and active WindField animation state.
    std::unordered_map<RE::FormID, AnimationConfig> _configs;
    std::unordered_map<RE::FormID, WindFieldAnimationState> _windFieldAnimations;
};
