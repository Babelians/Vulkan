#include "vulkan.h"

void Vulkan::run()
{
	init();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		device->resetFences({ swapchainImgFence.get() });

		vk::ResultValue acquireImgResult = device->acquireNextImageKHR(swapchain.get(), 1'000'000'000, {}, swapchainImgFence.get());
		if (acquireImgResult.result != vk::Result::eSuccess) {
			std::cerr << "次フレームの取得に失敗しました。"<< std::endl;
			return;
		}
		imageIndex = acquireImgResult.value;

		if (device->waitForFences({ swapchainImgFence.get() }, VK_TRUE, 1'000'000'000) != vk::Result::eSuccess) {
			std::cerr << "次フレームの取得に失敗しました。" << std::endl;
			return;
		}

		render();

		present();
	}

	glfwTerminate();
}

void Vulkan::init()
{
	if (!glfwInit())
	{
		throw "glfwInit is failed";
	}
	createInstance();
	initWindow();
	createSurface();
	selectPhysicalDevice();
	createDevice();
	createSwapchain();
	createRenderPass();
	createShaders();
	createPipeline();
	createImageView();
	createFramebuffer();
	createCommandBuffer();
	createFence();
}

void Vulkan::initWindow()
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(screenWidth, screenHeight, "GLFW Test Window", NULL, NULL);
	if (!window) {
		const char* err;
		glfwGetError(&err);
		std::cout << err << std::endl;
		glfwTerminate();
	}
}

void Vulkan::createInstance()
{
	uint32_t requiredExtensionsCount;
	const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionsCount);

	vk::InstanceCreateInfo instanceCI;
	instanceCI.enabledExtensionCount = requiredExtensionsCount;
	instanceCI.ppEnabledExtensionNames = requiredExtensions;
	instanceCI.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	instanceCI.ppEnabledLayerNames = validationLayers.data();
	instance = vk::createInstanceUnique(instanceCI);
}

void Vulkan::createSurface()
{
	VkSurfaceKHR cSurface;
	VkResult result = glfwCreateWindowSurface(instance.get(), window, nullptr, &cSurface);
	if (result != VK_SUCCESS) {
		const char* err;
		glfwGetError(&err);
		std::cout << err << std::endl;
		glfwTerminate();
	}

	surface = vk::UniqueSurfaceKHR( cSurface, instance.get() );
}

void Vulkan::selectPhysicalDevice()
{
	vector<vk::PhysicalDevice> physicalDevices = instance->enumeratePhysicalDevices();
	for (const auto& pd : physicalDevices)
	{
		vector<vk::QueueFamilyProperties>props = pd.getQueueFamilyProperties();
		optional<uint32_t> thisGraphicsQueueIndex;
		for (uint32_t i = 0; i < props.size(); i++)
		{
			// グラフィックス機能に加えてサーフェスへのプレゼンテーションもサポートしているキューを厳選
			if (props[i].queueFlags & vk::QueueFlagBits::eGraphics && pd.getSurfaceSupportKHR(i, surface.get()) )
			{
				thisGraphicsQueueIndex = i;
				break;
			}
		}

		vector<vk::ExtensionProperties> extensionProp = pd.enumerateDeviceExtensionProperties();
		bool supportSwapchain = false;
		for (const auto& ext : extensionProp)
		{
			if (string_view(ext.extensionName.data()) == VK_KHR_SWAPCHAIN_EXTENSION_NAME)
			{
				supportSwapchain = true;
			}
		}

		bool supportsSurface = 
			!pd.getSurfaceFormatsKHR(surface.get()).empty() || 
			!pd.getSurfacePresentModesKHR(surface.get()).empty();

		if (thisGraphicsQueueIndex.has_value() && supportSwapchain && supportsSurface)
		{
			physicalDevice = pd;
			graphicsQueueIndex = thisGraphicsQueueIndex.value();
			return;
		}
	}
	throw runtime_error("failed to find a asuitable GPU!");
}

