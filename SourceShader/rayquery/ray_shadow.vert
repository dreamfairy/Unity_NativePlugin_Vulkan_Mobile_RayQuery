#version 460
#extension GL_EXT_ray_query : enable

/* Copyright (c) 2021-2024 Holochip Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(set = 0, binding = 1) uniform GlobalUniform
{
	mat4 view_proj;
	vec3 camera_position;
	vec3 light_position;
	vec3 light_direction;
}
global_uniform;

layout(push_constant) uniform ConstantsUniform
{
	mat4 model;
}
pushConstant;

layout(location = 0) out vec4 o_pos;
layout(location = 1) out vec3 o_normal;
layout(location = 2) out vec4 scene_pos;        // scene with respect to BVH coordinates

void main(void)
{
	vec4 wPos = pushConstant.model * vec4(position, 1.0);

	// We want to be able to perform ray tracing, so don't apply any matrix to scene_pos
	scene_pos = wPos;

	vec4 wNormal = pushConstant.model * vec4(normal,0);
	wNormal = normalize(wNormal);
	o_normal = wNormal.xyz;

	gl_Position = global_uniform.view_proj * wPos;
	//gl_Position.y = 1.0 - gl_Position.y;

	o_pos = gl_Position;
}
