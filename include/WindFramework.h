#pragma once

#include "Settings.h"

#include "AnimationHandler.h"
#include "BaseObjSwapHandler.h"
#include "ModelSwapHandler.h"
#include "PushHandler.h"
#include "RotationHandler.h"
#include "TreeHandler.h"
#include "VisibilityHandler.h"
#include "OpenShadersWind.h"

#include "REX/REX.h"

#include <unordered_set>
#include <shared_mutex>
#include <optional>

struct WindObjectConfigs {
    const AnimationConfig* animationConfig = nullptr;
    const BaseObjSwapConfig* baseObjSwapConfig = nullptr;
    const ModelSwapConfig* modelSwapConfig = nullptr;
    const PushConfig* pushConfig = nullptr;
    const RotationConfig* rotationConfig = nullptr;
    const TreeConfig* treeConfig = nullptr;
    const VisibilityConfig* visibilityConfig = nullptr;
};

class WindFramework : public REX::Singleton<WindFramework> {
public:
    void Update(float strength, float angle, float deltaTime) {
        auto* conf = Config::GetSingleton();
        std::unique_lock lock(mutex_);

        const bool useOpenShaders = OpenShadersWind::IsEnabled();
        if (useOpenShaders) {
            _samplePositions.clear();
            _sampleIndices.clear();
            const auto addSamples = [&](const auto& refs) {
                for (const auto& [formID, handle] : refs) {
                    const auto ref = handle.get();
                    if (ref && !ref->IsDisabled() && !ref->IsDeleted() && !_sampleIndices.contains(formID)) {
                        _sampleIndices.emplace(formID, _samplePositions.size());
                        _samplePositions.push_back(ref->GetPosition());
                    }
                }
            };
            if (conf->RotationHandlerEnabled) addSamples(_trackedRotationRefs);
            if (conf->AnimationHandlerEnabled) addSamples(_trackedAnimationRefs);
            if (conf->VisibilityHandlerEnabled) addSamples(_trackedVisibilityRefs);
            if (conf->PushHandlerEnabled) addSamples(_trackedPushRefs);
            if (_pendingSwapTargets) {
                if (conf->ModelSwapHandlerEnabled) addSamples(_trackedModelSwapRefs);
                if (conf->BaseObjSwapHandlerEnabled) addSamples(_trackedBaseObjSwapRefs);
            }
        }

        const bool sampledOpenShaders = useOpenShaders && OpenShadersWind::SamplePositions(_samplePositions, _windSamples);
        if (sampledOpenShaders && !_reportedWindTargets && _samplePositions.size() > 1) {
            _reportedWindTargets = true;
            for (const auto& [formID, index] : _sampleIndices) {
                const auto& position = _samplePositions[index];
                logger::info("Open Shaders sample {}: ref={:08X}, position=({}, {}, {}), animation={}, rotation={}, visibility={}, push={}, modelSwap={}, baseSwap={}",
                             index, formID, position.x, position.y, position.z,
                             _trackedAnimationRefs.contains(formID), _trackedRotationRefs.contains(formID),
                             _trackedVisibilityRefs.contains(formID), _trackedPushRefs.contains(formID),
                             _trackedModelSwapRefs.contains(formID), _trackedBaseObjSwapRefs.contains(formID));
            }
        }
        if (!sampledOpenShaders) _rotationWind.clear();
        if (sampledOpenShaders) animationHandler_.BeginWindFieldUpdate();
        else animationHandler_.ClearWindFieldAnimations();
        enum class WindSampleUse { Visual, ExcludingHavokImpulses, Ambient };
        const auto resolveWind = [&](const RE::TESObjectREFR* ref, WindSampleUse use) {
            OpenShadersWind::Vector velocity{
                strength * std::cos(angle), strength * std::sin(angle), 0.0f };
            if (sampledOpenShaders) {
                const auto sample = _sampleIndices.find(ref->GetFormID());
                if (sample != _sampleIndices.end()) {
                    const auto& value = _windSamples[sample->second];
                    if (use == WindSampleUse::Ambient) {
                        velocity = {value.wind.baseVelocity.x + value.wind.gustVelocity.x,
                                    value.wind.baseVelocity.y + value.wind.gustVelocity.y,
                                    value.wind.baseVelocity.z + value.wind.gustVelocity.z};
                    } else {
                        velocity = use == WindSampleUse::ExcludingHavokImpulses ? value.windExcludingHavokImpulses : value.wind.finalVelocity;
                    }
                }
            }
            const float velocityLength = std::hypot(velocity.x, velocity.y);
            const float localStrength = std::clamp(velocityLength, 0.0f, 1.0f);
            const float localAngle = velocityLength > 0.0001f ? std::atan2(velocity.y, velocity.x) : angle;
            return std::tuple{ localStrength, localAngle, velocity };
        };
        if (conf->RotationHandlerEnabled) {
            for (auto it = _trackedRotationRefs.begin(); it != _trackedRotationRefs.end();) {
                auto ref = it->second.get().get();
                if (!ref) {
                    _rotationWind.erase(it->first);
                    it = _trackedRotationRefs.erase(it);
                } else {
                    if (ref->IsDisabled() || ref->IsDeleted()) {
                        _rotationWind.erase(it->first);
                        it = _trackedRotationRefs.erase(it);
                    } else {
                        if (!sampledOpenShaders) {
                            rotationHandler_.Apply(ref, strength, angle);
                            ++it;
                            continue;
                        }
                        const auto wind = resolveWind(ref, WindSampleUse::Ambient);
                        const auto localAngle = std::get<1>(wind);
                        const auto velocity = std::get<2>(wind);
                        auto [filtered, inserted] = _rotationWind.try_emplace(ref->GetFormID(), velocity);
                        if (!inserted) {
                            const float blend = 1.0f - std::exp(-4.0f * std::clamp(deltaTime, 0.0f, 0.1f));
                            filtered->second.x += (velocity.x - filtered->second.x) * blend;
                            filtered->second.y += (velocity.y - filtered->second.y) * blend;
                        }
                        const float filteredStrength = std::hypot(filtered->second.x, filtered->second.y);
                        const float filteredAngle = filteredStrength > 0.0001f ?
                                                        std::atan2(filtered->second.y, filtered->second.x) :
                                                        localAngle;
                        rotationHandler_.Apply(ref, std::clamp(filteredStrength, 0.0f, 1.0f), filteredAngle);
                        ++it;
                    }
                }
            }
        }
        if (conf->AnimationHandlerEnabled) {
            for (auto it = _trackedAnimationRefs.begin(); it != _trackedAnimationRefs.end();) {
                auto ref = it->second.get().get();
                if (!ref) {
                    it = _trackedAnimationRefs.erase(it);
                } else {
                    if (ref->IsDisabled() || ref->IsDeleted()) {
                        it = _trackedAnimationRefs.erase(it);
                    } else {
                        if (sampledOpenShaders) {
                            const auto wind = resolveWind(ref, WindSampleUse::Visual);
                            animationHandler_.Apply(ref, std::get<0>(wind), std::get<1>(wind), deltaTime);
                        } else {
                            animationHandler_.Apply(ref, strength, angle);
                        }
                        ++it;
                    }
                }
            }
        }
        if (conf->VisibilityHandlerEnabled) {
            for (auto it = _trackedVisibilityRefs.begin(); it != _trackedVisibilityRefs.end();) {
                auto ref = it->second.get().get();
                if (!ref) {
                    it = _trackedVisibilityRefs.erase(it);
                } else {
                    if (ref->IsDisabled() || ref->IsDeleted()) {
                        it = _trackedVisibilityRefs.erase(it);
                    } else {
                        if (sampledOpenShaders) {
                            const auto wind = resolveWind(ref, WindSampleUse::Ambient);
                            visibilityHandler_.Apply(ref, std::get<0>(wind), std::get<1>(wind));
                        } else {
                            visibilityHandler_.Apply(ref, strength, angle);
                        }
                        ++it;
                    }
                }
            }
        }
        if (conf->PushHandlerEnabled) {
            for (auto it = _trackedPushRefs.begin(); it != _trackedPushRefs.end();) {
                auto ref = it->second.get().get();
                if (!ref) {
                    it = _trackedPushRefs.erase(it);
                } else {
                    if (ref->IsDisabled() || ref->IsDeleted()) {
                        it = _trackedPushRefs.erase(it);
                    } else {
                        if (sampledOpenShaders) {
                            const auto wind = resolveWind(ref, WindSampleUse::ExcludingHavokImpulses);
                            pushHandler_.Apply(ref, std::get<0>(wind), std::get<1>(wind));
                        } else {
                            pushHandler_.Apply(ref, strength, angle);
                        }
                        ++it;
                    }
                }
            }
        }
        if (_pendingSwapTargets) {
            // Failed sampling must still apply this target using Dynamic Wind's fallback values.
            bool ready = !sampledOpenShaders;
            if (sampledOpenShaders && !_windSamples.empty()) {
                const auto frame = _windSamples.front().wind.frameId;
                if (!_pendingSwapFrame) _pendingSwapFrame = frame;
                else ready = frame != *_pendingSwapFrame;
            }
            if (ready) {
                const auto applyTargets = [&](auto& refs, auto& handler) {
                    for (auto it = refs.begin(); it != refs.end();) {
                        const auto ref = it->second.get();
                        if (!ref || ref->IsDisabled() || ref->IsDeleted()) {
                            it = refs.erase(it);
                        } else {
                            const auto wind = resolveWind(ref.get(), WindSampleUse::Ambient);
                            handler.Apply(ref.get(), std::get<0>(wind), std::get<1>(wind));
                            ++it;
                        }
                    }
                };
                if (conf->ModelSwapHandlerEnabled) applyTargets(_trackedModelSwapRefs, modelSwapHandler_);
                if (conf->BaseObjSwapHandlerEnabled) applyTargets(_trackedBaseObjSwapRefs, baseObjSwapHandler_);
                _pendingSwapTargets = false;
                _pendingSwapFrame.reset();
            }
        }
        if (sampledOpenShaders) animationHandler_.EndWindFieldUpdate();
    }

