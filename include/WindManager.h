#pragma once

#include "REX/REX.h"

#include "WindFramework.h"
#include "Settings.h"

#define M_PI 3.14159265358979323846f

namespace Wind {
    class Manager : public REX::Singleton<Manager> {
    public:
        void Update(RE::Actor* player, float deltaTime) {

            if (!Utils::IsInWindCell(player)) {
                auto currentWorldspace = player->GetWorldspace();
                auto currentCell = player->GetParentCell();
                if ((_previousWorldspace != currentWorldspace) || (_previousCell != currentCell)) {
                    _previousWorldspace = currentWorldspace;
                    _previousCell = currentCell;
                    logger::info("Cell is Interior or Cave, no wind here");
                    InteriorCellUpdate();
                }
                return;
            }

            auto* sky = RE::Sky::GetSingleton();
            if (!sky || !sky->currentWeather) {
                return;
            }

            if (_previousWeather != sky->currentWeather) {
                _previousWeather = sky->currentWeather;
                GenerateNewTargets(sky);
            }

            // Smooth interpolation toward target
            _angle = LerpAngle(_angle, _targetAngle, deltaTime * 0.1f);
            _strength = Lerp(_strength, _targetStrength, deltaTime * 0.1f);

            static float time = 0.0f;
            time += deltaTime;

            // Add smooth procedural gusts
            // Range: -3 / 10 <= f <= 3 / 10
            float gust = std::sin(time * 0.7f) * 0.2f + std::sin(time * 2.3f) * 0.1f;

            float finalStrength = std::clamp(_strength + (gust * _strength), 0.0f, 1.0f);

            // Angle turbulence
            float angleNoise = std::sin(time * 0.5f) * 0.1f + std::cos(time * 0.4f) * 0.1f;
            float finalAngle = _angle + angleNoise;

            while (finalAngle > M_PI) finalAngle -= M_PI * 2.0f;
            while (finalAngle < -M_PI) finalAngle += M_PI * 2.0f;

            if (_TestMode) {
                // Just set the wind to target values
                sky->windSpeed = _targetStrength;
                sky->windAngle = _targetAngle;
                // if no precipitation, set it to match the wind direction and strength
                if (!sky->IsRaining() && !sky->IsSnowing()) {
                    auto& precipDir = sky->precip->GetDirection();
                    precipDir.x = std::cos(_targetAngle);
                    precipDir.y = std::sin(_targetAngle);
                    precipDir.z = _targetStrength;
                }
                if (_UpdateWindFramework) {
                    WindFramework::GetSingleton()->Update(finalStrength, finalAngle, deltaTime);
                }
            } else {
                sky->windSpeed = finalStrength;
                sky->windAngle = finalAngle;
                // if no precipitation, set it to match the wind direction and strength
                if (!sky->IsRaining() && !sky->IsSnowing()) {
                    auto& precipDir = sky->precip->GetDirection();
                    precipDir.x = std::cos(finalAngle);
                    precipDir.y = std::sin(finalAngle);
                    precipDir.z = finalStrength;
                }
                WindFramework::GetSingleton()->Update(finalStrength, finalAngle, deltaTime);
            }
        }

        void SetTargets(float angle, float speed) {
            _targetAngle = angle;
            _targetStrength = speed;

            logger::info("Manual Targets: _targetAngle: {}, _targetStrength {}", _targetAngle, _targetStrength);
            WindFramework::GetSingleton()->NewTargets(_targetStrength, _targetAngle);
        }

        void SetTestMode(bool enabled) { _TestMode = enabled; }
        void SetUpdateFramework(bool enabled) { _UpdateWindFramework = enabled; }

        std::pair<float, float> GetTargets() const { return {_targetAngle, _targetStrength}; }

        void DisableMod() { 
            auto* sky = RE::Sky::GetSingleton();
            float vanillaStrength = float(sky->currentWeather->data.windSpeed) / 255.0f;
            float vanillaAngle = sky->currentWeather->data.windDirection * (M_PI / 128.0f);
            sky->windSpeed = vanillaStrength;
            sky->windAngle = vanillaAngle;
            WindFramework::GetSingleton()->DisableMod();
        }

        void EnableMod() {
            auto* sky = RE::Sky::GetSingleton();
            GenerateNewTargets(sky);
        }

    private:
        void GenerateNewTargets(const RE::Sky* sky) {
            float vanillaStrength = float(sky->currentWeather->data.windSpeed) / 255.0f;

            auto* conf = Config::GetSingleton();

            float scaledStrength =
                conf->minWindStrength + (vanillaStrength * (conf->maxWindStrength - conf->minWindStrength));

            _targetAngle = RandomFloat(-M_PI, M_PI); // 100% random direction
            _targetStrength = scaledStrength * RandomFloat(0.9f, 1.1f);  // A bit random strength

            WindFramework::GetSingleton()->NewTargets(_targetStrength, _targetAngle);

            logger::info("NewTargets: _targetAngle: {}, _targetStrength {}, vanillaStrength {}", _targetAngle,
                         _targetStrength, vanillaStrength);
        }

        void InteriorCellUpdate() {
            auto* sky = RE::Sky::GetSingleton();
            if (sky) {
                sky->windSpeed = 0.0f;
                sky->windAngle = 0.0f;
            }
        }

        static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

        float WrapAngle(float a) {
            while (a > M_PI) a -= 2.0f * M_PI;
            while (a < -M_PI) a += 2.0f * M_PI;
            return a;
        }

        float LerpAngle(float a, float b, float t) {
            float delta = WrapAngle(b - a);
            return a + delta * t;
        }

        static float RandomFloat(float min, float max) { return min + (max - min) * (float(rand()) / float(RAND_MAX)); }

        RE::TESWeather* _previousWeather{nullptr};
        RE::TESWorldSpace* _previousWorldspace{nullptr};
        RE::TESObjectCELL* _previousCell {nullptr};

        float _angle{0.0f};
        float _targetAngle{0.0f};
        float _strength{0.0f};
        float _targetStrength{0.0f};

        bool _TestMode{false};
        bool _UpdateWindFramework{true};
    };
}