#pragma once

#include "PlatformBase.h"

#if SUPPORT_VULKAN

#include "IResource.h"
#include <string>


#ifndef UNITY_VULKAN_HEADER
#define UNITY_VULKAN_HEADER <vulkan/vulkan.h>
#endif

#define VK_NO_PROTOTYPES
#include UNITY_VULKAN_HEADER

#include "Volk/volk.h"


namespace VulkanRT
{
	/// <summary>
   /// Represents a Vulkan buffer resource
   /// </summary>
	class Buffer : public IResource {
	public:
		static const VkMemoryPropertyFlags kDefaultMemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		Buffer();
		~Buffer();

		/// <summary>
		/// Create buffer
		/// </summary>
		/// <param name="size"></param>
		/// <param name="usage"></param>
		/// <param name="memoryProperties"></param>
		/// <returns></returns>
		VkResult Create(std::string name, VkDevice device, VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties);

		/// <summary>
		/// Destroy buffer
		/// </summary>
		virtual void Destroy();

		/// <summary>
		/// Get a handle to GPU memory 
		/// </summary>
		/// <param name="size"></param>
		/// <param name="offset"></param>
		/// <returns></returns>
		void* Map(VkDeviceSize size = UINT64_MAX, VkDeviceSize offset = 0) const;

		/// <summary>
		/// Free handle to GPU memory
		/// </summary>
		void Unmap() const;

		/// <summary>
		/// Upload data to GPU
		/// </summary>
		/// <param name="data"></param>
		/// <param name="size"></param>
		/// <param name="offset"></param>
		/// <returns></returns>
		bool UploadData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0) const;

		// getters
		VkBuffer GetBuffer() const;
		VkDeviceSize GetSize() const;

		// Helpers to get device address
		VkDeviceOrHostAddressKHR GetBufferDeviceAddress() const;
		VkDeviceOrHostAddressConstKHR GetBufferDeviceAddressConst() const;

	private:
		VkDevice        device_;
		VkBuffer        buffer_;
		VkDeviceMemory  memory_;
		VkDeviceSize    size_;

		std::string name_;
	};
}
#endif