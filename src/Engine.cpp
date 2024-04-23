#include "Engine.hpp"

#include <iostream>

#include <VkBootstrap.h>

#include "ErrorCheck.hpp"
#include "Image.hpp"

Engine::Engine()
{
    m_Window = std::make_unique<Window>();
    m_Window->init("LearnOpenGL-Vulkan", { 800, 800 });

    m_Window->attachObserver(this);

    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructures();

    mainLoop();
}

Engine::~Engine()
{
    vkDeviceWaitIdle(m_Device);
    cleanup();
}

void Engine::receiveEvent(const Event* event)
{
    switch (event->getType())
    {
    case EventType::KEYBOARD_PRESS:
        {
            std::cout << "KEY PRESS\n";
            break;
        }
    default:
        break;
    }
}

void Engine::cleanup()
{
    for (size_t i = 0; i < m_Frames.size(); i++)
    {
        vkDestroySemaphore(m_Device, m_Frames[i].renderSemaphore, nullptr);
        vkDestroySemaphore(m_Device, m_Frames[i].swapchainSemaphore, nullptr);
        vkDestroyFence(m_Device, m_Frames[i].renderFence, nullptr);

        vkDestroyCommandPool(m_Device, m_Frames[i].commandPool, nullptr);
    }

    m_DrawImage.destroy(m_Device, m_Allocator);
    destroySwapchain();

    vmaDestroyAllocator(m_Allocator);

    vkDestroyDevice(m_Device, nullptr);

    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

    vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger);
    vkDestroyInstance(m_Instance, nullptr);
}

void Engine::initVulkan()
{
    vkb::InstanceBuilder builder;
    auto instRet = builder.set_app_name("LearnOpenGL-Vulkan")
                       .request_validation_layers(true)
                       .use_default_debug_messenger()
                       .require_api_version(1, 3, 0)
                       .build();

    vkb::Instance vkbInst = instRet.value();
    m_Instance = vkbInst.instance;
    m_DebugMessenger = vkbInst.debug_messenger;

    m_Surface = m_Window->getSurface(m_Instance);

    VkPhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    VkPhysicalDeviceFeatures features{};
    features.robustBufferAccess = true;

    vkb::PhysicalDeviceSelector selector{ vkbInst };
    vkb::PhysicalDevice vkbPhysicalDevice = selector.set_minimum_version(1, 3)
                                                .set_required_features_13(features13)
                                                .set_required_features_12(features12)
                                                .set_required_features(features)
                                                .set_surface(m_Surface)
                                                .select()
                                                .value();

    vkb::DeviceBuilder deviceBuilder{ vkbPhysicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

    m_PhysicalDevice = vkbPhysicalDevice.physical_device;
    m_Device = vkbDevice.device;

    m_GraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    m_GraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorCI{};
    allocatorCI.physicalDevice = m_PhysicalDevice;
    allocatorCI.device = m_Device;
    allocatorCI.instance = m_Instance;
    allocatorCI.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorCI, &m_Allocator);
}

void Engine::destroySwapchain()
{
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

    for (size_t i = 0; i < m_SwapchainImageViews.size(); i++)
    {
        vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
    }

    m_SwapchainImages.clear();
    m_SwapchainImageViews.clear();
}

