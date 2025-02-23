#include "RenderAPI_VulkanRayQuery.h"
#include "NativeLogger.h"

#include "VulkanRTShader.h"
#include "VulkanRTData.h"
#include <array>

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}


VkDevice RenderAPI_VulkanRayQuery::NullDevice = VK_NULL_HANDLE;
bool RenderAPI_VulkanRayQuery::CreateDeviceSuccess = false;
bool RenderAPI_VulkanRayQuery::OnPluginLoad = false;
bool RenderAPI_VulkanRayQuery::UseLegacyVulkanLoader = false;


static int FindMemoryTypeIndex(VkPhysicalDeviceMemoryProperties const& physicalDeviceMemoryProperties, VkMemoryRequirements const& memoryRequirements, VkMemoryPropertyFlags memoryPropertyFlags)
{
	uint32_t memoryTypeBits = memoryRequirements.memoryTypeBits;

	// Search memtypes to find first index with those properties
	for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < VK_MAX_MEMORY_TYPES; ++memoryTypeIndex)
	{
		if ((memoryTypeBits & 1) == 1)
		{
			// Type is available, does it match user properties?
			if ((physicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)
				return memoryTypeIndex;
		}
		memoryTypeBits >>= 1;
	}

	return -1;
}

RenderAPI_VulkanRayQuery::RenderAPI_VulkanRayQuery()
	: m_UnityVulkan(NULL)
	, device_(NullDevice)

	, graphicsInterface_(nullptr)
	, graphicsQueueFamilyIndex_(0)
	, transferQueueFamilyIndex_(0)
	, graphicsQueue_(VK_NULL_HANDLE)
	, transferQueue_(VK_NULL_HANDLE)
	, computeQueue_(VK_NULL_HANDLE)
	, graphicsCommandPool_(VK_NULL_HANDLE)
	, transferCommandPool_(VK_NULL_HANDLE)
	, physicalDeviceMemoryProperties_(VkPhysicalDeviceMemoryProperties())
	, rayTracingProperties_(VkPhysicalDeviceRayTracingPipelinePropertiesKHR())

	, alreadyPrepared_(false)
	, alreadyProcessEvent(false)
	, rebuildTlas_(true)
	, updateTlas_(false)
	, tlas_(VulkanRT::VulkanRTData::RayTracerAccelerationStructure())
	, descriptorPool_(VK_NULL_HANDLE)

	//rt query
	, globalUniformBufferInfo(VkDescriptorBufferInfo())
	, rayQueryPipelieLayout(VK_NULL_HANDLE)
{

}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	std::string msg = std::string(pCallbackData->pMessage);

	// Only print validation errors
	if (msg.rfind("Validation Error:", 0) == 0)
	{
		NativeLogger::LogError(msg.c_str());
	}

	return VK_FALSE;
}

static const char* vkResultToString(VkResult result) {
	switch (result) {
	case VK_SUCCESS: return "VK_SUCCESS";
	case VK_NOT_READY: return "VK_NOT_READY";
	case VK_TIMEOUT: return "VK_TIMEOUT";
	case VK_EVENT_SET: return "VK_EVENT_SET";
	case VK_EVENT_RESET: return "VK_EVENT_RESET";
	case VK_INCOMPLETE: return "VK_INCOMPLETE";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
	case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
	case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
	case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
	case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
	case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
	case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
	case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
	case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
	case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
	case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
	case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
	case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
	case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
	case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
	case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
	case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
	case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
	case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
	case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
	case VK_ERROR_NOT_PERMITTED_KHR: return "VK_ERROR_NOT_PERMITTED_KHR";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
	case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
	case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
	case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
	case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
	case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR: return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
	case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
	case VK_INCOMPATIBLE_SHADER_BINARY_EXT: return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
	default: return "VK_RESULT_MAX_ENUM or unknown value";
	}
}


VkResult CreateInstance_RayQuery(const VkInstanceCreateInfo* unityCreateInfo, const VkAllocationCallbacks* unityAllocator, VkInstance* instance)
{
	// Copy ApplicationInfo from Unity and force it up to Vulkan 1.2
	VkApplicationInfo applicationInfo;
	applicationInfo.sType = unityCreateInfo->pApplicationInfo->sType;
	applicationInfo.apiVersion = VK_API_VERSION_1_2;
	applicationInfo.applicationVersion = unityCreateInfo->pApplicationInfo->applicationVersion;
	applicationInfo.engineVersion = unityCreateInfo->pApplicationInfo->engineVersion;
	applicationInfo.pApplicationName = unityCreateInfo->pApplicationInfo->pApplicationName;
	applicationInfo.pEngineName = unityCreateInfo->pApplicationInfo->pEngineName;
	applicationInfo.pNext = unityCreateInfo->pApplicationInfo->pNext;

	// Define Vulkan create information for instance
	VkInstanceCreateInfo createInfo;
	createInfo.pNext = nullptr;
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.flags = 0;
	createInfo.pApplicationInfo = &applicationInfo;

	// At time of writing this was 0, make sure Unity didn't change anything
	assert(unityCreateInfo->enabledLayerCount == 0);

	// Depending if we have validations layers defined, populate debug messenger create info
	//VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	const char* layerName = "VK_LAYER_KHRONOS_validation";
	const char* renderDocLayerName = "VK_LAYER_RENDERDOC_Capture";

	std::vector<const char*> enabledLayerNames = {
		layerName,
		//renderDocLayerName
	};

	//Add validateLayer
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayerNames.size());
		createInfo.ppEnabledLayerNames = enabledLayerNames.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		debugCreateInfo = {};
		debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugCreateInfo.pfnUserCallback = debugCallback;

		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	// Gather extensions required for instaqnce
	std::vector<const char*> requiredExtensions = {
		//this extension may not support in android?
		VK_KHR_DISPLAY_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME,

#ifdef UNITY_ANDROID
		VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#endif


#ifdef WIN32
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif

#ifndef NDEBUG
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME  // Enable when debugging of Vulkan is needed
#endif

	};

	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();


	NativeLogger::LogInfo("Unity Builtin ExtensionNames");
	for (uint32_t i = 0; i < unityCreateInfo->enabledExtensionCount; ++i) {
		NativeLogger::LogIntInfo(i);
		NativeLogger::LogInfo(unityCreateInfo->ppEnabledExtensionNames[i]);
	}
	NativeLogger::LogInfo("Unity Builtin ExtensionNames End");


	NativeLogger::LogInfo("Custom ExtensionNames");
	VkInstanceCreateInfo* pCustomCreateInfo = &createInfo;
	for (uint32_t i = 0; i < pCustomCreateInfo->enabledExtensionCount; ++i) {
		NativeLogger::LogIntInfo(i);
		NativeLogger::LogInfo(pCustomCreateInfo->ppEnabledExtensionNames[i]);
	}
	NativeLogger::LogInfo("Custom ExtensionNames End");


	vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
	VkResult result = vkCreateInstance(&createInfo, unityAllocator, instance);
	//VK_CHECK("vkCreateInstance", result);

	if (result == VK_SUCCESS)
	{
		
	}
	else {
		NativeLogger::LogInfo("Create VulkanInstance Failed");

		auto ret = vkResultToString(result);

		NativeLogger::LogInfo(ret);
		NativeLogger::LogInfo("End");
	}


	return result;
}

static std::vector<const char*> s_RayQueryphysicalDeviceExtensionVec;

