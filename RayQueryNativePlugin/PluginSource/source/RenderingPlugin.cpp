// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"
#include "NativeLogger.h"

#include <assert.h>
#include <math.h>
#include <vector>

#define USE_RDC 0

#if USE_RDC
#include <windows.h>
#include <libloaderapi.h>
#endif


#define PLUGIN_CHECK()  if (s_CurrentAPI == nullptr) { return; }
#define PLUGIN_CHECK_RETURN(returnValue)  if (s_CurrentAPI == nullptr) { return returnValue; }

//Log

extern "C" UNITY_INTERFACE_EXPORT void SetLogger(LoggerFuncPtr fp)
{
	NativeLogger::SetLogger(fp);
}

extern "C" UNITY_INTERFACE_EXPORT void SetIntLogger(LoggerIntFuncPtr fp)
{
	NativeLogger::SetIntLogger(fp);
}

extern "C" UNITY_INTERFACE_EXPORT void TestLogger()
{
	NativeLogger::LogInfo("Test Info");
	NativeLogger::LogWarn("Test Warning");
	NativeLogger::LogError("Test Error");

	NativeLogger::LogIntInfo(1);
	NativeLogger::LogIntWarn(2);
	NativeLogger::LogIntError(3);

	NativeLogger::Flush();
}

// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;
static bool IsUnityPlugunLoadCalled = false;
static bool IsSupportVulkan = false;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	IsUnityPlugunLoadCalled = true;

#if USE_RDC
	HMODULE rdcLib = LoadLibrary("C:\\Program Files\\RenderDoc\\renderdoc.dll");
	if (NULL != rdcLib) {
		NativeLogger::LogInfo("Load RDC Done!");
	}
#endif

	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

	NativeLogger::LogInfo("UnityPluginLoad RegisterDeviceEventCallback");
	
#if SUPPORT_VULKAN
	IsSupportVulkan = true;

	extern void RenderAPI_VulkanRayQuery_OnPluginLoad(IUnityInterfaces*);
	RenderAPI_VulkanRayQuery_OnPluginLoad(unityInterfaces);

#endif // SUPPORT_VULKAN

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

#if UNITY_WEBGL
typedef void	(UNITY_INTERFACE_API * PluginLoadFunc)(IUnityInterfaces* unityInterfaces);
typedef void	(UNITY_INTERFACE_API * PluginUnloadFunc)();

extern "C" void	UnityRegisterRenderingPlugin(PluginLoadFunc loadPlugin, PluginUnloadFunc unloadPlugin);

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterPlugin()
{
	UnityRegisterRenderingPlugin(UnityPluginLoad, UnityPluginUnload);
}
#endif

// --------------------------------------------------------------------------
// GraphicsDeviceEvent


static RenderAPI* s_CurrentAPI = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;
static int s_UnityAPIType = -1;


static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	s_UnityAPIType = -1;
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		assert(s_CurrentAPI == NULL);
		s_DeviceType = s_Graphics->GetRenderer();

		s_UnityAPIType = (int)s_DeviceType;

		s_CurrentAPI = CreateRenderAPI(s_DeviceType);

		NativeLogger::LogInfo("OnGraphicsDeviceEvent CreateRenderAPI");

		if (nullptr == s_CurrentAPI) {
			NativeLogger::LogInfo("OnGraphicsDeviceEvent CreateRenderAPI Failed");
		}
	}

	// Let the implementation process the device related events
	//bool success = false;
	if (s_CurrentAPI)
	{
		s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);

		NativeLogger::LogInfo("OnGraphicsDeviceEvent ProcessDeviceEvent");
	}

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		if (nullptr != s_CurrentAPI) {
			delete s_CurrentAPI;
			s_CurrentAPI = NULL;
		}
		
		s_DeviceType = kUnityGfxRendererNull;
	}
}

//Vulkan RT

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddLight(int lightInstanceId, float x, float y, float z, float dx, float dy, float dz, float r, float g, float b, float bounceIntensity, float intensity, float range, float spotAngle, int type, bool enabled)
{
	PLUGIN_CHECK_RETURN(-1);

	return (int)s_CurrentAPI->AddLight(lightInstanceId, x, y, dx, dy, dz, z, r, g, b, bounceIntensity, intensity, range, spotAngle, type, enabled);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateLight(int lightInstanceId, float x, float y, float z, float dx, float dy, float dz, float r, float g, float b, float bounceIntensity, float intensity, float range, float spotAngle, int type, bool enabled)
{
	PLUGIN_CHECK();
	s_CurrentAPI->UpdateLight(lightInstanceId, x, y, z, dx, dy, dz, r, g, b, bounceIntensity, intensity, range, spotAngle, type, enabled);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RemoveLight(int lightInstanceId)
{
	PLUGIN_CHECK();

	s_CurrentAPI->RemoveLight(lightInstanceId);
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddSharedMesh(int sharedMeshInstanceId, float* verticesArray, float* normalsArray, float* tangentsArray, float* uvsArray, int vertexCount, int* indicesArray, int indexCount)
{
	PLUGIN_CHECK_RETURN(-1);

	return (int)s_CurrentAPI->AddSharedMesh(sharedMeshInstanceId, verticesArray, normalsArray, tangentsArray, uvsArray, vertexCount, indicesArray, indexCount);
}


extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API AddTlasInstance(int gameObjectInstanceId, int sharedMeshInstanceId, float* l2wMatrix, float* w2lMatrix)
{
	PLUGIN_CHECK_RETURN(-1);

	return (int)s_CurrentAPI->AddTlasInstance(gameObjectInstanceId, sharedMeshInstanceId, l2wMatrix, w2lMatrix);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateTlasInstance(int gameObjectInstanceId, float* l2wMatrix, float* w2lMatrix)
{
	PLUGIN_CHECK();

	s_CurrentAPI->UpdateTlasInstance(gameObjectInstanceId, l2wMatrix, w2lMatrix);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RemoveTlasInstance(int gameObjectInstanceId)
{
	PLUGIN_CHECK();

	s_CurrentAPI->RemoveTlasInstance(gameObjectInstanceId);
}

enum class Events
{
	None = 0,
	TraceRays = 1
};

static void UNITY_INTERFACE_API OnEventAndData(int eventId, void* data)
{
	PLUGIN_CHECK();

	auto event = static_cast<Events>(eventId);

	switch (event)
	{
	case Events::TraceRays:
		int cameraInstanceId = *static_cast<int*>(data);
		s_CurrentAPI->TraceRays(cameraInstanceId);
		break;
	}
}


extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetEventAndDataFunc()
{
	return OnEventAndData;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetShaderData(int type, const unsigned char* shaderData, int dataSize)
{
	if (NULL == s_CurrentAPI) {
		NativeLogger::LogInfo("SetShaderData Failed, null API");
	}
	PLUGIN_CHECK();
	s_CurrentAPI->SetShaderData(type, shaderData, dataSize);
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API Prepare()
{
	PLUGIN_CHECK();
	s_CurrentAPI->Prepare();
}


extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateCameraMat(float x, float y, float z, float* world2cameraProj)
{
	PLUGIN_CHECK();
	s_CurrentAPI->UpdateCameraMat(x, y, z,world2cameraProj);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UpdateModelMat(int sharedMeshInstanceId, float* local2World)
{
	PLUGIN_CHECK();
	s_CurrentAPI->UpdateModelMat(sharedMeshInstanceId, local2World);
}