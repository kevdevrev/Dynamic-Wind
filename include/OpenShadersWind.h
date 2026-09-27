#pragma once

#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace RE {
    class NiPoint3;
}

namespace OpenShadersWind {
    inline constexpr uint32_t InterfaceMessageType = 0x43534150;
    inline constexpr unsigned int InterfaceRevision = 4;
    inline constexpr uint32_t MaximumBatchSize = 16384;

    struct Vector {
        float x{};
        float y{};
        float z{};
    };

    struct Sample {
        Vector baseVelocity{};
        Vector gustVelocity{};
        Vector transientVelocity{};
        Vector finalVelocity{};
        float ambientGust{};
        float transientIntensity{};
        uint64_t frameId{};
    };
    static_assert(sizeof(Vector) == 12);
    static_assert(sizeof(Sample) == 64);
    static_assert(std::is_standard_layout_v<Vector>);
    static_assert(std::is_standard_layout_v<Sample>);

    struct SampleWithHavokExclusion {
        Sample wind{};
        Vector windExcludingHavokImpulses{};
    };
    static_assert(sizeof(SampleWithHavokExclusion) == 80);
    static_assert(std::is_standard_layout_v<SampleWithHavokExclusion>);

    struct Message {
        void* (*GetApiFunction)(unsigned int revisionNumber) = nullptr;
    };

    enum class UpscalePreset : uint32_t {};
    enum class DLSSProfile : uint32_t {};
    enum class UpscaleMethod : uint32_t {};

    struct Interface {
        virtual unsigned int getBuildNumber() = 0;
        virtual bool GetSSSEnabled() = 0;
        virtual void SetSSSEnabled(bool enabled) = 0;
        virtual bool GetSSGIEnabled() = 0;
        virtual void SetSSGIEnabled(bool enabled) = 0;
        virtual bool GetVolumetricLightingExteriorEnabled() = 0;
        virtual void SetVolumetricLightingExteriorEnabled(bool enabled) = 0;
        virtual UpscalePreset GetUpscalePreset() = 0;
        virtual void SetUpscalePreset(UpscalePreset preset) = 0;
        virtual bool GetLightLimitFixContactShadowsEnabled() = 0;
        virtual void SetLightLimitFixContactShadowsEnabled(bool enabled) = 0;
        virtual DLSSProfile GetDLSSProfile() = 0;
        virtual void SetDLSSProfile(DLSSProfile profile) = 0;
        virtual bool GetRenderAtUpscaleResEnabled() = 0;
        virtual void SetRenderAtUpscaleResEnabled(bool enabled) = 0;
        virtual bool GetRenderAtUpscaleResActive() = 0;
        virtual void SetVRUpscalingTransitionProfile(bool enabled, UpscalePreset preset, DLSSProfile profile) = 0;
        virtual UpscaleMethod GetUpscaleMethod() = 0;
        virtual void SetUpscaleMethod(UpscaleMethod method) = 0;
        virtual void SetVRUpscalingTransitionProfileForMethod(UpscaleMethod method, bool enabled, UpscalePreset preset,
                                                              DLSSProfile profile) = 0;
        virtual uint32_t GetVRUpscalingApplyBlockReasons() = 0;
        virtual bool IsVRUpscalingProfileApplyAllowed() = 0;
        virtual bool SampleWind(const Vector* positions, Sample* samples, uint32_t count) = 0;
        virtual bool SampleWindExcludingHavokImpulses(const Vector* positions, SampleWithHavokExclusion* samples, uint32_t count) = 0;
    };

    void Connect();
    /// Reports a compatible wind provider regardless of the integration setting.
    [[nodiscard]] bool IsAvailable();
    /// Reports whether the detected provider is enabled for all Dynamic Wind handlers.
    [[nodiscard]] bool IsEnabled();
    [[nodiscard]] bool SamplePositions(std::span<const RE::NiPoint3> positions,
                                       std::vector<SampleWithHavokExclusion>& samples);
}