void ResolvePropertiesAndQueues_RayQuery(VkPhysicalDevice physicalDevice) {

	// find our queues
	const VkQueueFlagBits askingFlags[3] = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_COMPUTE_BIT };
	uint32_t queuesIndices[3] = { ~0u,  ~0u,  ~0u };

	// Call once to get the count
	uint32_t queueFamilyPropertyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, nullptr);

	// Call again to populate vector
	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

	//VkPhysicalDeviceProperties normalProperty;
	//vkGetPhysicalDeviceProperties(physicalDevice, &normalProperty);

	//// 输出API版本号
	//uint32_t major = VK_VERSION_MAJOR(normalProperty.apiVersion);
	//uint32_t minor = VK_VERSION_MINOR(normalProperty.apiVersion);
	//uint32_t patch = VK_VERSION_PATCH(normalProperty.apiVersion);

	//NativeLogger::LogInfoFormat("Current physical device Vulkan version %d-%d-%d", major, minor, patch);

	// Locate which queueFamilyProperties index 
	for (size_t i = 0; i < 3; ++i) {
		const VkQueueFlagBits flag = askingFlags[i];
		uint32_t& queueIdx = queuesIndices[i];

		if (flag == VK_QUEUE_TRANSFER_BIT) {
			for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j) {
				if ((queueFamilyProperties[j].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
					!(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
					) {
					queueIdx = j;
					break;
				}
			}
		}

		if (flag == VK_QUEUE_COMPUTE_BIT)
		{
			if (queuesIndices[0] != ~0u)
			{
				int idx = queuesIndices[0];
				for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j) {
					if (j != idx && (queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT))
					{
						queueIdx = j;
						break;
					}
				}
			}
		}

		// If an index wasn't return by the above, just find any queue that supports the flag
		if (queueIdx == ~0u) {
			for (uint32_t j = 0; j < queueFamilyPropertyCount; ++j) {
				if (queueFamilyProperties[j].queueFlags & flag) {
					queueIdx = j;
					break;
				}
			}
		}
	}

	if (queuesIndices[0] == ~0u || queuesIndices[1] == ~0u || queuesIndices[2] == ~0u) {
		NativeLogger::LogInfo("Could not find queues to support all required flags");
		return;
	}

	RenderAPI_VulkanRayQuery::Instance().graphicsQueueFamilyIndex_ = queuesIndices[0];
	RenderAPI_VulkanRayQuery::Instance().transferQueueFamilyIndex_ = queuesIndices[1];
	RenderAPI_VulkanRayQuery::Instance().computeQuueueFamilyIndex = queuesIndices[2];

	NativeLogger::LogInfo("Queues indices successfully reoslved");


	VkPhysicalDeviceProperties2 physicalDeviceProperties = { };
	physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalDeviceProperties.pNext = &RenderAPI_VulkanRayQuery::Instance().rayTracingProperties_;

	NativeLogger::LogInfo("Getting physical device properties");
	vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties);

	// Get memory properties
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &RenderAPI_VulkanRayQuery::Instance().physicalDeviceMemoryProperties_);
}


VkResult CreateDevice_RayQuery(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* unityCreateInfo, const VkAllocationCallbacks* unityAllocator, VkDevice* device)
{
	ResolvePropertiesAndQueues_RayQuery(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
	const float priority = 0.0f;

	//  Setup device queue
	VkDeviceQueueCreateInfo deviceQueueCreateInfo;
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = RenderAPI_VulkanRayQuery::Instance().graphicsQueueFamilyIndex_;
	deviceQueueCreateInfo.queueCount = 1;
	deviceQueueCreateInfo.pQueuePriorities = &priority;
	deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);

	// Determine if we have individual queues that need to be added 
	if (RenderAPI_VulkanRayQuery::Instance().transferQueueFamilyIndex_ != RenderAPI_VulkanRayQuery::Instance().graphicsQueueFamilyIndex_) {
		deviceQueueCreateInfo.queueFamilyIndex = RenderAPI_VulkanRayQuery::Instance().transferQueueFamilyIndex_;
		deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
	}

	if (RenderAPI_VulkanRayQuery::Instance().computeQuueueFamilyIndex != RenderAPI_VulkanRayQuery::Instance().graphicsQueueFamilyIndex_) {
		deviceQueueCreateInfo.queueFamilyIndex = RenderAPI_VulkanRayQuery::Instance().computeQuueueFamilyIndex;
		deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
	}

	// Setup required features backwards for chaining
	VkPhysicalDeviceDescriptorIndexingFeatures physicalDeviceDescriptorIndexingFeatures = { };
	physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	physicalDeviceDescriptorIndexingFeatures.pNext = nullptr;

	VkPhysicalDeviceRayQueryFeaturesKHR physicalDeviceRayQueryFeatures = { };
	physicalDeviceRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	physicalDeviceRayQueryFeatures.rayQuery = VK_TRUE;
	physicalDeviceRayQueryFeatures.pNext = &physicalDeviceDescriptorIndexingFeatures;

	VkPhysicalDeviceBufferDeviceAddressFeatures physicalDeviceBufferDeviceAddressFeatures = { };
	physicalDeviceBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	physicalDeviceBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
	physicalDeviceBufferDeviceAddressFeatures.pNext = &physicalDeviceRayQueryFeatures;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR physicalDeviceAccelerationStructureFeatures = {};
	physicalDeviceAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	physicalDeviceAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
	physicalDeviceAccelerationStructureFeatures.pNext = &physicalDeviceBufferDeviceAddressFeatures;

	VkPhysicalDeviceFeatures2 deviceFeatures = { };
	deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures.pNext = &physicalDeviceAccelerationStructureFeatures;

	vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures); // enable all the features our GPU has

	// Setup extensions required for ray tracing.  Rebuild from Unity
	std::vector<const char*> requiredExtensions = {
		// Required by Unity3D
		VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
		VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
		VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
		VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
		VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
		VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME,
		VK_KHR_MAINTENANCE2_EXTENSION_NAME,
		VK_KHR_MULTIVIEW_EXTENSION_NAME,
		VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
		VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME,
		VK_EXT_HDR_METADATA_EXTENSION_NAME,
		VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,

		// Required by Raytracing RayQuert
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_QUERY_EXTENSION_NAME,

		VK_KHR_SPIRV_1_4_EXTENSION_NAME,

		// Required by SPRIV 1.4
		//VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,

		// Required by acceleration structure
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
	};

	for (auto const& requiredExt : requiredExtensions) {
		bool finded = false;
		for (auto const& supportExt : s_RayQueryphysicalDeviceExtensionVec) {
			if (strcmp(requiredExt, supportExt) == 0)
			{
				finded = true;
				break;
			}
		}

		if (!finded) {
			LOGI("Current Ext Is Not Support %s", requiredExt);
#ifdef UNITY_WIN
			NativeLogger::LogInfoFormat("Current Ext Is Not Support %s", requiredExt);
#endif
		}
		else {
#ifdef UNITY_WIN
			LOGI("Current Ext Is Support %s", requiredExt);
#endif
			NativeLogger::LogInfoFormat("Current Ext Is Support %s", requiredExt);
		}
	}

	VkDeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &deviceFeatures;
	deviceCreateInfo.flags = VK_NO_FLAGS;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();
	deviceCreateInfo.pEnabledFeatures = nullptr; // Must be null since pNext != nullptr

	// Inject parts of Unity's VkDeviceCreateInfo just in case
	deviceCreateInfo.flags = unityCreateInfo->flags;
	deviceCreateInfo.enabledLayerCount = unityCreateInfo->enabledLayerCount;
	deviceCreateInfo.ppEnabledLayerNames = unityCreateInfo->ppEnabledLayerNames;

	VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, unityAllocator, device);

	if (!result == VK_SUCCESS)
	{
		NativeLogger::LogInfo("Create VulkanDevice Failed");
		NativeLogger::LogInfo(vkResultToString(result));
		return result;
	}


	// get our queues handles with our new logical device!
	NativeLogger::LogInfo("Getting device queues");

	vkGetDeviceQueue(*device, RenderAPI_VulkanRayQuery::Instance().graphicsQueueFamilyIndex_, 0, &RenderAPI_VulkanRayQuery::Instance().graphicsQueue_);
	vkGetDeviceQueue(*device, RenderAPI_VulkanRayQuery::Instance().transferQueueFamilyIndex_, 0, &RenderAPI_VulkanRayQuery::Instance().transferQueue_);
	vkGetDeviceQueue(*device, RenderAPI_VulkanRayQuery::Instance().computeQuueueFamilyIndex, 0, &RenderAPI_VulkanRayQuery::Instance().computeQueue_);

	if (result == VK_SUCCESS)
	{
		RenderAPI_VulkanRayQuery::CreateDeviceSuccess = true;
	}

	return result;
}