    void NewTargets(float strength, float angle) {
        auto* conf = Config::GetSingleton();

        std::unique_lock lock(mutex_);

        // Dynamic Wind tree writes would override the parameters and direction used by Open Shaders.
        if (conf->TreeHandlerEnabled && !OpenShadersWind::IsEnabled()) {
            treeHandler_.Update(strength, angle);
        }
        if (conf->RotationHandlerEnabled) {
            for (auto it = _trackedRotationRefs.begin(); it != _trackedRotationRefs.end();) {
                auto ref = it->second.get().get();
                if (!ref) {
                    it = _trackedRotationRefs.erase(it);
                } else {
                    if (ref->IsDisabled() || ref->IsDeleted()) {
                        it = _trackedRotationRefs.erase(it);
                    } else {
                        rotationHandler_.Apply(ref, strength, angle, true);
                        ++it;
                    }
                }
            }
        }

        if (OpenShadersWind::IsEnabled()) {
            // Sky is written after this callback; wait past the first subsequent sample to avoid the old field.
            _pendingSwapTargets = true;
            _pendingSwapFrame.reset();
            return;
        }
        _pendingSwapTargets = false;
        _pendingSwapFrame.reset();

        if (conf->ModelSwapHandlerEnabled) {
            for (auto it = _trackedModelSwapRefs.begin(); it != _trackedModelSwapRefs.end();) {
                auto ref = it->second.get().get();
                if (!ref) {
                    it = _trackedModelSwapRefs.erase(it);
                } else {
                    if (ref->IsDisabled() || ref->IsDeleted()) {
                        it = _trackedModelSwapRefs.erase(it);
                    } else {
                        modelSwapHandler_.Apply(ref, strength, angle);
                        ++it;
                    }
                }
            }
        }

        if (conf->BaseObjSwapHandlerEnabled) {
            for (auto it = _trackedBaseObjSwapRefs.begin(); it != _trackedBaseObjSwapRefs.end();) {
                auto ref = it->second.get().get();
                if (!ref) {
                    it = _trackedBaseObjSwapRefs.erase(it);
                } else {
                    if (ref->IsDisabled() || ref->IsDeleted()) {
                        it = _trackedBaseObjSwapRefs.erase(it);
                    } else {
                        baseObjSwapHandler_.Apply(ref, strength, angle);
                        ++it;
                    }
                }
            }
        }
    }

