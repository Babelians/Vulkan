#pragma once

#include <vulkan/vulkan.hpp> //必ずglfwの前にインクルード
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <optional>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstring>
#include "triangle.h"
#include "sceneData.h"

using namespace std;

class Vulkan
{
public:
	void run();
private:
	void init();
	void initWindow();
	void createInstance();
	void selectPhysicalDevice();
	void createDevice();
	void createCommandBuffer();
	void createRenderPass();
	void createPipeline();
	void render();
	void createShaders();
	void createImageView();
	void createFramebuffer();
	void createSurface();
	void createSwapchain();
	void present();
	void createFence();
	void createSemaphore();
	void fixSwapchain();
	void createVertexBuffer(void* data, size_t size);
	void createIndexBuffer(void* data, size_t size);
	void createStagingBuffer(void *data, size_t size);
	void createDescriptorSet();
	vk::UniqueDeviceMemory getSuitableDevMem(vk::Buffer buffer, vk::MemoryPropertyFlagBits flag);

	vector<char> readFile(const char* fileName);

	GLFWwindow* window;
	vk::UniqueInstance instance;
	vk::PhysicalDevice physicalDevice;
	vk::UniqueDevice device;
	uint32_t graphicsQueueFamIndex;
	vk::Queue graphicsQueue;
	vk::UniqueCommandPool commandPool;
	vector<vk::UniqueCommandBuffer> commandBuffers;
	vk::UniqueRenderPass renderpass;
	vk::UniquePipeline pipeline;
	vk::UniqueShaderModule vertShader;
	vk::UniqueShaderModule fragShader;
	vk::UniqueImageView imageView;
	vk::SurfaceCapabilitiesKHR surfaceCapabilities;
	vk::UniqueSurfaceKHR surface;
	vk::SurfaceFormatKHR swapchainFormat;
	vk::UniqueSwapchainKHR swapchain;
	vk::PresentModeKHR swapchainPresentMode;
	vector<vk::Image> swapchainImages;
	vector<vk::UniqueImageView> swapchainImageViews;
	vector<vk::UniqueFramebuffer> swapchainFrameBuffers;
	uint32_t imageIndex;
	vk::UniqueFence swapchainImgFence;
	vk::UniqueSemaphore swapchainImgSemaphore, imgRenderedSemaphore;
	vk::UniqueBuffer vertexBuffer;
	vk::UniqueBuffer indexBuffer;
	vk::UniqueBuffer stagingBuffer;
	vk::UniqueBuffer uniformBuffer;
	vk::PhysicalDeviceMemoryProperties physDevMemProps;
	vk::UniqueDeviceMemory stagingBufMemory;
	vk::UniqueDeviceMemory vertDeviceMemory;
	vk::UniqueDeviceMemory idxDeviceMemory;
	vk::UniqueDeviceMemory uniformBufMem;
	vk::UniqueDescriptorSetLayout descriptorSetLayout;
	vk::UniqueDescriptorPool descriptorPool;
	vector<vk::UniqueDescriptorSet> descriptorSets;
	vk::UniquePipelineLayout pipelineLayout;

	Triangle triangle;
	SceneData sceneData = { Vec2{ 0.3f, 0.0f } };

	uint32_t screenWidth = 640, screenHeight = 480;

	vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
};
