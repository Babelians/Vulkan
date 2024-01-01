#include "vulkan.h"

void Vulkan::run()
{
	init();

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		device->waitForFences({ swapchainImgFence.get() }, VK_TRUE, UINT64_MAX); //renderでfenceがシグナル状態になるまで待つ

		vk::ResultValue acquireImgResult = device->acquireNextImageKHR(swapchain.get(), 1'000'000'000, swapchainImgSemaphore.get());

		if (acquireImgResult.result == vk::Result::eSuboptimalKHR || acquireImgResult.result == vk::Result::eErrorOutOfDateKHR)
		{
			cout << "スワップチェーンを再作成します";
			fixSwapchain();
			continue;
		}

		if (acquireImgResult.result != vk::Result::eSuccess) {
			std::cerr << "次フレームの取得に失敗しました。"<< std::endl;
			return;
		}

		device->resetFences({ swapchainImgFence.get() }); //fenceを非シグナル状態にする

		imageIndex = acquireImgResult.value;

		render();

		present();
	}

	graphicsQueue.waitIdle();
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
	createVertexBuffer(triangle.vert.data(), sizeof(Vertex) * triangle.vert.size());
	createIndexBuffer(triangle.indices.data(), sizeof(uint32_t) * triangle.indices.size());
	createFence();
	createSemaphore();
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
			graphicsQueueFamIndex = thisGraphicsQueueIndex.value();
			physDevMemProps = physicalDevice.getMemoryProperties();
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
	deviceQueueCIs[0].queueFamilyIndex = graphicsQueueFamIndex;
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

	graphicsQueue = device->getQueue(graphicsQueueFamIndex, 0);
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
	cmdPoolCI.queueFamilyIndex = graphicsQueueFamIndex;
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

	// デスクリプション
	//binding
	vk::VertexInputBindingDescription vertInputBindingDescription[1];
	vertInputBindingDescription[0].binding = 0;
	vertInputBindingDescription[0].stride = sizeof(Vertex);
	vertInputBindingDescription[0].inputRate = vk::VertexInputRate::eVertex;

	// attribute
	vk::VertexInputAttributeDescription vertInputAttribDescription[3];
	vertInputAttribDescription[0].binding = 0;
	vertInputAttribDescription[0].location = 0;
	vertInputAttribDescription[0].format = vk::Format::eR32G32Sfloat;
	vertInputAttribDescription[0].offset = offsetof(Vertex, pos);
	vertInputAttribDescription[1].binding = 0;
	vertInputAttribDescription[1].location = 1;
	vertInputAttribDescription[1].format = vk::Format::eR32G32B32Sfloat;
	vertInputAttribDescription[1].offset = offsetof(Vertex, color);

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = vertInputBindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = 2;
	vertexInputInfo.pVertexAttributeDescriptions = vertInputAttribDescription;

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
	commandBuffers[0]->bindVertexBuffers(0, { vertexBuffer.get()}, {0});
	commandBuffers[0]->bindIndexBuffer(indexBuffer.get(), 0, vk::IndexType::eUint32);

	// ここでサブパス0番の処理
	commandBuffers[0]->drawIndexed(triangle.indices.size(), 1, 0, 0, 0); //第一引数は頂点の個数

	commandBuffers[0]->endRenderPass();

	commandBuffers[0]->end();

	vk::CommandBuffer submitCmdBuf[1] = { commandBuffers[0].get() };
	vk::Semaphore renderWaitSemaphores[] = { swapchainImgSemaphore.get() };
	vk::Semaphore renderSignalSemaphores[] = { imgRenderedSemaphore.get() };
	vk::PipelineStageFlags renderwaitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	vk::SubmitInfo submitInfo;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = renderWaitSemaphores;
	submitInfo.pWaitDstStageMask = renderwaitStages;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = renderSignalSemaphores;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = submitCmdBuf;

	graphicsQueue.submit({ submitInfo }, swapchainImgFence.get());

	graphicsQueue.waitIdle();
}