    void DisableMod() {
        std::unique_lock lock(mutex_);
        animationHandler_.ClearWindFieldAnimations();
        _pendingSwapTargets = false;
        _pendingSwapFrame.reset();
        // Resetting Dynamic Wind must not overwrite tree values while Open Shaders controls them.
        if (!OpenShadersWind::IsEnabled()) treeHandler_.Update(0, 0);
    }

    void RefLoad(RE::TESObjectREFR* ref, float angle = 0.0f, float strength = 0.0f) {
        if (!ref) return;

        if (!Utils::IsInWindCell(ref)) return;

        auto baseObj = ref->GetBaseObject();
        if (!baseObj) return;

        auto baseFormID = baseObj->GetFormID();
        auto formID = ref->GetFormID();

        auto* conf = Config::GetSingleton();

        std::unique_lock lock(mutex_);
        if (conf->AnimationHandlerEnabled && (animationHandler_.HasConfig(baseFormID) || animationHandler_.HasConfig(formID))) {
            _trackedAnimationRefs.try_emplace(formID, ref->GetHandle());
        } else if (conf->RotationHandlerEnabled && (rotationHandler_.HasConfig(baseFormID) || rotationHandler_.HasConfig(formID))) {
            if (_trackedRotationRefs.try_emplace(formID, ref->GetHandle()).second) {
                rotationHandler_.Apply(ref, strength, angle, true);
            }
        } else if (conf->VisibilityHandlerEnabled && (visibilityHandler_.HasConfig(baseFormID) || visibilityHandler_.HasConfig(formID))) {
            _trackedVisibilityRefs.try_emplace(formID, ref->GetHandle());
        } else if (conf->PushHandlerEnabled && (pushHandler_.HasConfig(baseFormID) || pushHandler_.HasConfig(formID))) {
            _trackedPushRefs.try_emplace(formID, ref->GetHandle());
        } else if (conf->ModelSwapHandlerEnabled && (modelSwapHandler_.HasConfig(baseFormID) || modelSwapHandler_.HasConfig(formID))) {
            if (_trackedModelSwapRefs.try_emplace(formID, ref->GetHandle()).second && !_pendingSwapTargets) {
                const auto [localStrength, localAngle] = GetSwapWind(ref, strength, angle);
                modelSwapHandler_.Apply(ref, localStrength, localAngle);
            }
        } else if (conf->BaseObjSwapHandlerEnabled && (baseObjSwapHandler_.HasConfig(baseFormID) || baseObjSwapHandler_.HasConfig(formID))) {
            if (_trackedBaseObjSwapRefs.try_emplace(formID, ref->GetHandle()).second && !_pendingSwapTargets) {
                const auto [localStrength, localAngle] = GetSwapWind(ref, strength, angle);
                baseObjSwapHandler_.Apply(ref, localStrength, localAngle);
            }
        }

        // --- other object types TBD ---
    }