void Vulkan::createDevice()
{

	auto requireExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	float priorities = 1.0f;
	// 使用するキューを指定する。
	vk::DeviceQueueCreateInfo deviceQueueCIs[1];
	deviceQueueCIs[0].queueFamilyIndex = graphicsQueueIndex;
	deviceQueueCIs[0].queueCount = 1;
	deviceQueueCIs[0].pQueuePriorities = &priorities;

	vk::DeviceCreateInfo deviceCI;
	deviceCI.pQueueCreateInfos = deviceQueueCIs;
	deviceCI.queueCreateInfoCount = 1;
	deviceCI.enabledExtensionCount = static_cast<uint32_t>(requireExtensions.size());
	deviceCI.ppEnabledExtensionNames = requireExtensions.begin();
	deviceCI.enabledLayerCount = uint32_t(validationLayers.size());
	deviceCI.ppEnabledLayerNames = validationLayers.data();

	device = physicalDevice.createDeviceUnique(deviceCI);

	graphicsQueue = device->getQueue(graphicsQueueIndex, 0);
}

void Vulkan::createSwapchain()
{
	surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface.get());
	vector<vk::SurfaceFormatKHR> surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface.get());
	vector<vk::PresentModeKHR> presentModes = physicalDevice.getSurfacePresentModesKHR(surface.get());

	swapchainFormat = surfaceFormats[0];
	swapchainPresentMode = presentModes[0];

	vk::SwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.surface = surface.get();
	swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
	swapchainCreateInfo.imageFormat = swapchainFormat.format;
	swapchainCreateInfo.imageColorSpace = swapchainFormat.colorSpace;
	swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.presentMode = swapchainPresentMode;
	swapchainCreateInfo.clipped = VK_TRUE;

	swapchain = device->createSwapchainKHRUnique(swapchainCreateInfo);
}

void Vulkan::createCommandBuffer()
{
	// コマンドプールの作成
	vk::CommandPoolCreateInfo cmdPoolCI;
	cmdPoolCI.queueFamilyIndex = graphicsQueueIndex;
	cmdPoolCI.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

	commandPool = device->createCommandPoolUnique(cmdPoolCI);

	// コマンドバッファの作成
	vk::CommandBufferAllocateInfo amdBufferAllocInfo;
	amdBufferAllocInfo.commandPool = commandPool.get();
	amdBufferAllocInfo.commandBufferCount = 1;
	amdBufferAllocInfo.level = vk::CommandBufferLevel::ePrimary;

	commandBuffers = device->allocateCommandBuffersUnique(amdBufferAllocInfo);
}

void Vulkan::createRenderPass()
{
	vk::AttachmentDescription attachments[1];
	attachments[0].format = swapchainFormat.format;
	attachments[0].samples = vk::SampleCountFlagBits::e1;
	attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
	attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
	attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[0].initialLayout = vk::ImageLayout::eUndefined;
	attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

	vk::AttachmentReference subpass0_attachmentRefs[1];
	subpass0_attachmentRefs[0].attachment = 0;
	subpass0_attachmentRefs[0].layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::SubpassDescription subpasses[1];
	subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = subpass0_attachmentRefs;

	vk::RenderPassCreateInfo renderPassCI;
	renderPassCI.attachmentCount = 1;
	renderPassCI.pAttachments = attachments;
	renderPassCI.subpassCount = 1;
	renderPassCI.pSubpasses = subpasses;
	renderPassCI.dependencyCount = 0;
	renderPassCI.pDependencies = nullptr;

	renderpass = device->createRenderPassUnique(renderPassCI);
}

