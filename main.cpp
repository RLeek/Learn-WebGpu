#include <iostream>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>
#include <cassert>
#include <vector>
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>


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


    wgpu::Queue queue = device.getQueue();

    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void*) {
        std::cout << "Queued work finished with status: " << status << std::endl;
    };

    wgpuQueueOnSubmittedWorkDone(queue, onQueueWorkDone, nullptr);

    wgpu::SwapChainDescriptor swapChainDesc = {};
    swapChainDesc.width = 640;
    swapChainDesc.height = 480;
    swapChainDesc.format = wgpuSurfaceGetPreferredFormat(surface, adapter);
    swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapChainDesc.presentMode = WGPUPresentMode_Fifo;
    wgpu::SwapChain swapChain = device.createSwapChain(surface, swapChainDesc);
    std::cout << "Swapchain: " << swapChain << std::endl;


    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

        wgpu::TextureView nextTexture = swapChain.getCurrentTextureView();
        if (!nextTexture) {
            std::cerr << "Cannot acquire next swap chain texture" << std::endl;
            break;
        }
        std::cout << "nextTexture: " << nextTexture << std::endl;

        // My guess of what is happening is that we create a command
        // encoder and then begin a render pass using that encoder
        // we then end the render pass without doing anything (so it just clears the screen)
        // we then convert this into a command
        // we then queue this command, which is what actually gets drawn by the GPU

        wgpu::CommandEncoderDescriptor commandEncoderDesc = {};
        commandEncoderDesc.label = "Command Encoder";
        wgpu::CommandEncoder encoder = device.createCommandEncoder(commandEncoderDesc);

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


        wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
        
        renderPass.end();
        renderPass.release();

        nextTexture.release();
        
        wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
        cmdBufferDescriptor.label = "command buffer";
        wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
        encoder.release();
        queue.submit(1, &command);
        command.release();

        swapChain.present();
    }
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