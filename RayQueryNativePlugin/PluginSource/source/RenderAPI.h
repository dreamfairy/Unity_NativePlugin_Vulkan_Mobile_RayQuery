#pragma once

#include "Unity/IUnityGraphics.h"

#include <stddef.h>
#include <string>


struct IUnityInterfaces;

// Super-simple "graphics abstraction". This is nothing like how a proper platform abstraction layer would look like;
// all this does is a base interface for whatever our plugin sample needs. Which is only "draw some triangles"
// and "modify a texture" at this point.
//
// There are implementations of this base class for D3D9, D3D11, OpenGL etc.; see individual RenderAPI_* files.
class RenderAPI
{
public:
	

public:
	virtual ~RenderAPI() { }


	// Process general event like initialization, shutdown, device loss/reset etc.
	virtual bool ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) = 0;

	//Vulkan RT
	virtual void Prepare() = 0;
	virtual void SetShaderData(int, const unsigned char* shaderData, int dataSize) = 0;
	 /// <summary>
		/// Add transform for an instance to be build on the tlas
		/// </summary>
		/// <param name="gameObjectInstanceId"></param>
		/// <param name="meshInstanceId"></param>
		/// <param name="sharedMeshInstanceId"></param>
		/// <param name="l2wMatrix"></param>
	virtual AddResourceResult AddTlasInstance(int gameObjectInstanceId, int sharedMeshInstanceId, float* l2wMatrix, float* w2lMatrix) = 0;
	virtual AddResourceResult AddSharedMesh(int sharedMeshInstanceId, float* verticesArray, float* normalsArray, float* tangentsArray, float* uvsArray, int vertexCount, int* indicesArray, int indexCount) = 0;
	virtual void UpdateTlasInstance(int gameObjectInstanceId, float* l2wMatrix, float* w2lMatrix) = 0;

	/// <summary>
	/// Removes instance to be removed on next tlas build
	/// </summary>
	/// <param name="meshInstanceIndex"></param>
	virtual void RemoveTlasInstance(int gameObjectInstanceId) = 0;

	virtual void TraceRays(int cameraInstanceId) = 0;


	virtual AddResourceResult AddLight(int lightInstanceId, float x, float y, float z, float dx, float dy, float dz, float r, float g, float b, float bounceIntensity, float intensity, float range, float spotAngle, int type, bool enabled) = 0;
	virtual void UpdateLight(int lightInstanceId, float x, float y, float z, float dx, float dy, float dz, float r, float g, float b, float bounceIntensity, float intensity, float range, float spotAngle, int type, bool enabled) = 0;
	virtual void RemoveLight(int lightInstanceId) = 0;

	virtual void UpdateCameraMat(float x, float y, float z, float* world2cameraProj) = 0;
	virtual void UpdateModelMat(int sharedMeshInstanceId, float* l2w) = 0;
};


// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);