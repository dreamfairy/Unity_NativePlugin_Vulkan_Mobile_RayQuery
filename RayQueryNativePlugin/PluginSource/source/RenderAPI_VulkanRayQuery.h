#pragma once

#include "RenderAPI.h"
#include "PlatformBase.h"

//#define NDEBUG

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

#if SUPPORT_VULKAN

#include <string.h>
#include <map>
#include <vector>
#include <math.h>
#include "Buffer.h"
#include "VulkanRTData.h"
#include "ResourcePool.h"
#include "RayQueryShsaderConst.h"
#include <memory>


/// <summary>
/// Builds VkDeviceCreateInfo that supports ray tracing
/// </summary>
/// <param name="physicalDevice">Physical device used to query properties and queues</param>
/// <param name="outDeviceCreateInfo"></param>
VkResult CreateDevice_RayQuery(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* unityCreateInfo, const VkAllocationCallbacks* unityAllocator, VkDevice* device);

/// <summary>
/// Resolve properties and queues required for ray tracing
/// </summary>
/// <param name="physicalDevice"></param>
void ResolvePropertiesAndQueues_RayQuery(VkPhysicalDevice physicalDevice);

// This plugin does not link to the Vulkan loader, easier to support multiple APIs and systems that don't have Vulkan support
#define VK_NO_PROTOTYPES
#include "Unity/IUnityGraphicsVulkan.h"
#include <mutex>
class RenderAPI_VulkanRayQuery : public RenderAPI
{
public:

	static RenderAPI_VulkanRayQuery& Instance()
	{
		static RenderAPI_VulkanRayQuery instance;
		return instance;
	}

	static bool CreateDeviceSuccess;

	//debug
	static bool OnPluginLoad;

	static bool UseLegacyVulkanLoader;


	/// <summary>
	   /// Builds VkDeviceCreateInfo that supports ray tracing
	   /// </summary>
	   /// <param name="physicalDevice">Physical device used to query properties and queues</param>
	   /// <param name="outDeviceCreateInfo"></param>
	friend VkResult CreateDevice_RayQuery(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* unityCreateInfo, const VkAllocationCallbacks* unityAllocator, VkDevice* device);
	friend VkResult CreateInstance_RayQuery(const VkInstanceCreateInfo* unityCreateInfo, const VkAllocationCallbacks* unityAllocator, VkInstance* instance);
	friend void BeginRenderPass(const VkRenderPassBeginInfo* pRenderPassBegin);
	friend void ResolvePropertiesAndQueues_RayQuery(VkPhysicalDevice physicalDevice);

	//test

	struct VulkanBuffer
	{
		VkBuffer buffer;
		VkDeviceMemory deviceMemory;
		void* mapped;
		VkDeviceSize sizeInBytes;
		VkDeviceSize deviceMemorySize;
		VkMemoryPropertyFlags deviceMemoryFlags;
	};

	RenderAPI_VulkanRayQuery();
	virtual ~RenderAPI_VulkanRayQuery() { }

	virtual bool ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

	virtual void Prepare();
	virtual void SetShaderData(int type, const unsigned char* data, int dataSize);
	virtual void TraceRays(int cameraInstanceId);

	virtual AddResourceResult AddLight(int lightInstanceId, float x, float y, float z, float dx, float dy, float dz, float r, float g, float b, float bounceIntensity, float intensity, float range, float spotAngle, int type, bool enabled);
	virtual void UpdateLight(int lightInstanceId, float x, float y, float z, float dx, float dy, float dz, float r, float g, float b, float bounceIntensity, float intensity, float range, float spotAngle, int type, bool enabled);
	virtual void RemoveLight(int lightInstanceId);

	virtual void UpdateCameraMat(float x, float y, float z, float* world2cameraProj);
	virtual void UpdateModelMat(int sharedMeshInstanceId, float* l2w);

	static VkDevice NullDevice;

private:
	void InitializeFromUnityInstance(IUnityGraphicsVulkan* graphicsInterface);

private:
	IUnityGraphicsVulkan* m_UnityVulkan;
	UnityVulkanInstance m_Instance;


	VkDevice& device_;
	VkCommandPool commandPool_;

	//RT API

	/// <summary>
	/// Graphics interface from Unity3D.  Used to secure command buffers
	/// </summary>
	const IUnityGraphicsVulkan* graphicsInterface_;


	uint32_t graphicsQueueFamilyIndex_;
	uint32_t transferQueueFamilyIndex_;
	uint32_t computeQuueueFamilyIndex;

	VkQueue graphicsQueue_;
	VkQueue transferQueue_;
	VkQueue computeQueue_;
	std::mutex computeQueueMutex;