    std::map<RE::FormID, WindObjectConfigs> GetConfigs() {
        std::map<RE::FormID, WindObjectConfigs> result;

        
        std::shared_lock lock(mutex_);
        // Animation
        for (const auto& [formID, cfg] : animationHandler_.GetConfigs()) {
            result[formID].animationConfig = &cfg;
        }
        // BaseObjSwap
        for (const auto& [formID, cfg] : baseObjSwapHandler_.GetConfigs()) {
            result[formID].baseObjSwapConfig = &cfg;
        }
        // ModelSwap
        for (const auto& [formID, cfg] : modelSwapHandler_.GetConfigs()) {
            result[formID].modelSwapConfig = &cfg;
        }
        // Push
        for (const auto& [formID, cfg] : pushHandler_.GetConfigs()) {
            result[formID].pushConfig = &cfg;
        }
        // Rotation
        for (const auto& [formID, cfg] : rotationHandler_.GetConfigs()) {
            result[formID].rotationConfig = &cfg;
        }
        // Tree
        for (const auto& [formID, cfg] : treeHandler_.GetConfigs()) {
            result[formID].treeConfig = &cfg;
        }
        // Visibility
        for (const auto& [formID, cfg] : visibilityHandler_.GetConfigs()) {
            result[formID].visibilityConfig = &cfg;
        }

        return result;
    }