static void CheckVKExtension()
{
	// 初始化变量以存储扩展的数量
	uint32_t extensionCount = 0;

	NativeLogger::LogInfo("CheckVKExtension");

	// 首先调用 vkEnumerateInstanceExtensionProperties 获取扩展数量
	if (!vkEnumerateInstanceExtensionProperties) {
		vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceExtensionProperties");
	}
	VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);


	if (result != VK_SUCCESS) {
		NativeLogger::LogInfo("Failed to get extension count.");
		return;
	}
	else {
		NativeLogger::LogInfo("Ext Count");
		NativeLogger::LogIntInfo(extensionCount);
	}

	// 分配足够的空间来存储所有的扩展信息
	std::vector<VkExtensionProperties> extensions(extensionCount);

	// 再次调用 vkEnumerateInstanceExtensionProperties 获取所有扩展的详细信息
	result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	if (result != VK_SUCCESS) {
		NativeLogger::LogInfo("Failed to get extension properties.");
		return;
	}
	else {
		NativeLogger::LogInfo("Good to get extension properties.");
	}

	// 打印所有找到的扩展
	NativeLogger::LogInfo("Available Vulkan instance extensions:");
	for (const auto& extension : extensions) {
		//std::cout << "\t" << extension.extensionName << " (spec version " << extension.specVersion << ")" << std::endl;
		NativeLogger::LogInfo(extension.extensionName);
	}
	NativeLogger::LogInfo("Available Vulkan instance extensions end");

}

static void CheckPhysicalDeviceExtension(VkPhysicalDevice physicalDevice)
{
	// 初始化变量以存储扩展的数量
	uint32_t extensionCount = 0;

	NativeLogger::LogInfo("CheckPhysicalDeviceExtension");

	if (!vkEnumerateDeviceExtensionProperties) {
		vkEnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)vkGetInstanceProcAddr(NULL, "vkEnumerateDeviceExtensionProperties");
	}

	VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

	if (result != VK_SUCCESS) {
		NativeLogger::LogInfo("Failed to get extension count.");
		return;
	}
	else {
		NativeLogger::LogInfo("Ext Count");
		NativeLogger::LogIntInfo(extensionCount);
	}

	// 分配足够的空间来存储所有的扩展信息
	std::vector<VkExtensionProperties> deviceExtensions(extensionCount);

	// 再次调用 vkEnumerateDeviceExtensionProperties 获取所有扩展的详细信息
	result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, deviceExtensions.data());
	if (result != VK_SUCCESS) {
		NativeLogger::LogInfo("Failed to get extension properties.");
		return;
	}
	else {
		NativeLogger::LogInfo("Good to get extension properties.");
	}

	// 打印所有找到的扩展
	NativeLogger::LogInfo("Available Vulkan instance extensions:");
	for (const auto& extension : deviceExtensions) {
		//std::cout << "\t" << extension.extensionName << " (spec version " << extension.specVersion << ")" << std::endl;
		NativeLogger::LogInfo(extension.extensionName);
		s_RayQueryphysicalDeviceExtensionVec.push_back(extension.extensionName);
	}
	NativeLogger::LogInfo("Available Vulkan instance extensions end");
}

static void LoadVulkanAPI(PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkInstance instance)
{
	if (!vkGetInstanceProcAddr && getInstanceProcAddr)
		vkGetInstanceProcAddr = getInstanceProcAddr;

	if (!vkCreateInstance)
		vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
}

static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
	NativeLogger::LogInfo("Vulkan Hook_vkCreateInstance");

	LoadVulkanAPI(vkGetInstanceProcAddr, *pInstance);

	VkResult result = CreateInstance_RayQuery(pCreateInfo, pAllocator, pInstance);

	if (result == VK_SUCCESS)
	{
		NativeLogger::LogInfo("Vulkan Hook_vkCreateInstance Success");
		volkLoadInstance(*pInstance);

		//Logout all support vulkan extension on device
		CheckVKExtension();
	}

	return result;
}



static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
	NativeLogger::LogInfo("Vulkan Hook_vkCreateDevice");

	CheckPhysicalDeviceExtension(physicalDevice);

	VkResult result = CreateDevice_RayQuery(physicalDevice, pCreateInfo, pAllocator, pDevice);

	if (result != VK_SUCCESS) {
		NativeLogger::LogInfo("Hook_vkCreateDevice Failed");
	}
	else {
		NativeLogger::LogInfo("Hook_vkCreateDevice Success");
	}

	return result;
}

static VKAPI_ATTR void VKAPI_CALL Hook_vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents)
{
	// Change this to 'true' to override the clear color with green
	const bool allowOverrideClearColor = true;
	if (pRenderPassBegin->clearValueCount <= 16 && pRenderPassBegin->clearValueCount > 0 && allowOverrideClearColor)
	{
		VkClearValue clearValues[16] = {};
		memcpy(clearValues, pRenderPassBegin->pClearValues, pRenderPassBegin->clearValueCount * sizeof(VkClearValue));

		VkRenderPassBeginInfo patchedBeginInfo = *pRenderPassBegin;
		patchedBeginInfo.pClearValues = clearValues;
		for (unsigned int i = 0; i < pRenderPassBegin->clearValueCount - 1; ++i)
		{
			clearValues[i].color.float32[0] = 0.0;
			clearValues[i].color.float32[1] = 1.0;
			clearValues[i].color.float32[2] = 0.0;
			clearValues[i].color.float32[3] = 1.0;
		}
		vkCmdBeginRenderPass(commandBuffer, &patchedBeginInfo, contents);
	}
	else
	{
		vkCmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
	}
}
	

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Hook_vkGetInstanceProcAddr(VkInstance device, const char* funcName)
{
	if (!funcName)
		return NULL;


#define INTERCEPT(fn) if (strcmp(funcName, #fn) == 0) return (PFN_vkVoidFunction)&Hook_##fn
	INTERCEPT(vkCreateInstance);
	INTERCEPT(vkCreateDevice);
	INTERCEPT(vkCmdBeginRenderPass);
#undef INTERCEPT

	//return vkGetInstanceProcAddr(device, funcName);
	return NULL;
}

static PFN_vkGetInstanceProcAddr UNITY_INTERFACE_API InterceptVulkanInitialization(PFN_vkGetInstanceProcAddr getInstanceProcAddr, void*)
{
	NativeLogger::LogInfo("Vulkan GetInstanceProcAddr");
	vkGetInstanceProcAddr = getInstanceProcAddr;
	return Hook_vkGetInstanceProcAddr;
}


extern "C" void RenderAPI_VulkanRayQuery_OnPluginLoad(IUnityInterfaces* interfaces)
{
	//NativeLogger::LogInfo("Vulkan AddIntercept");

	///*if (IUnityGraphicsVulkanV2* vulkanInterface = interfaces->Get<IUnityGraphicsVulkanV2>())
	//	vulkanInterface->AddInterceptInitialization(InterceptVulkanInitialization, NULL, 0);
	//else if (IUnityGraphicsVulkan* vulkanInterface = interfaces->Get<IUnityGraphicsVulkan>())
	//	vulkanInterface->InterceptInitialization(InterceptVulkanInitialization, NULL);*/

	RenderAPI_VulkanRayQuery::OnPluginLoad = true;
	RenderAPI_VulkanRayQuery::UseLegacyVulkanLoader = false;
	//   //RT Hook
	if (IUnityGraphicsVulkanV2* vulkanInterface = interfaces->Get<IUnityGraphicsVulkanV2>())
		vulkanInterface->InterceptInitialization(InterceptVulkanInitialization, NULL);
	else if (IUnityGraphicsVulkan* vulkanInterface = interfaces->Get<IUnityGraphicsVulkan>())
		vulkanInterface->InterceptInitialization(InterceptVulkanInitialization, NULL);
}

RenderAPI* CreateRenderAPI_VulkanRayQuery()
{
	return &RenderAPI_VulkanRayQuery::Instance();
}

