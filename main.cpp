#include <iostream>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>
#include <cassert>
#include <vector>
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>


const char* shaderSource = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index:u32) -> @builtin(position) vec4f {
    var p = vec2f(0.0, 0.0);
    if (in_vertex_index == 0u) {
        p = vec2f(-0.5, -0.5);
    } else if (in_vertex_index == 1u) {
        p = vec2f(0.5, -0.5);
    } else {
        p = vec2f(0.0, 0.5);
    }
    return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
    return vec4f(0.0, 0.4, 1.0, 1.0);
}
)";


WGPUAdapter requestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const* options) {
    struct UserData {
        WGPUAdapter adapter = nullptr;
        bool requestEnded = false;
    };

    UserData userData;


    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestAdapterStatus_Success) {
            userData.adapter = adapter;

        }
        else {
            std::cout << "Could not get WebGPU adapter:" << message << std::endl;
        }
        userData.requestEnded = true;
    };


    wgpuInstanceRequestAdapter(instance, options, onAdapterRequestEnded, (void*)&userData);

    assert(userData.requestEnded);

    return userData.adapter;
}

WGPUDevice requestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor) {
    struct UserData {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestDeviceStatus_Success) {
            userData.device = device;
        }
        else {
            std::cout << "Could not get WebGPU device: " << message << std::endl;
        }
        userData.requestEnded = true;
    };

    wgpuAdapterRequestDevice(
        adapter,
        descriptor,
        onDeviceRequestEnded,
        (void*)&userData
    );

    assert(userData.requestEnded);

    return userData.device;
}


