#pragma once

#include "PlatformBase.h"
#include <vector>

#ifndef UNITY_VULKAN_HEADER
#define UNITY_VULKAN_HEADER <vulkan/vulkan.h>
#endif

#define VK_NO_PROTOTYPES
#include UNITY_VULKAN_HEADER

#if SUPPORT_VULKAN

namespace VulkanRT
{
	/// <summary>
	/// Represents a shader for a shader binding table
	/// </summary>
	class Shader {
	public:
		Shader(VkDevice device);
		~Shader();

		bool LoadFromShaderByte(std::vector<char> content, int size);

		/// <summary>
		/// Load shader from file
		/// </summary>
		/// <param name="fileName"></param>
		/// <returns></returns>
		bool LoadFromFile(const wchar_t* fileName);

		/// <summary>
		/// Destory shader
		/// </summary>
		void Destroy();

		/// <summary>
		/// Get shader stage information
		/// </summary>
		/// <param name="stage"></param>
		/// <returns></returns>
		VkPipelineShaderStageCreateInfo GetShaderStage(VkShaderStageFlagBits stage);
		VkShaderModule GetShaderModule() { return shaderModule_; };

	private:
		VkDevice        device_;
		VkShaderModule  shaderModule_;
	};
}

#endif