bool RenderAPI_VulkanRayQuery::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	switch (type)
	{
	case kUnityGfxDeviceEventInitialize:
		m_UnityVulkan = interfaces->Get<IUnityGraphicsVulkan>();
		UnityVulkanPluginEventConfig eventConfig;
		//eventConfig.graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_Allow;
		//eventConfig.renderPassPrecondition = kUnityVulkanRenderPass_EnsureOutside;
		eventConfig.graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_DontCare;
		eventConfig.renderPassPrecondition = kUnityVulkanRenderPass_EnsureInside;
		eventConfig.flags = kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission | kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState;
		m_UnityVulkan->ConfigureEvent(1, &eventConfig);

		InitializeFromUnityInstance(m_UnityVulkan);

		alreadyProcessEvent = true;

		break;
	case kUnityGfxDeviceEventShutdown:

		if (m_Instance.device != VK_NULL_HANDLE)
		{
			GarbageCollect(true);
			if (rayQueryPipelineMap.size() > 0)
			{
				for (auto itor = rayQueryPipelineMap.begin(); itor != rayQueryPipelineMap.end(); ++itor)
				{
					VkPipeline pipeline = itor->second;
					vkDestroyPipeline(m_Instance.device, pipeline, NULL);
				}
				rayQueryPipelineMap.clear();
			}

			if (rayQueryPipelieLayout != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(m_Instance.device, rayQueryPipelieLayout, NULL);
				rayQueryPipelieLayout = VK_NULL_HANDLE;
			}
		}

		m_UnityVulkan = NULL;
		m_Instance = UnityVulkanInstance();
		break;
	}

	return true;
}

void RenderAPI_VulkanRayQuery::Prepare()
{
	NativeLogger::LogInfo("Native Do Prepare");

	if (alreadyProcessEvent) {
		NativeLogger::LogInfo("already processEvent");
	}

	if (alreadyPrepared_)
	{
		NativeLogger::LogInfo("already prepared");
		return;
	}

	NativeLogger::LogInfo(CreateDeviceSuccess ? "create Device success" : "create Device failed");
	NativeLogger::LogInfo(OnPluginLoad ? "pluginload success" : "pluginload failed");

	if (nullptr == device_)
	{
		NativeLogger::LogInfo("missing device");
		return;
	}

	CreateDescriptorSetsLayouts();
	CreateDescriptorPool();

	NativeLogger::LogInfo("after createDescriptor");

	globalUniformData_.Create(
		"GlobalUniform",
		device_,
		physicalDeviceMemoryProperties_,
		sizeof(GlobalUniform),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VulkanRT::Buffer::kDefaultMemoryPropertyFlags
	);
	
	globalUniformBufferInfo.buffer = globalUniformData_.GetBuffer();
	globalUniformBufferInfo.offset = 0;
	globalUniformBufferInfo.range = globalUniformData_.GetSize();


	NativeLogger::LogInfo("after data create");

	alreadyPrepared_ = true;

	NativeLogger::LogInfo(alreadyPrepared_ ? "PrepareDone" : "PrepareFailed");
}


void RenderAPI_VulkanRayQuery::SetShaderData(int type, const unsigned char* data, int dataSize)
{
	if (type == 0) {
		rayShadowVertData.assign(data, data + dataSize);
		rayShadowVertDataSize = dataSize;
	}
	else if (type == 1)
	{
		rayShadowFragData.assign(data, data + dataSize);
		rayShadowFragDataSize = dataSize;
	}
}