void Vulkan::createPipeline()
{
	vk::Viewport viewports[1];
	viewports[0].x = 0.0;
	viewports[0].y = 0.0;
	viewports[0].minDepth = 0.0;
	viewports[0].maxDepth = 1.0;
	viewports[0].width = static_cast<float>(screenWidth);
	viewports[0].height = static_cast<float>(screenHeight);

	vk::Rect2D scissors[1];
	scissors[0].offset = vk::Offset2D({ 0, 0 });
	scissors[0].extent = vk::Extent2D({screenWidth, screenHeight});

	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState.viewportCount = 1;
	viewportState.pViewports = viewports;
	viewportState.scissorCount = 1;
	viewportState.pScissors = scissors;

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssembly.primitiveRestartEnable = false;

	vk::PipelineRasterizationStateCreateInfo rasterizer;
	rasterizer.depthClampEnable = false;
	rasterizer.rasterizerDiscardEnable = false;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = vk::CullModeFlagBits::eBack;
	rasterizer.frontFace = vk::FrontFace::eClockwise;
	rasterizer.depthBiasEnable = false;

	vk::PipelineMultisampleStateCreateInfo multisample;
	multisample.sampleShadingEnable = false;
	multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;

	vk::PipelineColorBlendAttachmentState blendattachment[1];
	blendattachment[0].colorWriteMask =
		vk::ColorComponentFlagBits::eA |
		vk::ColorComponentFlagBits::eR |
		vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB;
	blendattachment[0].blendEnable = false;

	vk::PipelineColorBlendStateCreateInfo blend;
	blend.logicOpEnable = false;
	blend.attachmentCount = 1;
	blend.pAttachments = blendattachment;

	vk::PipelineLayoutCreateInfo layoutCreateInfo;
	layoutCreateInfo.setLayoutCount = 0;
	layoutCreateInfo.pSetLayouts = nullptr;

	vk::UniquePipelineLayout pipelineLayout = device->createPipelineLayoutUnique(layoutCreateInfo);

	vk::PipelineShaderStageCreateInfo shaderStage[2];
	shaderStage[0].stage = vk::ShaderStageFlagBits::eVertex;
	shaderStage[0].module = vertShader.get();
	shaderStage[0].pName = "main";
	shaderStage[1].stage = vk::ShaderStageFlagBits::eFragment;
	shaderStage[1].module = fragShader.get();
	shaderStage[1].pName = "main";

	vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pRasterizationState = &rasterizer;
	pipelineCreateInfo.pMultisampleState = &multisample;
	pipelineCreateInfo.pColorBlendState = &blend;
	pipelineCreateInfo.layout = pipelineLayout.get();
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStage;
	pipelineCreateInfo.renderPass = renderpass.get();
	pipelineCreateInfo.subpass = 0;

	pipeline = device->createGraphicsPipelineUnique(nullptr, pipelineCreateInfo).value;
}

void Vulkan::createImageView()
{
	swapchainImages = device->getSwapchainImagesKHR(swapchain.get());
	swapchainImageViews.resize(swapchainImages.size());
	for (uint32_t i = 0; i < swapchainImages.size(); i++)
	{
		vk::ImageViewCreateInfo imgViewCI;
		imgViewCI.image = swapchainImages[i];
		imgViewCI.viewType = vk::ImageViewType::e2D;
		imgViewCI.format = swapchainFormat.format;
		imgViewCI.components.r = vk::ComponentSwizzle::eIdentity;
		imgViewCI.components.g = vk::ComponentSwizzle::eIdentity;
		imgViewCI.components.b = vk::ComponentSwizzle::eIdentity;
		imgViewCI.components.a = vk::ComponentSwizzle::eIdentity;
		imgViewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		imgViewCI.subresourceRange.baseMipLevel = 0;
		imgViewCI.subresourceRange.levelCount = 1;
		imgViewCI.subresourceRange.baseArrayLayer = 0;
		imgViewCI.subresourceRange.layerCount = 1;

		swapchainImageViews[i] = device->createImageViewUnique(imgViewCI);
	}
}