    void AddNewTreeConfig(const RE::TESObjectTREE* tree, const TreeDataConfig& cfg) {
        treeHandler_.AddNewConfig(tree, cfg);
    }
    TreeConfig GetTreeConfig(const RE::FormID formID) { return treeHandler_.GetConfig(formID); }
    bool HasTreeConfig(const RE::FormID formID) { return treeHandler_.HasConfig(formID); }
    void RemoveTreeConfig(const RE::FormID formID) { treeHandler_.RemoveConfig(formID); }

    void AddNewAnimationConfig(const RE::FormID formID, float minAnimSpeed, float maxAnimSpeed, float headingRotation,
                               float angleFactor) {
        animationHandler_.AddNewConfig(formID, minAnimSpeed, maxAnimSpeed, headingRotation, angleFactor);
    }
    void AddNewAnimationConfig(const RE::TESBoundObject* obj, float minAnimSpeed, float maxAnimSpeed,
                               float headingRotation, float angleFactor) {
        animationHandler_.AddNewConfig(obj->GetFormID(), minAnimSpeed, maxAnimSpeed, headingRotation, angleFactor);
    }
    void AddNewAnimationConfig(const RE::TESObjectREFR* ref, float minAnimSpeed, float maxAnimSpeed,
                               float headingRotation, float angleFactor) {
        if (!ref->IsDynamicForm()) {
            animationHandler_.AddNewConfig(ref->GetFormID(), minAnimSpeed, maxAnimSpeed, headingRotation, angleFactor);
        }
    }
    void RemoveAnimationConfig(const RE::FormID formID) { animationHandler_.RemoveConfig(formID); }

    void AddNewRotationConfig(const RE::FormID formID, float headingRotation,
                              std::optional<std::vector<float>> allowedAngles = std::nullopt) {
        rotationHandler_.AddNewConfig(formID, headingRotation, allowedAngles);
    }
    void AddNewRotationConfig(const RE::TESBoundObject* obj, float headingRotation,
                              std::optional<std::vector<float>> allowedAngles = std::nullopt) {
        rotationHandler_.AddNewConfig(obj->GetFormID(), headingRotation, allowedAngles);
    }
    void AddNewRotationConfig(const RE::TESObjectREFR* ref, float headingRotation,
                              std::optional<std::vector<float>> allowedAngles = std::nullopt) {
        if (!ref->IsDynamicForm()) {
            rotationHandler_.AddNewConfig(ref->GetFormID(), headingRotation, allowedAngles);
        }
    }
    void RemoveRotationConfig(const RE::FormID formID) { rotationHandler_.RemoveConfig(formID); }

    void AddNewVisibilityConfig(const RE::FormID formID, float minVisibility, float maxVisibility,
                                float minWindStrength, float maxWindStrength, float headingRotation,
                                float angleFactor) {
        visibilityHandler_.AddNewConfig(formID, minVisibility, maxVisibility, minWindStrength, maxWindStrength,
                                        headingRotation, angleFactor);
    }
    void AddNewVisibilityConfig(const RE::TESBoundObject* obj, float minVisibility, float maxVisibility,
                                float minWindStrength, float maxWindStrength, float headingRotation,
                                float angleFactor) {
        visibilityHandler_.AddNewConfig(obj->GetFormID(), minVisibility, maxVisibility, minWindStrength,
                                        maxWindStrength,
                                        headingRotation, angleFactor);
    }
    void AddNewVisibilityConfig(const RE::TESObjectREFR* ref, float minVisibility, float maxVisibility,
                                float minWindStrength, float maxWindStrength, float headingRotation, float angleFactor) {
        if (!ref->IsDynamicForm()) {
            visibilityHandler_.AddNewConfig(ref->GetFormID(), minVisibility, maxVisibility, minWindStrength,
                                            maxWindStrength,
                                            headingRotation, angleFactor);
        }
    }
    void RemoveVisibilityConfig(const RE::FormID formID) { visibilityHandler_.RemoveConfig(formID); }
    