void RenderAPI_VulkanRayQuery::TraceRays(int cameraInstanceId)
{
	UnityVulkanRecordingState recordingState;
	if (!graphicsInterface_->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
	{
		return;
	}


	ManualBuildTlas();

	if (tlas_.accelerationStructure == VK_NULL_HANDLE)
	{
		GarbageCollect(recordingState.safeFrameNumber);
		return;
	}


	if (rayQueryPipelieLayout == VK_NULL_HANDLE)
	{
		CreatePipelineLayout();
		BuildDescriptorBufferInfos(cameraInstanceId, recordingState.currentFrameNumber);
		UpdateDescriptorSets(cameraInstanceId, recordingState.currentFrameNumber);
		if (rayQueryPipelieLayout == VK_NULL_HANDLE)
		{
			GarbageCollect(recordingState.safeFrameNumber);
			return;
		}
	}

	{
		CreatePipeline(recordingState.renderPass);
	}

	if (0 != rayQueryPipelineMap.size() && rayQueryPipelieLayout != VK_NULL_HANDLE)
	{
		BuildAndSubmitRayTracingCommandBuffer(cameraInstanceId, recordingState.commandBuffer, recordingState.currentFrameNumber);
	}

	GarbageCollect(recordingState.safeFrameNumber);
}


AddResourceResult RenderAPI_VulkanRayQuery::AddLight(int lightInstanceId, float x, float y, float z, float dx, float dy, float dz, float r, float g, float b, float bounceIntensity, float intensity, float range, float spotAngle, int type, bool enabled)
{
	return AddResourceResult::Success;
}

void RenderAPI_VulkanRayQuery::UpdateLight(int lightInstanceId, float x, float y, float z, float dx, float dy, float dz, float r, float g, float b, float bounceIntensity, float intensity, float range, float spotAngle, int type, bool enabled)
{
	auto globalUniform = reinterpret_cast<GlobalUniform*>(globalUniformData_.Map());
	globalUniform->light_position = vec3(x, y, z);
	globalUniform->light_direction = vec3(dx, dy, dz);
	globalUniformData_.Unmap();
}

void RenderAPI_VulkanRayQuery::RemoveLight(int lightInstanceId)
{
	
}


void RenderAPI_VulkanRayQuery::UpdateCameraMat(float x, float y, float z, float* world2cameraProj)
{
	auto globaluniform = reinterpret_cast<GlobalUniform*>(globalUniformData_.Map());
	
	FloatArrayToMatrixNoTranspose(world2cameraProj, globaluniform->view_proj);

	globaluniform->camera_position.x = x;
	globaluniform->camera_position.y = y;
	globaluniform->camera_position.z = z;

	globalUniformData_.Unmap();
}


void RenderAPI_VulkanRayQuery::UpdateModelMat(int sharedMeshInstanceId, float* l2w)
{
	auto it = sharedMeshesL2WMatrices_.find(sharedMeshInstanceId);
	if (it != sharedMeshesL2WMatrices_.end()) {
		sharedMeshesL2WMatrices_[sharedMeshInstanceId] = l2w;
	}
	else {
		sharedMeshesL2WMatrices_.insert(std::make_pair(sharedMeshInstanceId, l2w));
	}
}

void RenderAPI_VulkanRayQuery::ManualBuildTlas()
{
	UnityVulkanRecordingState recordingState;
	if (!graphicsInterface_->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
	{
		return;
	}

	if (nullptr == commandPool_)
	{
		VkCommandPoolCreateInfo command_pool_info = {};
		command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_info.queueFamilyIndex = computeQuueueFamilyIndex;
		command_pool_info.flags = 0;

		VkResult poolResult = vkCreateCommandPool(device_, &command_pool_info, nullptr, &commandPool_);

		if (poolResult != VK_SUCCESS)
		{
			return;
		}
	}

	VkCommandBufferAllocateInfo cmd_buf_allocate_info{};
	cmd_buf_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmd_buf_allocate_info.commandPool = commandPool_;
	cmd_buf_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	cmd_buf_allocate_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer;

	VkResult result = vkAllocateCommandBuffers(device_, &cmd_buf_allocate_info, &command_buffer);

	if (result == VK_SUCCESS) {

		VkCommandBufferInheritanceInfo command_buffer_inheritance_info{};
		command_buffer_inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

		// If requested, also start recording for the new command buffer
		VkCommandBufferBeginInfo command_buffer_info{};
		command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_buffer_info.pInheritanceInfo = &command_buffer_inheritance_info;

		vkBeginCommandBuffer(command_buffer, &command_buffer_info);

		BuildTlas(command_buffer, recordingState.currentFrameNumber);
		
		//submit commandbuffer
		vkEndCommandBuffer(command_buffer);

		
		
		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = 0;

		VkFence fence;
		VkResult fenceResult = vkCreateFence(device_, &fence_info, nullptr, &fence);
		if (fenceResult != VK_SUCCESS)
		{
			return;
		}

		//std::lock_guard<std::mutex> lock(computeQueueMutex);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;

		// Submit to the queue
		VkResult submitRet = vkQueueSubmit(computeQueue_, 1, &submit_info, fence);

		if (submitRet != VK_SUCCESS)
		{
			return;
		}

		submitRet = vkWaitForFences(device_, 1, &fence, VK_TRUE, 100000000000);

		if (submitRet != VK_SUCCESS)
		{
			return;
		}

		vkDestroyFence(device_, fence, nullptr);

		vkFreeCommandBuffers(device_, commandPool_, 1, &command_buffer);
	}
}

void RenderAPI_VulkanRayQuery::InitializeFromUnityInstance(IUnityGraphicsVulkan* graphicsInterface)
{
	graphicsInterface_ = graphicsInterface;
	device_ = graphicsInterface_->Instance().device;


	m_UnityVulkan = graphicsInterface;
	m_Instance = m_UnityVulkan->Instance();
}

AddResourceResult RenderAPI_VulkanRayQuery::AddSharedMesh(int sharedMeshInstanceId, float* verticesArray, float* normalsArray, float* tangentsArray, float* uvsArray, int vertexCount, int* indicesArray, int indexCount)
{
	// Check that this shared mesh hasn't been added yet
	if (sharedMeshesPool_.find(sharedMeshInstanceId) != sharedMeshesPool_.in_use_end())
	{
		return AddResourceResult::AlreadyExists;
	}

	// We can only add tris, make sure the index count reflects this
	assert(indexCount % 3 == 0);

	auto sentMesh = make_unique<VulkanRT::VulkanRTData::RayTracerMeshSharedData>();

	// Setup where we are going to store the shared mesh data and all data needed for shaders
	sentMesh->sharedMeshInstanceId = sharedMeshInstanceId;
	sentMesh->vertexCount = vertexCount;
	sentMesh->indexCount = indexCount;

	static constexpr VkBufferUsageFlags buffer_usage_flags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	// Setup buffers
	bool success = true;
	if (sentMesh->vertexBuffer.Create(
		"vertexBuffer",
		device_,
		physicalDeviceMemoryProperties_,
		sizeof(RayQueryVertex) * sentMesh->vertexCount,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | buffer_usage_flags,
		VulkanRT::Buffer::kDefaultMemoryPropertyFlags)
		!= VK_SUCCESS)
	{
		success = false;
	}

	if (sentMesh->indexBuffer.Create(
		"indexBuffer",
		device_,
		physicalDeviceMemoryProperties_,
		sizeof(uint32_t) * sentMesh->indexCount,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | buffer_usage_flags,
		VulkanRT::Buffer::kDefaultMemoryPropertyFlags)
		!= VK_SUCCESS)
	{
		success = false;
	}


	if (!success)
	{
		return AddResourceResult::Error;
	}

	// Creating buffers was successful.  Move onto getting the data in there
	auto vertices = reinterpret_cast<RayQueryVertex*>(sentMesh->vertexBuffer.Map());
	auto indices = reinterpret_cast<uint32_t*>(sentMesh->indexBuffer.Map());

	for (int i = 0; i < vertexCount; ++i)
	{
		// Build for acceleration structure
		vertices[i].position.x = verticesArray[3 * i + 0];
		vertices[i].position.y = verticesArray[3 * i + 1];
		vertices[i].position.z = verticesArray[3 * i + 2];

		vertices[i].normal.x = normalsArray[3 * i + 0];
		vertices[i].normal.y = normalsArray[3 * i + 1];
		vertices[i].normal.z = normalsArray[3 * i + 2];
	}


	for (int i = 0; i < indexCount / 3; ++i)
	{
		// Build for acceleration structure
		indices[3 * i + 0] = static_cast<uint32_t>(indicesArray[3 * i + 0]);
		indices[3 * i + 1] = static_cast<uint32_t>(indicesArray[3 * i + 1]);
		indices[3 * i + 2] = static_cast<uint32_t>(indicesArray[3 * i + 2]);
	}

	sentMesh->vertexBuffer.Unmap();
	sentMesh->indexBuffer.Unmap();

	// All done creating the data, get it added to the pool
	sharedMeshesPool_.add(sharedMeshInstanceId, std::move(sentMesh));

	// Build blas here so we don't have to do it later
	BuildBlas(sharedMeshInstanceId);


	return AddResourceResult::Success;
}

AddResourceResult RenderAPI_VulkanRayQuery::AddTlasInstance(int gameObjectInstanceId, int sharedMeshInstanceId, float* l2wMatrix, float* w2lMatrix)
{
	if (meshInstancePool_.find(gameObjectInstanceId) != meshInstancePool_.in_use_end())
	{
		return AddResourceResult::AlreadyExists;
	}

	meshInstancePool_.add(gameObjectInstanceId, VulkanRT::VulkanRTData::RayTracerMeshInstanceData());
	auto& instance = meshInstancePool_[gameObjectInstanceId];

	instance.gameObjectInstanceId = gameObjectInstanceId;
	instance.sharedMeshInstanceId = sharedMeshInstanceId;

	FloatArrayToMatrix(l2wMatrix, instance.localToWorld);
	FloatArrayToMatrix(w2lMatrix, instance.worldToLocal);

	instance.instanceData.Create(
		"instanceData",
		device_,
		physicalDeviceMemoryProperties_,
		sizeof(RayQueryTLASInstanceData),
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,

		VulkanRT::Buffer::kDefaultMemoryPropertyFlags
	);

	rebuildTlas_ = true;

	NativeLogger::LogInfo("Create TLAS Done");


	return AddResourceResult::Success;
}


void RenderAPI_VulkanRayQuery::UpdateTlasInstance(int gameObjectInstanceId, float* l2wMatrix, float* w2lMatrix)
{
	FloatArrayToMatrix(l2wMatrix, meshInstancePool_[gameObjectInstanceId].localToWorld);
	FloatArrayToMatrix(w2lMatrix, meshInstancePool_[gameObjectInstanceId].worldToLocal);
	updateTlas_ = true;

	//NativeLogger::LogInfo("Update TLAS Done");
}

void RenderAPI_VulkanRayQuery::RemoveTlasInstance(int gameObjectInstanceId)
{
	meshInstancePool_.remove(gameObjectInstanceId);
	rebuildTlas_ = true;
}

void RenderAPI_VulkanRayQuery::BuildBlas(int sharedMeshInstanceId)
{
	// Create buffers for the bottom level geometry

		// Setup a single transformation matrix that can be used to transform the whole geometry for a single bottom level acceleration structure
	VkTransformMatrixKHR transformMatrix = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f };

	auto transformBuffer = make_unique<VulkanRT::Buffer>();
	transformBuffer->Create(
		"transformBuffer",
		device_,
		physicalDeviceMemoryProperties_,
		sizeof(VkTransformMatrixKHR),
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VulkanRT::Buffer::kDefaultMemoryPropertyFlags);
	transformBuffer->UploadData(&transformMatrix, sizeof(VkTransformMatrixKHR));


	// The bottom level acceleration structure contains one set of triangles as the input geometry
	VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	accelerationStructureGeometry.geometry.triangles.pNext = nullptr;
	accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	accelerationStructureGeometry.geometry.triangles.vertexData = sharedMeshesPool_[sharedMeshInstanceId]->vertexBuffer.GetBufferDeviceAddressConst();
	accelerationStructureGeometry.geometry.triangles.maxVertex = sharedMeshesPool_[sharedMeshInstanceId]->vertexCount;
	accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(RayQueryVertex);
	accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	accelerationStructureGeometry.geometry.triangles.indexData = sharedMeshesPool_[sharedMeshInstanceId]->indexBuffer.GetBufferDeviceAddressConst();
	accelerationStructureGeometry.geometry.triangles.transformData = transformBuffer->GetBufferDeviceAddressConst();

	// Get the size requirements for buffers involved in the acceleration structure build process
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	// Number of triangles 
	const uint32_t primitiveCount = sharedMeshesPool_[sharedMeshInstanceId]->indexCount / 3;

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(
		device_,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&primitiveCount,
		&accelerationStructureBuildSizesInfo);

	// Create a buffer to hold the acceleration structure
	sharedMeshesPool_[sharedMeshInstanceId]->blas.buffer.Create(
		"blas",
		device_,
		physicalDeviceMemoryProperties_,
		accelerationStructureBuildSizesInfo.accelerationStructureSize,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
		VulkanRT::Buffer::kDefaultMemoryPropertyFlags);

	// Create the acceleration structure
	VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {};
	accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationStructureCreateInfo.buffer = sharedMeshesPool_[sharedMeshInstanceId]->blas.buffer.GetBuffer();
	accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	vkCreateAccelerationStructureKHR(device_, &accelerationStructureCreateInfo, nullptr, &sharedMeshesPool_[sharedMeshInstanceId]->blas.accelerationStructure);

	// The actual build process starts here
	// Create a scratch buffer as a temporary storage for the acceleration structure build
	auto scratchBuffer = make_unique<VulkanRT::Buffer>();
	scratchBuffer->Create(
		"scratch",
		device_,
		physicalDeviceMemoryProperties_,
		accelerationStructureBuildSizesInfo.buildScratchSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = { };
	accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationBuildGeometryInfo.dstAccelerationStructure = sharedMeshesPool_[sharedMeshInstanceId]->blas.accelerationStructure;
	accelerationBuildGeometryInfo.geometryCount = 1;
	accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
	accelerationBuildGeometryInfo.scratchData = scratchBuffer->GetBufferDeviceAddress();

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = { };
	accelerationStructureBuildRangeInfo.primitiveCount = primitiveCount;
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationStructureBuildRangeInfos = { &accelerationStructureBuildRangeInfo };

	// Get the bottom acceleration structure's handle, which will be used during the top level acceleration build
	VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo{};
	accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationStructureDeviceAddressInfo.accelerationStructure = sharedMeshesPool_[sharedMeshInstanceId]->blas.accelerationStructure;
	sharedMeshesPool_[sharedMeshInstanceId]->blas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device_, &accelerationStructureDeviceAddressInfo);

	graphicsInterface_->EnsureOutsideRenderPass();

	UnityVulkanRecordingState recordingState;
	if (!graphicsInterface_->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
	{
		return;
	}

	vkCmdBuildAccelerationStructuresKHR(
		recordingState.commandBuffer,
		1,
		&accelerationBuildGeometryInfo,
		accelerationStructureBuildRangeInfos.data());

	auto index = garbageBuffers_.size();
	garbageBuffers_.push_back(VulkanRT::VulkanRTData::RayTracerGarbageBuffer());
	garbageBuffers_[index].frameCount = recordingState.currentFrameNumber;
	garbageBuffers_[index].buffer = std::move(scratchBuffer);

	index = garbageBuffers_.size();
	garbageBuffers_.push_back(VulkanRT::VulkanRTData::RayTracerGarbageBuffer());
	garbageBuffers_[index].frameCount = recordingState.currentFrameNumber;
	garbageBuffers_[index].buffer = std::move(transformBuffer);
}

void RenderAPI_VulkanRayQuery::BuildTlas(VkCommandBuffer commandBuffer, uint64_t currentFrameNumber)
{
	if (rebuildTlas_ == false && updateTlas_ == false)
	{
		return;
	}

	bool update = updateTlas_;
	if (rebuildTlas_) {
		update = false;
	}

	if (meshInstancePool_.in_use_size() == 0)
	{
		return;
	}

	if (!update)
	{
		std::vector<VkAccelerationStructureInstanceKHR> instanceAccelerationStructures;
		instanceAccelerationStructures.resize(meshInstancePool_.in_use_size(), VkAccelerationStructureInstanceKHR{});

		uint32_t instanceAccelerationStructuresIndex = 0;
		for (auto i = meshInstancePool_.in_use_begin(); i != meshInstancePool_.in_use_end(); ++i)
		{
			auto gameObjectInstanceId = (*i).first;

			auto& instance = meshInstancePool_[gameObjectInstanceId];

			const auto& t = instance.localToWorld;
			VkTransformMatrixKHR transformMatrix = {
				t[0][0], t[0][1], t[0][2], t[0][3],
				t[1][0], t[1][1], t[1][2], t[1][3],
				t[2][0], t[2][1], t[2][2], t[2][3]
			};

			VkAccelerationStructureInstanceKHR& accelerationStructureInstance = instanceAccelerationStructures[instanceAccelerationStructuresIndex];
			accelerationStructureInstance.transform = transformMatrix;
			accelerationStructureInstance.instanceCustomIndex = instanceAccelerationStructuresIndex;
			accelerationStructureInstance.mask = 0xFF;
			accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
			accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			accelerationStructureInstance.accelerationStructureReference = sharedMeshesPool_[instance.sharedMeshInstanceId]->blas.deviceAddress;

			auto instanceData = reinterpret_cast<RayQueryTLASInstanceData*>(instance.instanceData.Map());


			instanceData->localToWorld = instance.localToWorld;
			instanceData->worldToLocal = instance.worldToLocal;

			instance.instanceData.Unmap();

			++instanceAccelerationStructuresIndex;
		}

		instancesAccelerationStructuresBuffer_.Destroy();

		instancesAccelerationStructuresBuffer_.Create(
			"instanceBuffer",
			device_,
			physicalDeviceMemoryProperties_,
			instanceAccelerationStructures.size() * sizeof(VkAccelerationStructureInstanceKHR),
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VulkanRT::Buffer::kDefaultMemoryPropertyFlags);

		instancesAccelerationStructuresBuffer_.UploadData(instanceAccelerationStructures.data(), instancesAccelerationStructuresBuffer_.GetSize());
	}
	else
	{
		auto instances = reinterpret_cast<VkAccelerationStructureInstanceKHR*>(instancesAccelerationStructuresBuffer_.Map());

		uint32_t instanceAcclerationStructionIndex = 0;
		for (auto i = meshInstancePool_.in_use_begin(); i != meshInstancePool_.in_use_end(); ++i)
		{
			auto gameObjectInstanceId = (*i).first;

			auto& instance = meshInstancePool_[gameObjectInstanceId];

			const auto& t = instance.localToWorld;
			VkTransformMatrixKHR transformMatrix = {
				t[0][0], t[0][1], t[0][2], t[0][3],
				t[1][0], t[1][1], t[1][2], t[1][3],
				t[2][0], t[2][1], t[2][2], t[2][3],
			};

			instances[instanceAcclerationStructionIndex].transform = transformMatrix;

			auto instanceData = reinterpret_cast<RayQueryTLASInstanceData*>(instance.instanceData.Map());


			instanceData->localToWorld = instance.localToWorld;
			instanceData->worldToLocal = instance.worldToLocal;

			instance.instanceData.Unmap();

			++instanceAcclerationStructionIndex;
		}
		instancesAccelerationStructuresBuffer_.Unmap();
	}

	VkAccelerationStructureGeometryInstancesDataKHR accelerationStructureGeometryInstancesData = {};
	accelerationStructureGeometryInstancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	accelerationStructureGeometryInstancesData.arrayOfPointers = VK_FALSE;
	accelerationStructureGeometryInstancesData.data.deviceAddress = instancesAccelerationStructuresBuffer_.GetBufferDeviceAddressConst().deviceAddress;

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelerationStructureGeometry.geometry.instances = accelerationStructureGeometryInstancesData;

	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	uint32_t instancesCount = static_cast<uint32_t>(meshInstancePool_.in_use_size());

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(
		device_,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&instancesCount,
		&accelerationStructureBuildSizesInfo
	);

	if (!update)
	{
		if (tlas_.accelerationStructure != VK_NULL_HANDLE)
		{
			vkDestroyAccelerationStructureKHR(device_, tlas_.accelerationStructure, nullptr);
		}
		tlas_.buffer.Destroy();

		tlas_.buffer.Create(
			"tlas",
			device_,
			physicalDeviceMemoryProperties_,
			accelerationStructureBuildSizesInfo.accelerationStructureSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
			VulkanRT::Buffer::kDefaultMemoryPropertyFlags
		);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = tlas_.buffer.GetBuffer();
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device_, &accelerationStructureCreateInfo, nullptr, &tlas_.accelerationStructure);
	}

	//实际创建tlas
	auto scratchBuffer = make_unique<VulkanRT::Buffer>();
	scratchBuffer->Create(
		"scratch",
		device_,
		physicalDeviceMemoryProperties_,
		accelerationStructureBuildSizesInfo.buildScratchSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = {};
	accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
	accelerationBuildGeometryInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationBuildGeometryInfo.srcAccelerationStructure = update ? tlas_.accelerationStructure : VK_NULL_HANDLE;
	accelerationBuildGeometryInfo.dstAccelerationStructure = tlas_.accelerationStructure;
	accelerationBuildGeometryInfo.geometryCount = 1;
	accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
	accelerationBuildGeometryInfo.scratchData = scratchBuffer->GetBufferDeviceAddress();

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo;
	accelerationStructureBuildRangeInfo.primitiveCount = static_cast<uint32_t>(meshInstancePool_.in_use_size());
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;

	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationStructureBuildRangeInfos = { &accelerationStructureBuildRangeInfo };

	VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo = {};
	accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationStructureDeviceAddressInfo.accelerationStructure = tlas_.accelerationStructure;
	tlas_.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device_, &accelerationStructureDeviceAddressInfo);

	vkCmdBuildAccelerationStructuresKHR(
		commandBuffer,
		1,
		&accelerationBuildGeometryInfo,
		accelerationStructureBuildRangeInfos.data()
	);

	auto index = garbageBuffers_.size();
	garbageBuffers_.push_back(VulkanRT::VulkanRTData::RayTracerGarbageBuffer());
	garbageBuffers_[index].frameCount = currentFrameNumber;
	garbageBuffers_[index].buffer = std::move(scratchBuffer);

	rebuildTlas_ = false;
	updateTlas_ = false;
}


void RenderAPI_VulkanRayQuery::CreateDescriptorSetsLayouts()
{
	{
		//加速结构
		VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding{};
		accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		accelerationStructureLayoutBinding.binding = 0;
		accelerationStructureLayoutBinding.descriptorCount = 1;

		//shader uniform
		VkDescriptorSetLayoutBinding globalUniformLayoutBinding{};
		globalUniformLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		globalUniformLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		globalUniformLayoutBinding.binding = 1;
		globalUniformLayoutBinding.descriptorCount = 1;

		std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings =
		{
			accelerationStructureLayoutBinding,
			globalUniformLayoutBinding
		};

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(set_layout_bindings.size());
		descriptorSetLayoutCreateInfo.pBindings = set_layout_bindings.data();

		VkResult result = vkCreateDescriptorSetLayout(device_, &descriptorSetLayoutCreateInfo, nullptr, &rayQueryDescrioptorSetLayout);
		if (result != VK_SUCCESS)
		{
			NativeLogger::LogInfo("CreateDescSetLayout Failed");
			NativeLogger::LogInfo(vkResultToString(result));
		}

	}
}

void RenderAPI_VulkanRayQuery::CreatePipelineLayout()
{
	VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	VkPushConstantRange push_constant;
	push_constant.offset = 0;
	push_constant.size = sizeof(PerMeshUniform);
	push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	pipeline_layout_create_info.pPushConstantRanges = &push_constant;
	pipeline_layout_create_info.pushConstantRangeCount = 1;


	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts = &rayQueryDescrioptorSetLayout;

	VkResult ret = vkCreatePipelineLayout(device_, &pipeline_layout_create_info, nullptr, &rayQueryPipelieLayout);

	if (ret != VK_SUCCESS) {
		NativeLogger::LogError("Create PipelineLayout Failed");
	}
}

void RenderAPI_VulkanRayQuery::CreatePipeline(VkRenderPass renderPass)
{
	for (auto itor = sharedMeshesPool_.in_use_begin(); itor != sharedMeshesPool_.in_use_end(); ++itor)
	{
		int idx = itor->first;

		auto pipelineItor = rayQueryPipelineMap.find(idx);
		if (pipelineItor == rayQueryPipelineMap.end()) {

			VulkanRT::Shader rayShadowVert(device_);
			VulkanRT::Shader rayShadowFrag(device_);

			//create rtquery shader
			if (!rayShadowVertData.empty()) {
				rayShadowVert.LoadFromShaderByte(rayShadowVertData, rayShadowVertDataSize);
			}
			else {
				return;
			}

			if (!rayShadowFragData.empty())
			{
				rayShadowFrag.LoadFromShaderByte(rayShadowFragData, rayShadowFragDataSize);
			}
			else {
				return;
			}

			const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
				rayShadowVert.GetShaderStage(VK_SHADER_STAGE_VERTEX_BIT),
				rayShadowFrag.GetShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT)
			};

			VkPipelineInputAssemblyStateCreateInfo CreateInfo = {};
			CreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			CreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			CreateInfo.primitiveRestartEnable = VK_FALSE;
			CreateInfo.flags = 0;

			VkPipelineRasterizationStateCreateInfo RasterizationCreateInfo = {};
			RasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			RasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
			RasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
			RasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
			RasterizationCreateInfo.depthClampEnable = VK_FALSE;
			RasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
			RasterizationCreateInfo.depthBiasEnable = VK_FALSE;
			RasterizationCreateInfo.lineWidth = 1.0f;
			RasterizationCreateInfo.flags = 0;

			VkPipelineColorBlendAttachmentState ColorBlendAttachmentCreateInfo[1] = {};
			ColorBlendAttachmentCreateInfo[0].colorWriteMask = 0xf;
			ColorBlendAttachmentCreateInfo[0].blendEnable = VK_FALSE;

			VkPipelineColorBlendStateCreateInfo ColorBlendCreateInfo = {};
			ColorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			ColorBlendCreateInfo.attachmentCount = 1;
			ColorBlendCreateInfo.pAttachments = ColorBlendAttachmentCreateInfo;


			VkPipelineViewportStateCreateInfo ViewportCreateInfo = {};
			ViewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			ViewportCreateInfo.viewportCount = 1;
			ViewportCreateInfo.scissorCount = 1;
			ViewportCreateInfo.flags = 0;

			const VkDynamicState dynamicStateEnables[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			VkPipelineDynamicStateCreateInfo DynamicCreateInfo = {};
			DynamicCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			DynamicCreateInfo.pDynamicStates = dynamicStateEnables;
			DynamicCreateInfo.dynamicStateCount = sizeof(dynamicStateEnables) / sizeof(*dynamicStateEnables);
			DynamicCreateInfo.flags = 0;

			VkPipelineMultisampleStateCreateInfo MultiSampleCreateInfo = {};
			MultiSampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			MultiSampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			MultiSampleCreateInfo.flags = 0;

			VkPipelineDepthStencilStateCreateInfo DepthStencilCreateInfo = {};
			DepthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			DepthStencilCreateInfo.depthTestEnable = VK_TRUE;
			DepthStencilCreateInfo.depthWriteEnable = VK_TRUE;
			DepthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
			DepthStencilCreateInfo.stencilTestEnable = VK_FALSE;
			DepthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // Unity/Vulkan uses reverse Z
			DepthStencilCreateInfo.front = DepthStencilCreateInfo.back;
			DepthStencilCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
			DepthStencilCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
			DepthStencilCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;

			// Vertex:  ray_shadow.vert
			// float3 vpos;
			// float3 normal;
			VkVertexInputBindingDescription VertexInputDesc = {};
			VertexInputDesc.binding = 0;
			VertexInputDesc.stride = sizeof(RayQueryVertex);
			VertexInputDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			VkVertexInputAttributeDescription VertexInputAttrDesc[2];
			VertexInputAttrDesc[0].binding = 0;
			VertexInputAttrDesc[0].location = 0;
			VertexInputAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			VertexInputAttrDesc[0].offset = 0;

			VertexInputAttrDesc[1].binding = 0;
			VertexInputAttrDesc[1].location = 1;
			VertexInputAttrDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			VertexInputAttrDesc[1].offset = sizeof(RayQueryVertex::position);

			VkPipelineVertexInputStateCreateInfo VertexInputCreateInfo = {};
			VertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			VertexInputCreateInfo.vertexBindingDescriptionCount = 1;
			VertexInputCreateInfo.pVertexBindingDescriptions = &VertexInputDesc;
			VertexInputCreateInfo.vertexAttributeDescriptionCount = 2;
			VertexInputCreateInfo.pVertexAttributeDescriptions = VertexInputAttrDesc;

			VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.layout = rayQueryPipelieLayout;
			pipelineCreateInfo.renderPass = renderPass;

			pipelineCreateInfo.stageCount = shader_stages.size();
			pipelineCreateInfo.pStages = shader_stages.data();
			pipelineCreateInfo.pVertexInputState = &VertexInputCreateInfo;
			pipelineCreateInfo.pInputAssemblyState = &CreateInfo;
			pipelineCreateInfo.pRasterizationState = &RasterizationCreateInfo;
			pipelineCreateInfo.pColorBlendState = &ColorBlendCreateInfo;
			pipelineCreateInfo.pMultisampleState = &MultiSampleCreateInfo;
			pipelineCreateInfo.pViewportState = &ViewportCreateInfo;
			pipelineCreateInfo.pDepthStencilState = &DepthStencilCreateInfo;
			pipelineCreateInfo.pDynamicState = &DynamicCreateInfo;

			VkPipeline pipeline;

			VkResult result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
			if (result != VK_SUCCESS)
			{

			}

			rayQueryPipelineMap.insert(std::make_pair(idx, std::move(pipeline)));
		}
	}
}

