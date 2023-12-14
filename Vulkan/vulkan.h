#pragma once

#include <vulkan/vulkan.hpp> //必ずglfwの前にインクルード
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <optional>
#include <fstream>
#include <filesystem>
#include <string>

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

	vector<char> readFile(const char* fileName);

	GLFWwindow* window;
	vk::UniqueInstance instance;
	vk::PhysicalDevice physicalDevice;
	vk::UniqueDevice device;
	uint32_t graphicsQueueIndex;
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

	uint32_t screenWidth = 640, screenHeight = 480;

	vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
};