    void AddNewPushConfig(const RE::FormID formID, float windSensitivity) {
        pushHandler_.AddNewConfig(formID, windSensitivity);
    }
    void AddNewPushConfig(const RE::TESBoundObject* obj, float windSensitivity) {
        pushHandler_.AddNewConfig(obj->GetFormID(), windSensitivity);
    }
    void AddNewPushConfig(const RE::TESObjectREFR* ref, float windSensitivity) {
        if (!ref->IsDynamicForm()) {
            pushHandler_.AddNewConfig(ref->GetFormID(), windSensitivity);
        }
    }
    void RemovePushConfig(const RE::FormID formID) { pushHandler_.RemoveConfig(formID); }

    void AddNewModelSwapConfig(const RE::FormID formID, float headingRotation, float angleFactor,
                               std::vector<ModelSwapEntry>& swaps) {
        modelSwapHandler_.AddNewConfig(formID, headingRotation, angleFactor, swaps);
    }
    void AddNewModelSwapConfig(const RE::TESBoundObject* obj, float headingRotation, float angleFactor,
                               std::vector<ModelSwapEntry>& swaps) {
        modelSwapHandler_.AddNewConfig(obj->GetFormID(), headingRotation, angleFactor, swaps);
    }
    void AddNewModelSwapConfig(const RE::TESObjectREFR* ref, float headingRotation, float angleFactor,
                               std::vector<ModelSwapEntry>& swaps) {
        if (!ref->IsDynamicForm()) {
            modelSwapHandler_.AddNewConfig(ref->GetFormID(), headingRotation, angleFactor, swaps);
        }
    }
    void RemoveModelSwapConfig(const RE::FormID formID) { modelSwapHandler_.RemoveConfig(formID); }

    void AddNewBaseObjSwapConfig(const RE::FormID formID, const std::vector<BaseObjSwapEntry>& swaps,
                                 float headingRotation, float angleFactor) {
        baseObjSwapHandler_.AddNewConfig(formID, swaps, headingRotation, angleFactor);
    }
    void RemoveBaseObjSwapConfig(const RE::FormID formID) { baseObjSwapHandler_.RemoveConfig(formID); }

private:
    // Sample local ambient wind to select the initial model or base-object variant.
    std::pair<float, float> GetSwapWind(const RE::TESObjectREFR* ref, float strength, float angle) {
        if (OpenShadersWind::IsEnabled()) {
            const auto position = ref->GetPosition();
            if (OpenShadersWind::SamplePositions({&position, 1}, _windSamples)) {
                const auto& sample = _windSamples.front().wind;
                const float x = sample.baseVelocity.x + sample.gustVelocity.x;
                const float y = sample.baseVelocity.y + sample.gustVelocity.y;
                const float speed = std::hypot(x, y);
                return {std::clamp(speed, 0.0f, 1.0f), speed > 0.0001f ? std::atan2(y, x) : angle};
            }
        }
        return {strength, angle};
    }

    AnimationHandler animationHandler_;
    BaseObjSwapHandler baseObjSwapHandler_;
    ModelSwapHandler modelSwapHandler_;
    PushHandler pushHandler_;
    RotationHandler rotationHandler_;
    TreeHandler treeHandler_;
    VisibilityHandler visibilityHandler_;

    std::shared_mutex mutex_;
    std::unordered_map<RE::FormID, RE::ObjectRefHandle> _trackedAnimationRefs;
    std::unordered_map<RE::FormID, RE::ObjectRefHandle> _trackedBaseObjSwapRefs;
    std::unordered_map<RE::FormID, RE::ObjectRefHandle> _trackedModelSwapRefs;
    std::unordered_map<RE::FormID, RE::ObjectRefHandle> _trackedPushRefs;
    std::unordered_map<RE::FormID, RE::ObjectRefHandle> _trackedRotationRefs;
    std::unordered_map<RE::FormID, RE::ObjectRefHandle> _trackedVisibilityRefs;
    std::unordered_map<RE::FormID, OpenShadersWind::Vector> _rotationWind;
    bool _pendingSwapTargets = false;
    std::optional<uint64_t> _pendingSwapFrame;
    bool _reportedWindTargets = false;
    std::vector<RE::NiPoint3> _samplePositions;
    std::vector<OpenShadersWind::SampleWithHavokExclusion> _windSamples;
    std::unordered_map<RE::FormID, std::size_t> _sampleIndices;
};