void RenderAPI_VulkanRayQuery::BuildAndSubmitRayTracingCommandBuffer(int cameraInstanceId, VkCommandBuffer commandBuffer, uint64_t currentFrameNumber)
{
	//绑定管线
	for (auto itor = meshInstancePool_.in_use_begin(); itor != meshInstancePool_.in_use_end(); ++itor)
	{
		int gameObjectInstanceId = itor->first;
		int idx = meshInstancePool_[gameObjectInstanceId].sharedMeshInstanceId;
		auto& rayTracerMeshData = sharedMeshesPool_[idx];
		auto& renderPipeline = rayQueryPipelineMap[idx];
		float* modelMat = sharedMeshesL2WMatrices_[idx];

		if(nullptr == modelMat) continue;

		
		
		VkBuffer vertexBuffer = rayTracerMeshData->vertexBuffer.GetBuffer();
		VkBuffer indexBuffer = rayTracerMeshData->indexBuffer.GetBuffer();


		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayQueryPipelieLayout, 0, 1, &rayQueryDescSet, 0, 0);
		vkCmdPushConstants(commandBuffer, rayQueryPipelieLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PerMeshUniform), modelMat);

		VkDeviceSize offsets[1] = {0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		

		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(rayTracerMeshData->indexCount), 1, 0, 0, 0);
	}
}

