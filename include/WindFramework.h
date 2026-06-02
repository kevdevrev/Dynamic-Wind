#pragma once

#include "Settings.h"

#include "AnimationHandler.h"
#include "BaseObjSwapHandler.h"
#include "ModelSwapHandler.h"
#include "PushHandler.h"
#include "RotationHandler.h"
#include "TreeHandler.h"
#include "VisibilityHandler.h"

#include "REX/REX.h"

#include <unordered_set>
#include <shared_mutex>

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
        if (conf->RotationHandlerEnabled) {
            for (auto it = _trackedRotationRefs.begin(); it != _trackedRotationRefs.end();) {
                auto ref = it->second.get().get();
                if (!ref) {
                    it = _trackedRotationRefs.erase(it);
                } else {
                    if (ref->IsDisabled() || ref->IsDeleted()) {
                        it = _trackedRotationRefs.erase(it);
                    } else {
                        rotationHandler_.Apply(ref, strength, angle);
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
                        animationHandler_.Apply(ref, strength, angle);
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
                        visibilityHandler_.Apply(ref, strength, angle);
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
                        pushHandler_.Apply(ref, strength, angle);
                        ++it;
                    }
                }
            }
        }
    }

    void NewTargets(float strength, float angle) {
        auto* conf = Config::GetSingleton();

        std::unique_lock lock(mutex_);

        if (conf->TreeHandlerEnabled) {
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

    void DisableMod() { treeHandler_.Update(0, 0); }

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
            if (_trackedModelSwapRefs.try_emplace(formID, ref->GetHandle()).second) {
                modelSwapHandler_.Apply(ref, strength, angle);
            }
        } else if (conf->BaseObjSwapHandlerEnabled && (baseObjSwapHandler_.HasConfig(baseFormID) || baseObjSwapHandler_.HasConfig(formID))) {
            if (_trackedBaseObjSwapRefs.try_emplace(formID, ref->GetHandle()).second) {
                baseObjSwapHandler_.Apply(ref, strength, angle);
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
};