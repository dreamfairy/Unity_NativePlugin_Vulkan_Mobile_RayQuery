#pragma once

#include "Buffer.h"
#include "Image.h"
#include "IResource.h"
#include "PlatformBase.h"

#include <vector>
#include <map>
#include <memory>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
using mat4 = glm::highp_mat4;
using vec4 = glm::highp_vec4;


#define DESCRIPTOR_SET_SIZE                       8

// Descriptor set bindings
#define DESCRIPTOR_SET_ACCELERATION_STRUCTURE     0
#define DESCRIPTOR_BINDING_ACCELERATION_STRUCTURE 0

#define DESCRIPTOR_SET_SCENE_DATA                 0
#define DESCRIPTOR_BINDING_SCENE_DATA             1

#define DESCRIPTOR_SET_CAMERA_DATA                0
#define DESCRIPTOR_BINDING_CAMERA_DATA            2

#define DESCRIPTOR_SET_RENDER_TARGET              1
#define DESCRIPTOR_BINDING_RENDER_TARGET          0

#define DESCRIPTOR_SET_FACE_DATA                  2
#define DESCRIPTOR_BINDING_FACE_DATA              0

#define DESCRIPTOR_SET_VERTEX_ATTRIBUTES          3
#define DESCRIPTOR_BINDING_VERTEX_ATTRIBUTES      0

#define DESCRIPTOR_SET_LIGHTS_DATA                4
#define DESCRIPTOR_BINDING_LIGHTS_DATA            0

#define DESCRIPTOR_SET_MATERIALS_DATA             5
#define DESCRIPTOR_BINDING_MATERIALS_DATA         0

#define DESCRIPTOR_SET_TEXTURES_DATA              6
#define DESCRIPTOR_BINDING_TEXTURES_DATA          0

#define DESCRIPTOR_SET_INSTANCE_DATA              7
#define DESCRIPTOR_BINDING_INSTANCE_DATA          0

#define PRIMARY_HIT_SHADERS_INDEX   0
#define PRIMARY_MISS_SHADERS_INDEX  0
#define SHADOW_HIT_SHADERS_INDEX    1
#define SHADOW_MISS_SHADERS_INDEX   1


#define LOCATION_PRIMARY_RAY    0
#define LOCATION_SHADOW_RAY     1

#define RAYTRACE_MAX_RECURSION 10

#ifdef __cplusplus

#define align4  alignas(4)
#define align8  alignas(8)
#define align16 alignas(16)
#define align64 alignas(64)

#else

#define align4
#define align8
#define align16
#define align64 

#endif

typedef struct float4 {
	float x;
	float y;
	float z;
	float w;
} float4;

/// <summary>
/// Convert a float array representing a 4x4 matrix from Unity to a 4x4 matrix for Vulkan
/// </summary>
/// <param name="src"></param>
/// <param name="dst"></param>
void static FloatArrayToMatrix(const float* src, mat4& dst) {

	//// Unity is column major, but Vulkan is row major.  Convert here
	dst = mat4(src[0], src[4], src[8], src[12],
		src[1], src[5], src[9], src[13],
		src[2], src[6], src[10], src[14],
		src[3], src[7], src[11], src[15]);
}

void static FloatArrayToMatrixNoTranspose(const float* src, mat4& dst) {

	// Unity is column major, but Vulkan is row major.  Convert here
	dst = mat4(src[0], src[1], src[2], src[3],
		src[4], src[5], src[6], src[7],
		src[8], src[9], src[10], src[11],
		src[12], src[13], src[14], src[15]);
}


namespace VulkanRT
{
	class VulkanRTData
	{
	public:
		struct RayTracerAccelerationStructure
		{
			RayTracerAccelerationStructure()
				: accelerationStructure(VK_NULL_HANDLE)
				, deviceAddress(0)
				, buffer(Buffer())
			{}

			VkAccelerationStructureKHR    accelerationStructure;
			uint64_t                      deviceAddress;
			Buffer                buffer;
		};

		struct RayTracerMeshSharedData
		{
			RayTracerMeshSharedData()
				: sharedMeshInstanceId(-1)
				, vertexCount(0)
				, indexCount(0)
				, blas(RayTracerAccelerationStructure())
			{}

			int sharedMeshInstanceId;

			int vertexCount;
			int indexCount;

			Buffer vertexBuffer;         
			Buffer indexBuffer;           

			RayTracerAccelerationStructure blas;
		};


		struct RayTracerAccelerationStructureBuildInfo
		{
			VkAccelerationStructureBuildGeometryInfoKHR& accelerationBuildGeometryInfo;
		};

		struct RayTracerMeshInstanceData
		{
			RayTracerMeshInstanceData()
				: gameObjectInstanceId(0)
				, sharedMeshInstanceId(0)
			{}

			int32_t  gameObjectInstanceId;
			int32_t  sharedMeshInstanceId;
			mat4     localToWorld;
			mat4     worldToLocal;

			Buffer instanceData;
		};

		struct RayTracerGarbageBuffer
		{
			RayTracerGarbageBuffer()
				: frameCount(0)
			{}

			std::unique_ptr<IResource> buffer;
			uint64_t frameCount;
		};
	};
	
}