void Vulkan::present()
{
	vk::PresentInfoKHR presentInfo;

	auto presentSwapchains = { swapchain.get() };
	auto imgIndices = { imageIndex };

	vk::Semaphore presentWaitSenaphores[] = { imgRenderedSemaphore.get() };

	presentInfo.swapchainCount = static_cast<uint32_t>(presentSwapchains.size());
	presentInfo.pSwapchains = presentSwapchains.begin();
	presentInfo.pImageIndices = imgIndices.begin();
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = presentWaitSenaphores;

	graphicsQueue.presentKHR(presentInfo);
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
	vk::FenceCreateInfo fenceCI;
	fenceCI.flags = vk::FenceCreateFlagBits::eSignaled;
	swapchainImgFence = device->createFenceUnique(fenceCI);
}

void Vulkan::createSemaphore()
{
	vk::SemaphoreCreateInfo semaphoreCI{};

	swapchainImgSemaphore = device->createSemaphoreUnique(semaphoreCI);
	imgRenderedSemaphore = device->createSemaphoreUnique(semaphoreCI);
}

void Vulkan::fixSwapchain()
{
	// swapchain関連のオブジェクトを破棄
	swapchainFrameBuffers.clear();
	swapchainImageViews.clear();
	swapchainImages.clear();
	swapchain.reset();

	createSwapchain();
	createRenderPass();
	createPipeline();
	createImageView();
	createFramebuffer();
}

void Vulkan::createVertexBuffer(void *data, size_t size)
{
	// バッファの作成
	vk::BufferCreateInfo BufferCI{};
	BufferCI.size = size;
	BufferCI.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
	BufferCI.sharingMode = vk::SharingMode::eExclusive;

	vertexBuffer = device->createBufferUnique(BufferCI);

	// デバイスメモリの作成

	vk::MemoryRequirements memReq = device->getBufferMemoryRequirements(vertexBuffer.get());

	vk::MemoryAllocateInfo memAlloc;
	memAlloc.allocationSize = memReq.size;

	bool suitableMemoryTypeFound = false;

	for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; i++)
	{
		if (memReq.memoryTypeBits & (1 << i) && (physDevMemProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal))
		{
			memAlloc.memoryTypeIndex = i;
			suitableMemoryTypeFound = true;
			break;
		}
	}

	if (!suitableMemoryTypeFound) {
		std::cerr << "適切なメモリタイプが存在しません。" << std::endl;
		return;
	}

	vertDeviceMemory = device->allocateMemoryUnique(memAlloc);

	device->bindBufferMemory(vertexBuffer.get(), vertDeviceMemory.get(), 0);

	createStagingBuffer(data, size);

	// コマンドバッファの作成
	vk::CommandPoolCreateInfo cmdPoolCI{};
	cmdPoolCI.queueFamilyIndex = graphicsQueueFamIndex;
	cmdPoolCI.flags = vk::CommandPoolCreateFlagBits::eTransient;
	
	vk::UniqueCommandPool cmdPool = device->createCommandPoolUnique(cmdPoolCI);

	vk::CommandBufferAllocateInfo cmdBufAllocInfo;
	cmdBufAllocInfo.commandPool = cmdPool.get();
	cmdBufAllocInfo.commandBufferCount = 1;
	cmdBufAllocInfo.level = vk::CommandBufferLevel::ePrimary;

	vector<vk::UniqueCommandBuffer> tmpCmdBuffers = device->allocateCommandBuffersUnique(cmdBufAllocInfo);

	// ステージングバッファからデバイスローカルのバッファにコピー
	vk::BufferCopy bufferCopy;
	bufferCopy.srcOffset = 0;
	bufferCopy.dstOffset = 0;
	bufferCopy.size = size;

	vk::CommandBufferBeginInfo cmdBeginInfo;
	cmdBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	tmpCmdBuffers[0]->begin(cmdBeginInfo);
	tmpCmdBuffers[0]->copyBuffer(stagingBuffer.get(), vertexBuffer.get(), { bufferCopy });
	tmpCmdBuffers[0]->end();

	// submit
	vk::CommandBuffer submitCmdBufs[1] = {tmpCmdBuffers[0].get()};
	
	vk::SubmitInfo submitInfo{};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = submitCmdBufs;

	graphicsQueue.submit({ submitInfo });
	graphicsQueue.waitIdle();
}

