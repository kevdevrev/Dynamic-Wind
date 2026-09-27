#pragma once

#include "REX/REX.h"

class Config : public REX::Singleton<Config> {
public:
    void LoadIni() {
        std::ifstream file("Data\\SKSE\\Plugins\\DynamicWind\\DynamicWind.ini");
        if (!file.is_open()) {
            logger::error("INI not found");
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.contains("bModActive")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    ModActive = (value == "true" || value == "True" || value == "1");
                }
            }
            if (line.contains("bAnimationHandlerEnabled")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    AnimationHandlerEnabled = (value == "true" || value == "True" || value == "1");
                }
            }
            if (line.contains("bBaseObjSwapHandlerEnabled")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    BaseObjSwapHandlerEnabled = (value == "true" || value == "True" || value == "1");
                }
            }
            if (line.contains("bModelSwapHandlerEnabled")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    ModelSwapHandlerEnabled = (value == "true" || value == "True" || value == "1");
                }
            }
            if (line.contains("bPushHandlerEnabled")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    PushHandlerEnabled = (value == "true" || value == "True" || value == "1");
                }
            }
            if (line.contains("bRotationHandlerEnabled")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    RotationHandlerEnabled = (value == "true" || value == "True" || value == "1");
                }
            }
            if (line.contains("bTreeHandlerEnabled")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    TreeHandlerEnabled = (value == "true" || value == "True" || value == "1");
                }
            }
            if (line.contains("bOpenShadersIntegrationEnabled")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    OpenShadersIntegrationEnabled = (value == "true" || value == "True" || value == "1");
                }
            }
            if (line.contains("bVisibilityHandlerEnabled")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    VisibilityHandlerEnabled = (value == "true" || value == "True" || value == "1");
                }
            }
            if (line.contains("fMinWindStrength")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    minWindStrength = std::stof(value);
                }
            }
            if (line.contains("fMaxWindStrength")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string value = line.substr(pos + 1);
                    maxWindStrength = std::stof(value);
                }
            }
        }

        if (maxWindStrength < minWindStrength) {
            maxWindStrength = minWindStrength;
        }
        if (minWindStrength < 0.0f) {
            minWindStrength = 0.0f;
        }
        if (maxWindStrength > 1.0f) {
            maxWindStrength = 1.0f;
        }

        logger::info("Settings loaded from INI");
        logger::info("ModActive: {}, AnimationHandlerEnabled: {}, BaseObjSwapHandlerEnabled: {}, ModelSwapHandlerEnabled: {}, PushHandlerEnabled: {}, RotationHandlerEnabled: {}, TreeHandlerEnabled: {}, VisibilityHandlerEnabled: {}, MinWindStrength: {}, MaxWindStrength: {}",
            ModActive, AnimationHandlerEnabled, BaseObjSwapHandlerEnabled, ModelSwapHandlerEnabled, PushHandlerEnabled,
            RotationHandlerEnabled, TreeHandlerEnabled, VisibilityHandlerEnabled, minWindStrength, maxWindStrength);
    }

    void SaveIni() {
        std::ofstream file("Data\\SKSE\\Plugins\\DynamicWind\\DynamicWind.ini");
        if (!file.is_open()) {
            logger::error("Failed to save INI");
            return;
        }
        file << "bModActive=" << (ModActive ? "true" : "false") << std::endl;
        file << "bAnimationHandlerEnabled=" << (AnimationHandlerEnabled ? "true" : "false") << std::endl;
        file << "bBaseObjSwapHandlerEnabled=" << (BaseObjSwapHandlerEnabled ? "true" : "false") << std::endl;
        file << "bModelSwapHandlerEnabled=" << (ModelSwapHandlerEnabled ? "true" : "false") << std::endl;
        file << "bPushHandlerEnabled=" << (PushHandlerEnabled ? "true" : "false") << std::endl;
        file << "bRotationHandlerEnabled=" << (RotationHandlerEnabled ? "true" : "false") << std::endl;
        file << "bOpenShadersIntegrationEnabled=" << (OpenShadersIntegrationEnabled ? "true" : "false") << std::endl;
        file << "bTreeHandlerEnabled=" << (TreeHandlerEnabled ? "true" : "false") << std::endl;
        file << "bVisibilityHandlerEnabled=" << (VisibilityHandlerEnabled ? "true" : "false") << std::endl;

        file << "fMinWindStrength=" << minWindStrength << std::endl;
        file << "fMaxWindStrength=" << maxWindStrength << std::endl;

        logger::info("Settings saved to INI");
    }

    bool ModActive{true};
    bool AnimationHandlerEnabled{true};
    bool BaseObjSwapHandlerEnabled{true};
    bool ModelSwapHandlerEnabled{false};
    bool PushHandlerEnabled{true};
    bool RotationHandlerEnabled{true};
    bool TreeHandlerEnabled{true};
    bool OpenShadersIntegrationEnabled{true};
    bool VisibilityHandlerEnabled{true};
    bool EnableTimeLogging{false};

    float minWindStrength{0.0f};
    float maxWindStrength{1.0f};

};