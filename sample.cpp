#include <pch.hpp>
#include <gles.hpp>
#include <half.hpp>
#include <log.hpp>
#include <npu.hpp>
#include "sample.hpp"
#include <scope_guard.hpp>
#include <vk.hpp>

namespace ns::sdf_unet {

namespace {

constexpr char const *VERTEX_SHADER = R"(#version 320 es
out vec2 vUV;
void main() {
    float x = float((gl_VertexID & 1) << 2);
    float y = float((gl_VertexID & 2) << 1);
    vUV = vec2(x * 0.5, y * 0.5);
    gl_Position = vec4(x - 1.0, y - 1.0, 0.0, 1.0);
}
)";

constexpr char const *FRAGMENT_SHADER = R"(#version 320 es
precision highp float;
uniform sampler2D uSdf;
in vec2 vUV;
out vec4 fragColor;
void main() {
    float v = texture(uSdf, vUV).r;
    if (v < 0.0) {
        fragColor = vec4(1.0 + v, 0.0, 0.0, 1.0);
    } else {
        fragColor = vec4(0.0, 0.0, 1.0 - v, 1.0);
    }
}
)";

}

GLuint Sample::CompileShader(GLenum type, char const *source) noexcept
{
    GLuint shader = glCreateShader(type);
    if (!shader) [[unlikely]] {
        NSLOGE("glCreateShader failed");
        return 0U;
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) [[unlikely]] {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            char log[512];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            NSLOGE("Shader compile error: %s", log);
        }
        glDeleteShader(shader);
        return 0U;
    }

    return shader;
}

Sample::Sample(MessageQueue &messageQueue, vk::Renderer &vkRenderer, gles::Renderer &glesRenderer,
    npu::Device &npuDevice) noexcept
    : ns::Sample(messageQueue, vkRenderer, glesRenderer, npuDevice)
{
    inputBitmap_.resize(SDF_PIXELS);
    outputSdf_.resize(SDF_PIXELS);
    textureData_.resize(SDF_PIXELS * 4U);
}

void Sample::OnVKDeviceCreated() noexcept
{
}

void Sample::OnVKDeviceDestroyed() noexcept
{
}

void Sample::OnVKSwapchainCreated() noexcept
{
}

void Sample::OnVKSwapchainDestroyed() noexcept
{
}

ns::Sample::PresentResult Sample::OnVKFrame(VkCommandBuffer commandBuffer, size_t /* commandBufferIndex */,
    double /* deltaTime */) noexcept
{
    vk::PresentPass &presentPass = vkRenderer_.GetPresentPass();
    presentPass.Begin(commandBuffer);
    return presentPass.End(vkRenderer_, commandBuffer, nullptr);
}

void Sample::OnGLESContextCreated() noexcept
{

    GLuint const vert = CompileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
    if (!vert) [[unlikely]] {
        NSLOGE("Can't compile vertex shader");
        return;
    }

    ScopeGuard const deleteVert([vert]() noexcept {
        glDeleteShader(vert);
    });

    GLuint const frag = CompileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    if (!frag) [[unlikely]] {
        NSLOGE("Can't compile fragment shader");
        return;
    }

    ScopeGuard const deleteFrag([frag]() noexcept {
        glDeleteShader(frag);
    });

    program_ = glCreateProgram();
    if (!program_) [[unlikely]] {
        NSLOGE("glCreateProgram failed");
        return;
    }

    glAttachShader(program_, vert);
    glAttachShader(program_, frag);
    glLinkProgram(program_);

    GLint success = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) [[unlikely]] {
        GLint length = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            char log[512];
            glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
            NSLOGE("Program link error: %s", log);
        }
        glDeleteProgram(std::exchange(program_, 0U));
        return;
    }

    sdfUniform_ = glGetUniformLocation(program_, "uSdf");

    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(SDF_SIZE), static_cast<GLsizei>(SDF_SIZE), 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0U);

    NSLOGI("SDF U-Net GLES renderer ready");
}