void Vulkan::createFramebuffer()
{
	swapchainFrameBuffers.resize(swapchainImages.size());
	for (uint32_t i = 0; i < swapchainImages.size(); i++)
	{
		vk::ImageView frameBufferAttachments[1];
		frameBufferAttachments[0] = swapchainImageViews[i].get();

		vk::FramebufferCreateInfo frameBufCreateInfo;
		frameBufCreateInfo.width = surfaceCapabilities.currentExtent.width;
		frameBufCreateInfo.height = surfaceCapabilities.currentExtent.height;
		frameBufCreateInfo.layers = 1;
		frameBufCreateInfo.renderPass = renderpass.get();
		frameBufCreateInfo.attachmentCount = 1;
		frameBufCreateInfo.pAttachments = frameBufferAttachments;

		swapchainFrameBuffers[i] = device->createFramebufferUnique(frameBufCreateInfo);
	}
}

void Vulkan::render()
{
	commandBuffers[0]->reset();
	vk::CommandBufferBeginInfo cmdBeginInfo;
	commandBuffers[0]->begin(cmdBeginInfo);

	vk::ClearValue clearVal[1];
	clearVal[0].color.float32[0] = 0.0f;
	clearVal[0].color.float32[1] = 0.0f;
	clearVal[0].color.float32[2] = 0.0f;
	clearVal[0].color.float32[3] = 1.0f;

	vk::RenderPassBeginInfo renderpassBeginInfo;
	renderpassBeginInfo.renderPass = renderpass.get();
	renderpassBeginInfo.framebuffer = swapchainFrameBuffers[imageIndex].get();
	renderpassBeginInfo.renderArea = vk::Rect2D({ 0,0 }, { screenWidth, screenHeight });
	renderpassBeginInfo.clearValueCount = 1;
	renderpassBeginInfo.pClearValues = clearVal;

	commandBuffers[0]->beginRenderPass(renderpassBeginInfo, vk::SubpassContents::eInline);
	commandBuffers[0]->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());

	// ここでサブパス0番の処理
	commandBuffers[0]->draw(3, 1, 0, 0);

	commandBuffers[0]->endRenderPass();

	commandBuffers[0]->end();

	vk::CommandBuffer submitCmdBuf[1] = { commandBuffers[0].get() };
	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = submitCmdBuf;

	graphicsQueue.submit({ submitInfo }, nullptr);

	graphicsQueue.waitIdle();
}

void Vulkan::present()
{
	vk::PresentInfoKHR presentInfo;

	auto presentSwapchains = { swapchain.get() };
	auto imgIndices = { imageIndex };

	presentInfo.swapchainCount = static_cast<uint32_t>(presentSwapchains.size());
	presentInfo.pSwapchains = presentSwapchains.begin();
	presentInfo.pImageIndices = imgIndices.begin();

	graphicsQueue.presentKHR(presentInfo);

	graphicsQueue.waitIdle();
}

void Vulkan::createShaders()
{
	vector<char> vertSpv = readFile("shaders/shader.vert.spv");
	vector<char> fragSpv = readFile("shaders/shader.frag.spv");

	vk::ShaderModuleCreateInfo vertShaderCI;
	vertShaderCI.codeSize = vertSpv.size();
	vertShaderCI.pCode = reinterpret_cast<const uint32_t*>(vertSpv.data());

	vk::ShaderModuleCreateInfo fragShaderCI;
	fragShaderCI.codeSize = fragSpv.size();
	fragShaderCI.pCode = reinterpret_cast<const uint32_t*>(fragSpv.data());

	vertShader = device->createShaderModuleUnique(vertShaderCI);
	fragShader = device->createShaderModuleUnique(fragShaderCI);
}

vector<char> Vulkan::readFile(const char* fileName)
{
	
	ifstream ifs(fileName, ios::binary);
	if (!ifs.is_open())
	{
		cout << "file open is failed!";
	}
	uintmax_t fileSize = filesystem::file_size(fileName);
	vector<char> fileData(fileSize);

	ifs.read(fileData.data(), fileSize);

	return fileData;
}

void Vulkan::createFence()
{
	vk::FenceCreateInfo fenceCreateInfo;
	swapchainImgFence = device->createFenceUnique(fenceCreateInfo);
}