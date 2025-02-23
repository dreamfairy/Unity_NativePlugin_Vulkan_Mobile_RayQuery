#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"
#include "PlatformBase.h"

using vec2 = glm::highp_vec2;
using vec3 = glm::highp_vec3;
using vec4 = glm::highp_vec4;
using mat4 = glm::highp_mat4;
using quat = glm::highp_quat;



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


struct GlobalUniform
{
	align64 mat4 view_proj;
	alignas(16) vec3 camera_position;
	alignas(16) vec3 light_position;
	alignas(16) vec3 light_direction;
};

struct PerMeshUniform
{
	align64 mat4 model;
};

struct RayQueryVertex
{
	alignas(16) vec3 position;
	alignas(16) vec3 normal;
};

struct RayQueryTLASInstanceData
{
	align64 mat4        localToWorld;
	align64 mat4        worldToLocal;
};