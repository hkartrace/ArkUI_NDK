#ifndef NS_SDF_UNET_SAMPLE_HPP
#define NS_SDF_UNET_SAMPLE_HPP

#include "half.hpp"
#include <half.hpp>
#include <sample.hpp>

NS_DISABLE_COMMON_WARNINGS

#include <GLES3/gl32.h>
#include <neural_network_runtime/neural_network_runtime_type.h>
#include <atomic>
#include <vector>

NS_RESTORE_WARNING_STATE

namespace ns::sdf_unet {

class Sample final : public ns::Sample {
public:
    Sample() = delete;

    Sample(Sample const &) = delete;
    Sample &operator=(Sample const &) = delete;

    Sample(Sample &&) = delete;
    Sample &operator=(Sample &&) = delete;

    explicit Sample(MessageQueue &messageQueue, vk::Renderer &vkRenderer, gles::Renderer &glesRenderer,
        npu::Device &npuDevice) noexcept;

    ~Sample() override = default;

private:
    void OnVKDeviceCreated() noexcept override;
    void OnVKDeviceDestroyed() noexcept override;

    void OnVKSwapchainCreated() noexcept override;
    void OnVKSwapchainDestroyed() noexcept override;

    [[nodiscard]] PresentResult OnVKFrame(VkCommandBuffer commandBuffer, size_t commandBufferIndex,
        double deltaTime) noexcept override;

    void OnGLESContextCreated() noexcept override;
    void OnGLESContextDestroyed() noexcept override;

    void OnGLESSwapchainCreated(uint32_t width, uint32_t height) noexcept override;
    void OnGLESSwapchainDestroyed() noexcept override;

    void OnGLESFrame(double deltaTime) noexcept override;

    void OnNPUDeviceCreated() noexcept override;
    void OnNPUDeviceDestroyed() noexcept override;
    void OnNPUTask(double deltaTime) noexcept override;

    static constexpr uint32_t SDF_SIZE = 128U;
    static constexpr size_t SDF_PIXELS = SDF_SIZE * SDF_SIZE;

    static GLuint CompileShader(GLenum type, char const *source) noexcept;

    OH_NNExecutor *executor_ = nullptr;
    NN_Tensor *input_ = nullptr;
    NN_Tensor *output_ = nullptr;

    size_t inputSize_ = 0U;
    size_t outputSize_ = 0U;

    Half *inputData_ = nullptr;
    Half const *outputData_ = nullptr;

    std::vector<Half> inputBitmap_;
    std::vector<Half> outputSdf_;

    GLuint program_ = 0U;
    GLuint texture_ = 0U;
    GLint sdfUniform_ = -1;
    std::vector<uint8_t> textureData_;
    std::atomic<bool> sdfReady_{false};

    uint32_t surfaceWidth_ = 0U;
    uint32_t surfaceHeight_ = 0U;
};

} // namespace ns::sdf_unet

#endif // NS_SDF_UNET_SAMPLE_HPP