void Vulkan::createIndexBuffer(void* data, size_t size)
{
	createStagingBuffer(data, size);

	vk::BufferCreateInfo bufferCI;
	bufferCI.size = size;
	bufferCI.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;
	bufferCI.sharingMode = vk::SharingMode::eExclusive;

	indexBuffer = device->createBufferUnique(bufferCI);

	vk::MemoryRequirements memReq = device->getBufferMemoryRequirements(indexBuffer.get());

	vk::MemoryAllocateInfo memAlloc;
	memAlloc.allocationSize = memReq.size;

	bool suitableMemoryTypeFound = false;
	for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; i++) {
		if (memReq.memoryTypeBits & (1 << i) && (physDevMemProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)) {
			memAlloc.memoryTypeIndex = i;
			suitableMemoryTypeFound = true;
			break;
		}
	}
	if (!suitableMemoryTypeFound) {
		std::cerr << "適切なメモリタイプが存在しません。" << std::endl;
		return;
	}

	idxDeviceMemory = device->allocateMemoryUnique(memAlloc);

	device->bindBufferMemory(indexBuffer.get(), idxDeviceMemory.get(), 0);

	// コマンドバッファの作成
	vk::CommandPoolCreateInfo cmdPoolCI{};
	cmdPoolCI.queueFamilyIndex = graphicsQueueFamIndex;
	cmdPoolCI.flags = vk::CommandPoolCreateFlagBits::eTransient;

	vk::UniqueCommandPool cmdPool = device->createCommandPoolUnique(cmdPoolCI);

	vk::CommandBufferAllocateInfo cmdBufAllocInfo;
	cmdBufAllocInfo.commandPool = cmdPool.get();
	cmdBufAllocInfo.commandBufferCount = 1;
	cmdBufAllocInfo.level = vk::CommandBufferLevel::ePrimary;

	vector<vk::UniqueCommandBuffer> tmpCmdBuffers = device->allocateCommandBuffersUnique(cmdBufAllocInfo);

	// ステージングバッファからデバイスローカルのバッファにコピー
	vk::BufferCopy bufferCopy;
	bufferCopy.srcOffset = 0;
	bufferCopy.dstOffset = 0;
	bufferCopy.size = size;

	vk::CommandBufferBeginInfo cmdBeginInfo;
	cmdBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	tmpCmdBuffers[0]->begin(cmdBeginInfo);
	tmpCmdBuffers[0]->copyBuffer(stagingBuffer.get(), indexBuffer.get(), { bufferCopy });
	tmpCmdBuffers[0]->end();

	// submit
	vk::CommandBuffer submitCmdBufs[1] = { tmpCmdBuffers[0].get() };

	vk::SubmitInfo submitInfo{};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = submitCmdBufs;

	graphicsQueue.submit({ submitInfo });
	graphicsQueue.waitIdle();
}

void Vulkan::createStagingBuffer(void* data, size_t size)
{
	// バッファの作成
	vk::BufferCreateInfo BufferCI{};
	BufferCI.size = size;
	BufferCI.usage = vk::BufferUsageFlagBits::eTransferSrc;
	BufferCI.sharingMode = vk::SharingMode::eExclusive;

	stagingBuffer = device->createBufferUnique(BufferCI);

	// デバイスメモリの作成

	vk::MemoryRequirements memReq = device->getBufferMemoryRequirements(stagingBuffer.get());

	vk::MemoryAllocateInfo memAlloc;
	memAlloc.allocationSize = memReq.size;

	bool suitableMemoryTypeFound = false;

	for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; i++)
	{
		if (memReq.memoryTypeBits & (1 << i) && (physDevMemProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible))
		{
			memAlloc.memoryTypeIndex = i;
			suitableMemoryTypeFound = true;
			break;
		}
	}

	if (!suitableMemoryTypeFound) {
		std::cerr << "適切なメモリタイプが存在しません。" << std::endl;
		return;
	}

	// メモリ確保
	stagingBufMemory = device->allocateMemoryUnique(memAlloc);

	// バッファとメモリの結びつけ
	device->bindBufferMemory(stagingBuffer.get(), stagingBufMemory.get(), 0);

	// マッピング
	void* pStagingBufferMem = device->mapMemory(stagingBufMemory.get(), 0, size);

	// メインメモリにコピー
	memcpy(pStagingBufferMem, data, size);

	// デバイスメモリとメインメモリの同期
	vk::MappedMemoryRange flushMemRange;
	flushMemRange.memory = stagingBufMemory.get();
	flushMemRange.offset = 0; //開始
	flushMemRange.size = size; //大きさ

	device->flushMappedMemoryRanges({ flushMemRange });

	device->unmapMemory(stagingBufMemory.get());
}