void RenderAPI_VulkanRayQuery::BuildDescriptorBufferInfos(int cameraInstanceId, uint64_t currentFrameNumber)
{
	uint32_t i;
	i = 1;

	int meshInUseSize = meshInstancePool_.in_use_size();
	meshInstancesDataBufferInfos_.resize(meshInUseSize);

	auto meshBeginItor = meshInstancePool_.in_use_begin();
	auto meshEndItor = meshInstancePool_.in_use_end();
	i = 0;
	for (auto itor = meshBeginItor; itor != meshEndItor; ++itor)
	{
		auto gameObjectInstanceId = (*itor).first;
		auto sharedMeshInstanced = meshInstancePool_[gameObjectInstanceId].sharedMeshInstanceId;

		{
			const VulkanRT::Buffer& buffer = meshInstancePool_[gameObjectInstanceId].instanceData;
			meshInstancesDataBufferInfos_[i].buffer = buffer.GetBuffer();
			meshInstancesDataBufferInfos_[i].offset = 0;
			meshInstancesDataBufferInfos_[i].range = buffer.GetSize();
		}
		++i;
	}
}

void RenderAPI_VulkanRayQuery::CreateDescriptorPool()
{
	//  data 0  ->  Acceleration structure
	//  data 1  ->  Global Uniform Data

	std::vector<VkDescriptorPoolSize> pool_sizes = {
		{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 5},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
	};

	VkDescriptorPoolCreateInfo descriptor_pool_info{};
	descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptor_pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	descriptor_pool_info.pPoolSizes = pool_sizes.data();
	descriptor_pool_info.maxSets = pool_sizes.size() * 10;

	VkResult result = vkCreateDescriptorPool(device_, &descriptor_pool_info, nullptr, &descriptorPool_);
	if (result != VK_SUCCESS)
	{
		NativeLogger::LogInfo("Create Pool Failed");
	}
}

