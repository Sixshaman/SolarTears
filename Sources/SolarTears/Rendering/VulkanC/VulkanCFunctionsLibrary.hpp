#pragma once

#ifdef _WIN32

#include <Windows.h>
using LIBRARY_TYPE = HMODULE;

#else

//TODO: Linux/etc. library

#endif

#include <vulkan/vulkan.h>

namespace VulkanCBindings
{
	class FunctionsLibrary
	{
	public:
		FunctionsLibrary();
		~FunctionsLibrary();

		void LoadGlobalFunctions();
		void LoadInstanceFunctions(VkInstance instance);
		void LoadDeviceFunctions(VkDevice device);

	private:
		LIBRARY_TYPE mLibrary;
	};
}