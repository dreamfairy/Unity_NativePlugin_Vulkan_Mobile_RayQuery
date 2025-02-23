#include "RenderAPI.h"
#include "PlatformBase.h"
#include "Unity/IUnityGraphics.h"
#include "NativeLogger.h"
#include <string.h>

RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType)
{
	#if SUPPORT_VULKAN
	
	NativeLogger::LogInfo("Support Vulkan");

	if (apiType == kUnityGfxRendererVulkan)
	{
		NativeLogger::LogInfo("CreateVulkan RenderAPI");
		extern RenderAPI* CreateRenderAPI_VulkanRayQuery();
		return CreateRenderAPI_VulkanRayQuery();
	}
	#else
	NativeLogger::LogInfo("CreateVulkan RenderAPI But Int No Support Macro");
	#endif
	// Unknown or unsupported graphics API
	return NULL;
}