	VkCommandPool graphicsCommandPool_;
	VkCommandPool transferCommandPool_;

	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties_;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties_;

	bool alreadyPrepared_;
	bool alreadyProcessEvent;

#pragma region SharedMeshMembers

	VulkanRT::resourcePool<int, std::unique_ptr<VulkanRT::VulkanRTData::RayTracerMeshSharedData>> sharedMeshesPool_;
	std::map<int, float*> sharedMeshesL2WMatrices_;

#pragma endregion SharedMeshMembers

#pragma region MeshInstanceMembers

	// meshInstanceID -> Buffer that represents ShaderMeshInstanceData
	VulkanRT::resourcePool<int, VulkanRT::VulkanRTData::RayTracerMeshInstanceData> meshInstancePool_;

	std::vector<VkDescriptorBufferInfo> meshInstancesDataBufferInfos_;

	// Buffer that represents VkAccelerationStructureInstanceKHR
	VulkanRT::Buffer instancesAccelerationStructuresBuffer_;

	VulkanRT::VulkanRTData::RayTracerAccelerationStructure tlas_;
	bool rebuildTlas_;
	bool updateTlas_;

#pragma endregion MeshInstanceMembers

#pragma region ShaderResources

	std::wstring shaderFolder_;

	//RT Query
	VulkanRT::Buffer globalUniformData_;
	float* modelMat;
	VkDescriptorBufferInfo globalUniformBufferInfo;
	std::map<int, VkPipeline> rayQueryPipelineMap;

	VkDescriptorSetLayout rayQueryDescrioptorSetLayout;

	VkDescriptorSet						rayQueryDescSet;
	VkDescriptorPool                   descriptorPool_;


	std::vector<VulkanRT::VulkanRTData::RayTracerGarbageBuffer> garbageBuffers_;

#pragma endregion ShaderResources

#pragma region PipelineResources


	VkPipelineLayout rayQueryPipelieLayout;

#pragma endregion PipelineResources

	//RT API
public:
	AddResourceResult AddSharedMesh(int sharedMeshInstanceId, float* verticesArray, float* normalsArray, float* tangentsArray, float* uvsArray, int vertexCount, int* indicesArray, int indexCount);
	AddResourceResult AddTlasInstance(int gameObjectInstanceId, int sharedMeshInstanceId, float* l2wMatrix, float* w2lMatrix);

	void UpdateTlasInstance(int gameObjectInstanceId, float* l2wMatrix, float* w2lMatrix);

	/// <summary>
	/// Removes instance to be removed on next tlas build
	/// </summary>
	/// <param name="meshInstanceIndex"></param>
	void RemoveTlasInstance(int gameObjectInstanceId);
private:
	std::vector<char> rayShadowVertData;
	std::vector<char> rayShadowFragData;
	int rayShadowVertDataSize;
	int rayShadowFragDataSize;

	/// <summary>
	/// Build a bottom level acceleration structure for an added shared mesh
	/// </summary>
	/// <param name="sharedMeshPoolIndex"></param>
	void BuildBlas(int sharedMeshInstanceId);
	void BuildTlas(VkCommandBuffer commandBuffer, uint64_t currentFrameNumber);
	void ManualBuildTlas();

	/// <summary>
	/// Create descriptor set layouts for shaders
	/// </summary>
	void CreateDescriptorSetsLayouts();

	/// <summary>
	/// Creates ray tracing pipeline layout
	/// </summary>
	void CreatePipelineLayout();

	/// <summary>
	/// Creates ray tracing pipeline
	/// </summary>
	void CreatePipeline(VkRenderPass renderPass);

	/// <summary>
	/// Builds and submits ray tracing commands
	/// </summary>
	void BuildAndSubmitRayTracingCommandBuffer(int cameraInstanceId, VkCommandBuffer commandBuffer, uint64_t currentFrameNumber);

	void CopyRenderToRenderTarget(int cameraInstanceId, VkCommandBuffer commandBuffer);

	/// <summary>
	/// Builds descriptor buffer infos for descriptor sets
	/// </summary>
	void BuildDescriptorBufferInfos(int cameraInstanceId, uint64_t currentFrameNumber);

	/// <summary>
	/// Create the descriptor pool for generating descriptor sets
	/// </summary>
	void CreateDescriptorPool();

	/// <summary>
	/// Update the descriptor sets for the shader
	/// </summary>
	void UpdateDescriptorSets(int cameraInstanceId, uint64_t currentFrameNumber);


	void GarbageCollect(uint64_t frameCount);
};

#endif