int main(int,char**) 
{
    if (!glfwInit())
    {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(650, 480, "Learn WebGPU", NULL, NULL);
    if (!window) 
    {
        std::cerr << "Could not open window!" << std::endl;
        glfwTerminate();
        return 1;
    }

    wgpu::InstanceDescriptor desc = {};

    wgpu::Instance instance = wgpu::createInstance(desc);
    if (!instance) 
    {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        return 1;
    }

    std::cout << "WGPU instance:" << instance << std::endl;

    std::cout << "Requesting adapter..." << std::endl;

    wgpu::Surface surface = glfwGetWGPUSurface(instance, window);

    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.compatibleSurface = surface;
    wgpu::Adapter adapter = requestAdapter(instance, &adapterOptions);

    std::cout << "Got adapter: " << adapter << std::endl;

    // Is something missing here??
        // Why doesn't this work??
        // Huh??
        // PR opportunity??
    std::vector<WGPUFeatureName> features;
    size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
    features.resize(featureCount);

    wgpuAdapterEnumerateFeatures(adapter, features.data());

    std::cout << "Adapter features:" << std::endl;
    for (auto f : features) {
        std::cout << " - " << f << std::endl;
    }

    std::cout << "Requesting device..." << std::endl;
    wgpu::DeviceDescriptor deviceDesc = {};
    deviceDesc.label = "My Device";
    deviceDesc.requiredFeaturesCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    wgpu::Device device = adapter.requestDevice(deviceDesc);
    std::cout << "Got device: " << device << std::endl;

    auto onDeviceError = [](WGPUErrorType type, char const* message, void*) {
        std::cout << "Uncaptured device error: type " << type;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
    };
    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr);


    std::cout << "Allocating GPU memory..." << std::endl;

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.label = "Some GPU-side data buffer";
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
    bufferDesc.size = 16;
    bufferDesc.mappedAtCreation = false;
    wgpu::Buffer buffer1 = device.createBuffer(bufferDesc);
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer buffer2 = device.createBuffer(bufferDesc);

    wgpu::Queue queue = device.getQueue();

    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void*) {
        std::cout << "Queued work finished with status: " << status << std::endl;
    };

    wgpuQueueOnSubmittedWorkDone(queue, onQueueWorkDone, nullptr);

    std::cout << "Uploading data to the GPU..." << std::endl;

    std::vector<uint8_t> numbers(16);
    for (uint8_t i = 0; i < 16; ++i) numbers[i] = i;
    queue.writeBuffer(buffer1, 0, numbers.data(), numbers.size());
    std::cout << "Sending buffer copy operation..." << std::endl;

    wgpu::CommandEncoderDescriptor commandEncoderDesc = {};
    commandEncoderDesc.label = "Command Encoder";
    wgpu::CommandEncoder encoder = device.createCommandEncoder(commandEncoderDesc);

    encoder.copyBufferToBuffer(buffer1, 0, buffer2, 0, 16);
    wgpu::CommandBuffer command = encoder.finish(wgpu::CommandBufferDescriptor{});
    encoder.release();
    queue.submit(1, &command);
    command.release();




    wgpu::SwapChainDescriptor swapChainDesc = {};
    swapChainDesc.width = 640;
    swapChainDesc.height = 480;
    swapChainDesc.format = surface.getPreferredFormat(adapter);
    swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
    wgpu::SwapChain swapChain = device.createSwapChain(surface, swapChainDesc);
    std::cout << "Swapchain: " << swapChain << std::endl;



    // None of this is relevant for this one

    // Create shader module
    wgpu::ShaderModuleDescriptor shaderDesc;
    shaderDesc.hintCount = 0;
    shaderDesc.hints = nullptr;

    wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    shaderCodeDesc.code = shaderSource;
    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    wgpu::ShaderModule shaderModule = device.createShaderModule(shaderDesc);

    // begin renering here
    wgpu::RenderPipelineDescriptor pipelineDesc;
    // define vertex shader
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;
    // define rasterization
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    pipelineDesc.primitive.cullMode = wgpu::CullMode::None;
    // define fragment shader
    wgpu::FragmentState fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    pipelineDesc.fragment = &fragmentState;
    // define stencil/depth test
    pipelineDesc.depthStencil = nullptr;
    // define blending
    wgpu::BlendState blendState;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blendState.alpha.dstFactor = wgpu::BlendFactor::One;
    blendState.alpha.operation = wgpu::BlendOperation::Add;
    wgpu::ColorTargetState colorTarget;
    wgpu::TextureFormat swapChainFormat = surface.getPreferredFormat(adapter);
    colorTarget.format = swapChainFormat; //????
    colorTarget.blend = &blendState;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    pipelineDesc.label = nullptr;

    wgpu::RenderPipeline pipeline = device.createRenderPipeline(pipelineDesc);

    // None of this is relevant for this one




    struct Context {
        wgpu::Buffer buffer;
    };

    auto onBuffer2Mapped = [](WGPUBufferMapAsyncStatus status, void* pUserData) {
        Context* context = reinterpret_cast<Context*>(pUserData);
        std::cout << "Buffer 2 mapped with status " << status << std::endl;
        if (status != wgpu::BufferMapAsyncStatus::Success) return;
        uint8_t* bufferData = (uint8_t*)context->buffer.getConstMappedRange(0, 16);

        std::cout << "bufferData = [";
        for (int i = 0;i < 16; ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << bufferData[i];
        }
        std::cout << "]" << std::endl;

        context->buffer.unmap();
    };

    Context context = { buffer2 };
    wgpuBufferMapAsync(buffer2, wgpu::MapMode::Read, 0, 16, onBuffer2Mapped, (void*)&context);

    while (!glfwWindowShouldClose(window)) 
    {
        queue.submit(0, nullptr);

        glfwPollEvents();

        wgpu::TextureView nextTexture = swapChain.getCurrentTextureView();
        if (!nextTexture) {
            std::cerr << "Cannot acquire next swap chain texture" << std::endl;
            break;
        }
        std::cout << "nextTexture: " << nextTexture << std::endl;

        wgpu::RenderPassDescriptor renderPassDesc = {};
        

        wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
        renderPassColorAttachment.view = nextTexture;
        renderPassColorAttachment.resolveTarget = nullptr;
        renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
        renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
        renderPassColorAttachment.clearValue = wgpu::Color{ 0.9, 0.1, 0.2, 1.0 };


        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &renderPassColorAttachment;
        renderPassDesc.depthStencilAttachment = nullptr;
        renderPassDesc.timestampWriteCount = 0;
        renderPassDesc.timestampWrites = nullptr;


        commandEncoderDesc = {};
        commandEncoderDesc.label = "Command Encoder";
        encoder = device.createCommandEncoder(commandEncoderDesc);


        wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
        

        renderPass.setPipeline(pipeline);

        // Not sure I understand how this gets mapped to wlsl
        renderPass.draw(3, 1, 0, 0);

        renderPass.end();
        renderPass.release();

        nextTexture.release();
        
        wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
        cmdBufferDescriptor.label = "command buffer";
        command = encoder.finish(cmdBufferDescriptor);
        encoder.release();
        queue.submit(1, &command);
        command.release();

        swapChain.present();
    }        
    buffer1.destroy();
    buffer2.destroy();
    buffer1.release();
    buffer2.release();

    swapChain.release();
    queue.release();
    device.release();
    adapter.release();
    surface.release();
    instance.release();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}