#pragma once

namespace VulkanRT
{
	/// <summary>
	/// Interface for Vulkan resource.  Used by garbage collection to destroy resources after all frames using it have been completed
	/// </summary>
	class IResource
	{
	public:
		virtual ~IResource() { }
		virtual void Destroy() = 0;
	};
}