void Sample::OnGLESContextDestroyed() noexcept
{

    if (texture_) [[likely]] {
        GLuint const t = std::exchange(texture_, 0U);
        glDeleteTextures(1, &t);
    }

    if (program_) [[likely]] {
        glDeleteProgram(std::exchange(program_, 0U));
    }

    sdfUniform_ = -1;
}

void Sample::OnGLESSwapchainCreated(uint32_t width, uint32_t height) noexcept
{
    surfaceWidth_ = width;
    surfaceHeight_ = height;
}

void Sample::OnGLESSwapchainDestroyed() noexcept
{
    surfaceWidth_ = 0U;
    surfaceHeight_ = 0U;
}

void Sample::OnGLESFrame(double /* deltaTime */) noexcept
{

    if (sdfReady_.exchange(false)) [[likely]] {
        for (size_t i = 0U; i < SDF_PIXELS; ++i) {
            float const v = static_cast<float>(outputSdf_[i]);
            float r, g, b;
            if (v < 0.0f) {
                r = 1.0f + v;
                g = 0.0f;
                b = 0.0f;
            } else {
                r = 0.0f;
                g = 0.0f;
                b = 1.0f - v;
            }
            size_t const offset = i * 4U;
            textureData_[offset + 0U] = static_cast<uint8_t>(r * 255.0f);
            textureData_[offset + 1U] = static_cast<uint8_t>(g * 255.0f);
            textureData_[offset + 2U] = static_cast<uint8_t>(b * 255.0f);
            textureData_[offset + 3U] = 255U;
        }

        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(SDF_SIZE), static_cast<GLsizei>(SDF_SIZE),
            GL_RGBA, GL_UNSIGNED_BYTE, textureData_.data());
        glBindTexture(GL_TEXTURE_2D, 0U);
    }

    glViewport(0, 0, static_cast<GLsizei>(surfaceWidth_), static_cast<GLsizei>(surfaceHeight_));
    glClear(static_cast<GLbitfield>(GL_COLOR_BUFFER_BIT));

    if (!program_ || !texture_) [[unlikely]] {
        return;
    }

    glUseProgram(program_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glUniform1i(sdfUniform_, 0);

    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Sample::OnNPUDeviceCreated() noexcept
{

    executor_ = npuDevice_.CreateExecutor("models/sdf_unet.omc", HiAI_BandMode::HIAI_BANDMODE_NORMAL, true);
    if (!executor_) [[unlikely]] {
        NSLOGE("Can't create executor for SDF U-Net model");
        return;
    }

    size_t inputCount = 0U;
    bool result = npu::Device::CheckNNResult(OH_NNExecutor_GetInputCount(executor_, &inputCount),
        "SdfUnet::OnNPUDeviceCreated", "Can't get input count");
    if (!result) [[unlikely]] {
        return;
    }

    if (inputCount != 1U) [[unlikely]] {
        NSLOGE("Expected 1 input, got %zu", inputCount);
        return;
    }

    size_t outputCount = 0U;
    result = npu::Device::CheckNNResult(OH_NNExecutor_GetOutputCount(executor_, &outputCount),
        "SdfUnet::OnNPUDeviceCreated", "Can't get output count");
    if (!result) [[unlikely]] {
        return;
    }

    if (outputCount != 1U) [[unlikely]] {
        NSLOGE("Expected 1 output, got %zu", outputCount);
        return;
    }

    size_t device = npuDevice_.GetDevice();

    NN_TensorDesc *inputDesc = OH_NNExecutor_CreateInputTensorDesc(executor_, 0U);
    if (!inputDesc) [[unlikely]] {
        NSLOGE("Can't create input tensor descriptor");
        return;
    }

    ScopeGuard const freeInputDesc([inputDesc]() mutable noexcept {
        OH_NNTensorDesc_Destroy(&inputDesc);
    });

    input_ = OH_NNTensor_Create(device, inputDesc);
    if (!input_) [[unlikely]] {
        NSLOGE("Can't create input tensor");
        return;
    }

    result = npu::Device::CheckNNResult(OH_NNTensor_GetSize(input_, &inputSize_),
        "SdfUnet::OnNPUDeviceCreated", "Can't get input tensor size");
    if (!result) [[unlikely]] {
        return;
    }

    inputData_ = static_cast<Half *>(OH_NNTensor_GetDataBuffer(input_));
    if (!inputData_) [[unlikely]] {
        NSLOGE("Can't get input data buffer");
        return;
    }

    NN_TensorDesc *outputDesc = OH_NNExecutor_CreateOutputTensorDesc(executor_, 0U);
    if (!outputDesc) [[unlikely]] {
        NSLOGE("Can't create output tensor descriptor");
        return;
    }

    ScopeGuard const freeOutputDesc([outputDesc]() mutable noexcept {
        OH_NNTensorDesc_Destroy(&outputDesc);
    });

    output_ = OH_NNTensor_Create(device, outputDesc);
    if (!output_) [[unlikely]] {
        NSLOGE("Can't create output tensor");
        return;
    }

    result = npu::Device::CheckNNResult(OH_NNTensor_GetSize(output_, &outputSize_),
        "SdfUnet::OnNPUDeviceCreated", "Can't get output tensor size");
    if (!result) [[unlikely]] {
        return;
    }

    outputData_ = static_cast<Half const *>(OH_NNTensor_GetDataBuffer(output_));
    if (!outputData_) [[unlikely]] {
        NSLOGE("Can't get output data buffer");
        return;
    }

    NSLOGI("SDF U-Net ready: input=%zu bytes, output=%zu bytes", inputSize_, outputSize_);

    for (size_t i = 0U; i < SDF_PIXELS; ++i) {
        float dx = static_cast<float>(i % SDF_SIZE) - static_cast<float>(SDF_SIZE) / 2.0f;
        float dy = static_cast<float>(i / SDF_SIZE) - static_cast<float>(SDF_SIZE) / 2.0f;
        float dist = sqrtf(dx * dx + dy * dy);
        inputBitmap_[i] = static_cast<float>(dist < static_cast<float>(SDF_SIZE) * 0.3f ? 1.0f : 0.0f);
    }
}

void Sample::OnNPUDeviceDestroyed() noexcept
{

    if (NN_Tensor *tensor = std::exchange(output_, nullptr); tensor) [[likely]] {
        OH_NNTensor_Destroy(&tensor);
    }

    if (NN_Tensor *tensor = std::exchange(input_, nullptr); tensor) [[likely]] {
        OH_NNTensor_Destroy(&tensor);
    }

    inputData_ = nullptr;
    outputData_ = nullptr;
    inputBitmap_.clear();
    inputBitmap_.shrink_to_fit();
    outputSdf_.clear();
    outputSdf_.shrink_to_fit();

    if (executor_) [[likely]] {
        OH_NNExecutor_Destroy(&executor_);
        executor_ = nullptr;
    }
}

void Sample::OnNPUTask(double /* deltaTime */) noexcept
{
    if (!executor_ || !input_ || !output_) [[unlikely]] {
        return;
    }

    memcpy(inputData_, inputBitmap_.data(), SDF_PIXELS * sizeof(Half));

    NN_Tensor *inputs[] = {input_};
    bool result = npu::Device::CheckNNResult(
        OH_NNExecutor_RunSync(executor_, inputs, std::size(inputs), &output_, 1U),
        "SdfUnet::OnNPUTask", "NPU inference failed");
    if (!result) [[unlikely]] {
        return;
    }

    memcpy(outputSdf_.data(), outputData_, SDF_PIXELS * sizeof(Half));
    sdfReady_.store(true);
}

} // namespace ns::sdf_unet