void Engine::createSwapchain()
{
    vkb::SwapchainBuilder swapchainBuilder{ m_PhysicalDevice, m_Device, m_Surface };

    m_SwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            .set_desired_format({ .format = m_SwapchainImageFormat,
                                  .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(m_Window->getSize().x, m_Window->getSize().y)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

    m_Swapchain = vkbSwapchain.swapchain;
    m_SwapchainImageExtent = vkbSwapchain.extent;
    m_SwapchainImages = vkbSwapchain.get_images().value();
    m_SwapchainImageViews = vkbSwapchain.get_image_views().value();
}

void Engine::initSwapchain()
{
    createSwapchain();

    m_DrawImage.create(m_Device, m_Allocator,
                       { (uint32_t)m_Window->getSize().x, (uint32_t)m_Window->getSize().y, 1 },
                       VK_FORMAT_R16G16B16A16_UNORM,
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
}

void Engine::initCommands()
{
    VkCommandPoolCreateInfo commandPoolCI{};
    commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCI.pNext = nullptr;
    commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCI.queueFamilyIndex = m_GraphicsQueueFamily;

    for (size_t i = 0; i < m_Frames.size(); i++)
    {
        VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolCI, nullptr, &m_Frames[i].commandPool));

        VkCommandBufferAllocateInfo commandBufferAI{};
        commandBufferAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAI.pNext = nullptr;
        commandBufferAI.commandPool = m_Frames[i].commandPool;
        commandBufferAI.commandBufferCount = 1;
        commandBufferAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        VK_CHECK(
            vkAllocateCommandBuffers(m_Device, &commandBufferAI, &m_Frames[i].mainCommandBuffer));
    }
}

void Engine::initSyncStructures()
{
    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCI.pNext = nullptr;
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreCI{};
    semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCI.pNext = nullptr;

    for (size_t i = 0; i < m_Frames.size(); i++)
    {
        VK_CHECK(vkCreateFence(m_Device, &fenceCI, nullptr, &m_Frames[i].renderFence));

        VK_CHECK(
            vkCreateSemaphore(m_Device, &semaphoreCI, nullptr, &m_Frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCI, nullptr, &m_Frames[i].renderSemaphore));
    }
}

FrameData& Engine::getCurrentFrame() { return m_Frames[m_CurrentFrame % MAX_FRAMES_IN_FLIGHT]; }

void Engine::render()
{
    VK_CHECK(vkWaitForFences(m_Device, 1, &getCurrentFrame().renderFence, true, 1e9));

    uint32_t swapchainImageIndex;
    {
        VkResult result =
            vkAcquireNextImageKHR(m_Device, m_Swapchain, 1e9, getCurrentFrame().swapchainSemaphore,
                                  nullptr, &swapchainImageIndex);
    }

    VK_CHECK(vkResetFences(m_Device, 1, &getCurrentFrame().renderFence));

    VkCommandBuffer cmd = getCurrentFrame().mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo commandBufferBI{};
    commandBufferBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBI.pNext = nullptr;
    commandBufferBI.pInheritanceInfo = nullptr;
    commandBufferBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferBI));

    ImageAllocation::transition(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    ImageAllocation::transition(cmd, m_SwapchainImages[swapchainImageIndex],
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkExtent2D drawExtent = { m_DrawImage.imageExtent.width, m_DrawImage.imageExtent.height };
    ImageAllocation::copyImgToImg(cmd, m_DrawImage.image, m_SwapchainImages[swapchainImageIndex],
                                  drawExtent, m_SwapchainImageExtent);

    ImageAllocation::transition(cmd, m_SwapchainImages[swapchainImageIndex],
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo commandBufferSI{};
    commandBufferSI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferSI.pNext = nullptr;
    commandBufferSI.commandBuffer = cmd;
    commandBufferSI.deviceMask = 0;

    VkSemaphoreSubmitInfo waitSI{};
    waitSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSI.pNext = nullptr;
    waitSI.semaphore = getCurrentFrame().swapchainSemaphore;
    waitSI.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    waitSI.deviceIndex = 0;
    waitSI.value = 1;

    VkSemaphoreSubmitInfo signalSI{};
    signalSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSI.pNext = nullptr;
    signalSI.semaphore = getCurrentFrame().renderSemaphore;
    signalSI.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    signalSI.deviceIndex = 0;
    signalSI.value = 1;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitSI;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalSI;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSI;

    VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submitInfo, getCurrentFrame().renderFence));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &getCurrentFrame().renderSemaphore;
    presentInfo.pImageIndices = &swapchainImageIndex;

    {
        VkResult result = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);
    }

    m_CurrentFrame++;
}

void Engine::mainLoop()
{
    while (!m_Window->shouldClose())
    {
        m_Window->getEvents();

        // glfwSetWindowShouldClose(m_Window->getWindow(), true);

        render();

        m_Window->swapBuffers();
    }
}