void RenderAPI_VulkanRayQuery::UpdateDescriptorSets(int cameraInstanceId, uint64_t currentFrameNumber)
{
	/*VkDescriptorSetAllocateInfo descriptor_set_allocate_info = vkb::initializers::descriptor_set_allocate_info(descriptor_pool, &descriptor_set_layout, 1);*/
	

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info{};
	descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptor_set_allocate_info.descriptorPool = descriptorPool_;
	descriptor_set_allocate_info.descriptorSetCount = 1;
	descriptor_set_allocate_info.pSetLayouts = &rayQueryDescrioptorSetLayout;

	VkResult result = vkAllocateDescriptorSets(device_, &descriptor_set_allocate_info, &rayQueryDescSet);

	//rayQueryDescrioptorSetVec.resize(1);


	std::vector<VkWriteDescriptorSet> descriptorWrites;
	{
		// Set up the descriptor for binding our top level acceleration structure to the ray tracing shaders
		VkWriteDescriptorSetAccelerationStructureKHR descriptor_acceleration_structure_info{};
		descriptor_acceleration_structure_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		descriptor_acceleration_structure_info.accelerationStructureCount = 1;
		descriptor_acceleration_structure_info.pAccelerationStructures = &tlas_.accelerationStructure;

		VkWriteDescriptorSet accelerationStructureWrite;
		accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		accelerationStructureWrite.pNext = &descriptor_acceleration_structure_info; // Notice that pNext is assigned here!
		accelerationStructureWrite.dstSet = rayQueryDescSet;
		accelerationStructureWrite.dstBinding = 0;
		accelerationStructureWrite.dstArrayElement = 0;
		accelerationStructureWrite.descriptorCount = 1;
		accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		accelerationStructureWrite.pImageInfo = nullptr;
		accelerationStructureWrite.pBufferInfo = nullptr;
		accelerationStructureWrite.pTexelBufferView = nullptr;

		descriptorWrites.push_back(accelerationStructureWrite);

		//global uniform
		VkWriteDescriptorSet globalUniformBufferWrite;
		globalUniformBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		globalUniformBufferWrite.pNext = nullptr;
		globalUniformBufferWrite.dstSet = rayQueryDescSet;
		globalUniformBufferWrite.dstBinding = 1;
		globalUniformBufferWrite.dstArrayElement = 0;
		globalUniformBufferWrite.descriptorCount = 1;
		globalUniformBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		globalUniformBufferWrite.pImageInfo = nullptr;
		globalUniformBufferWrite.pBufferInfo = &globalUniformBufferInfo;
		globalUniformBufferWrite.pTexelBufferView = nullptr;

		descriptorWrites.push_back(globalUniformBufferWrite);
	}
	
	vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, VK_NULL_HANDLE);
}


void RenderAPI_VulkanRayQuery::GarbageCollect(uint64_t frameCount)
{
	std::vector<uint32_t> removeIndices;
	for (int32_t i = 0; i < static_cast<int32_t>(garbageBuffers_.size()); ++i)
	{
		if (garbageBuffers_[i].frameCount < frameCount)
		{
			garbageBuffers_[i].buffer->Destroy();
			garbageBuffers_[i].buffer.release();
			removeIndices.push_back(i);
		}
	}

	for (int32_t i = static_cast<int32_t>(removeIndices.size()) - 1; i >= 0; --i)
	{
		garbageBuffers_.erase(garbageBuffers_.begin() + removeIndices[i]);
	}
}