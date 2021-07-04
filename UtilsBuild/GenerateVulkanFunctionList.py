# MIT License
#
# Copyright (c) 2020 Sixshaman
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import urllib.request
import re
from bs4 import BeautifulSoup

header_start_h = """\
#pragma once

#include <vulkan/vulkan.h>

#ifdef VK_NO_PROTOTYPES

#define DECLARE_VULKAN_FUNCTION(funcName) extern PFN_##funcName funcName;

extern "C"
{
"""

header_end = """\
}

#endif"""

header_start_cpp = """\
#include "VulkanFunctions.hpp"

#ifdef VK_NO_PROTOTYPES

#define DEFINE_VULKAN_FUNCTION(funcName) PFN_##funcName funcName;

extern "C"
{
"""

header_end_cpp = """\
}

#endif"""

class_start_h = """\
#pragma once

#ifdef _WIN32

#include <Windows.h>
using LIBRARY_TYPE = HMODULE;

#else

//TODO: Linux/etc. library

#endif

#include <vulkan/vulkan.h>

namespace Vulkan
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
}"""

class_start_cpp = """\
#include "VulkanFunctionsLibrary.hpp"
#include "VulkanFunctions.hpp"
#include <cassert>

Vulkan::FunctionsLibrary::FunctionsLibrary(): mLibrary(nullptr)
{
#ifdef WIN32
	mLibrary = LoadLibraryA("vulkan-1.dll");
	assert(mLibrary != 0);

	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)(GetProcAddress(mLibrary, "vkGetInstanceProcAddr"));
#endif // WIN32
}

Vulkan::FunctionsLibrary::~FunctionsLibrary()
{
#ifdef WIN32
	FreeLibrary(mLibrary);
#endif // WIN32
}

"""

def open_vk_spec(url):
	with urllib.request.urlopen(url) as response:
	    spec_data = response.read()
	    return spec_data.decode("utf8")

def parse_funcs(spec_contents):
	spec_soup   = BeautifulSoup(spec_contents, features="xml")

	spec_platform_defines = {}

	spec_platforms_block = spec_soup.find("platforms")
	if spec_platforms_block is not None:
	    spec_platform_tags = spec_platforms_block.find_all("platform")
	    for platform_tag in spec_platform_tags:
	        spec_platform_defines[platform_tag["name"]] = platform_tag["protect"]

	spec_extensions = {}

	extension_blocks = spec_soup.find_all("extension")
	for extension_block in extension_blocks:
	    extension_name = extension_block["name"]

	    extension_define_tag = extension_block.find("enum", {"value": re.compile(".*" + extension_name +".*")})
	    if extension_define_tag is None:
	        continue

	    extension_define = extension_define_tag["name"]
	    if extension_define is None:
	        continue
	    
	    extension_functions = []
	    extension_function_tags = extension_block.find_all("command")
	    for type_tag in extension_function_tags:
	        extension_defined_func = type_tag["name"]
	        if extension_defined_func is not None:
	            extension_functions.append(extension_defined_func)

	    extension_platform = ""
	    if "platform" in extension_block.attrs:
	        extension_platform = extension_block["platform"]

	    extension_platform_define = ""
	    if extension_platform in spec_platform_defines:
	        extension_platform_define = spec_platform_defines[extension_platform]

	    for extension_func in extension_functions:
	        spec_extensions[extension_func] = (extension_name, extension_define, extension_platform_define)

	function_levels_by_func_name = \
	{
		"vkGetInstanceProcAddr":                  "OMNIPERCIEVING",
		"vkGetDeviceProcAddr":                    "INSTANCE",
		"vkCreateInstance":                       "GLOBAL",
		"vkEnumerateInstanceVersion":             "GLOBAL",
		"vkEnumerateInstanceLayerProperties":     "GLOBAL",
		"vkEnumerateInstanceExtensionProperties": "GLOBAL"
	}

	function_levels_by_first_param = \
	{
		"VkInstance":       "INSTANCE",
		"VkPhysicalDevice": "INSTANCE",
		"VkDevice":         "DEVICE",
		"VkQueue":          "DEVICE",
		"VkCommandBuffer":  "DEVICE",
		"VkRenderPass":     "DEVICE"
	}

	funcs = []

	command_blocks = spec_soup.find_all("command")
	for command_block in command_blocks:
		func_name = None
		proto_block = command_block.find("proto")

		if proto_block is not None:
			name_block = proto_block.find("name")

			if name_block is not None:
				func_name = name_block.text

		if func_name is None:
			continue

		func_level = None		
		if func_name in function_levels_by_func_name.keys():
			func_level = function_levels_by_func_name[func_name]
		else:
			#Function level (device, instance, global) is defined from the first parameter
			param_blocks = list(command_block.find_all("param"))

			if len(param_blocks) != 0:
				first_param = param_blocks[0]

				first_param_type = first_param.find("type")
				if first_param_type is not None and first_param_type.text in function_levels_by_first_param.keys():
					func_level = function_levels_by_first_param[first_param_type.text]

		if func_level is None:
			raise "Cannot guess function level"

		extension_define = ""
		platform_define  = ""

		if func_name in spec_extensions:
			func_requires = spec_extensions[func_name]

			extension_define = func_requires[1]
			platform_define  = func_requires[2]

		funcs.append((func_name, extension_define, platform_define, func_level))

		funcs = sorted(funcs, key=lambda struct_data: struct_data[1])
		funcs = sorted(funcs, key=lambda struct_data: struct_data[2])

	return funcs

def compile_cpp_header_h(funcs):
	cpp_data = ""

	cpp_data += header_start_h

	current_extension_define = ""
	current_platform_define  = ""
	tab_level                = ""
	for func in funcs:
		if current_extension_define != func[1] or current_platform_define != func[2]:
			if current_extension_define != "" or current_platform_define != "":
				cpp_data += "#endif\n"
				tab_level = ""

			if func[1] != "" or func[2] != "":
				tab_level = "\t"

				cpp_data += "\n#if "

				if func[1] != "":
					cpp_data += "defined(" + func[1] + ")"

					if func[2] != "":
						cpp_data += " && "
					else:
						cpp_data += "\n"

				if func[2] != "":
					cpp_data += "defined(" + func[2] + ")\n"

			current_extension_define = func[1]
			current_platform_define  = func[2]

		cpp_data += tab_level + "DECLARE_VULKAN_FUNCTION(" + func[0] + ")\n";

	if current_extension_define != "" or current_platform_define != "":
	    cpp_data += "#endif\n\n"

	cpp_data += header_end

	return cpp_data

def compile_cpp_header_cpp(funcs):
	cpp_data = ""

	cpp_data += header_start_cpp

	current_extension_define = ""
	current_platform_define  = ""
	tab_level                = ""
	for func in funcs:
		if current_extension_define != func[1] or current_platform_define != func[2]:
			if current_extension_define != "" or current_platform_define != "":
				cpp_data += "#endif\n"
				tab_level = ""

			if func[1] != "" or func[2] != "":
				tab_level = "\t"

				cpp_data += "\n#if "

				if func[1] != "":
					cpp_data += "defined(" + func[1] + ")"

					if func[2] != "":
						cpp_data += " && "
					else:
						cpp_data += "\n"

				if func[2] != "":
					cpp_data += "defined(" + func[2] + ")\n"

			current_extension_define = func[1]
			current_platform_define  = func[2]

		cpp_data += tab_level + "DEFINE_VULKAN_FUNCTION(" + func[0] + ")\n";

	if current_extension_define != "" or current_platform_define != "":
	    cpp_data += "#endif\n\n"

	cpp_data += header_end_cpp

	return cpp_data


def compile_cpp_class_h(funcs):
	cpp_data = ""
	
	cpp_data += class_start_h

	return cpp_data

def compile_cpp_class_cpp(funcs):
	cpp_data = ""
	
	cpp_data += class_start_cpp

	global_func_spacing_intendation   = 0;
	instance_func_spacing_intendation = 0;
	device_func_spacing_intendation   = 0;
	for func in funcs:
		if func[3] == "GLOBAL":
			global_func_spacing_intendation = max(global_func_spacing_intendation, len(func[0]))
		if func[3] == "INSTANCE":
			instance_func_spacing_intendation = max(instance_func_spacing_intendation, len(func[0]))
		if func[3] == "DEVICE":
			device_func_spacing_intendation = max(device_func_spacing_intendation, len(func[0]))

	current_extension_define = ""
	current_platform_define  = ""

	cpp_data += "void Vulkan::FunctionsLibrary::LoadGlobalFunctions()\n"
	cpp_data += "{\n"

	for func in funcs:
		if func[3] == "GLOBAL":
			if current_extension_define != func[1] or current_platform_define != func[2]:
				if current_extension_define != "" or current_platform_define != "":
					cpp_data += "#endif\n"
					tab_level = ""

				if func[1] != "" or func[2] != "":
					tab_level = "\t"

					cpp_data += "\n#if "

					if func[1] != "":
						cpp_data += "defined(" + func[1] + ")"

						if func[2] != "":
							cpp_data += " && "
						else:
							cpp_data += "\n"

					if func[2] != "":
						cpp_data += "defined(" + func[2] + ")\n"

				current_extension_define = func[1]
				current_platform_define  = func[2]

			spacing = " " * (global_func_spacing_intendation - len(func[0]))
			cpp_data += "\t" + func[0] + spacing + " = (PFN_" + func[0] + ")(vkGetInstanceProcAddr(nullptr, " + spacing + "\"" + func[0] + "\"));\n"

	if current_extension_define != "" or current_platform_define != "":
	    cpp_data += "#endif\n"

	cpp_data += "}\n"

	current_extension_define = ""
	current_platform_define  = ""

	cpp_data += "\n"

	cpp_data += "void Vulkan::FunctionsLibrary::LoadInstanceFunctions(VkInstance instance)\n"
	cpp_data += "{\n"

	for func in funcs:
		if func[3] == "INSTANCE":
			if current_extension_define != func[1] or current_platform_define != func[2]:
				if current_extension_define != "" or current_platform_define != "":
					cpp_data += "#endif\n"
					tab_level = ""

				if func[1] != "" or func[2] != "":
					tab_level = "\t"

					cpp_data += "\n#if "

					if func[1] != "":
						cpp_data += "defined(" + func[1] + ")"

						if func[2] != "":
							cpp_data += " && "
						else:
							cpp_data += "\n"

					if func[2] != "":
						cpp_data += "defined(" + func[2] + ")\n"

				current_extension_define = func[1]
				current_platform_define  = func[2]

			spacing = " " * (instance_func_spacing_intendation - len(func[0]))
			cpp_data += "\t" + func[0] + spacing + " = (PFN_" + func[0] + ")(vkGetInstanceProcAddr(instance, " + spacing + "\"" + func[0] + "\"));\n"

	if current_extension_define != "" or current_platform_define != "":
	    cpp_data += "#endif\n"

	cpp_data += "}\n"

	current_extension_define = ""
	current_platform_define  = ""

	cpp_data += "\n"

	cpp_data += "void Vulkan::FunctionsLibrary::LoadDeviceFunctions(VkDevice device)\n"
	cpp_data += "{\n"

	for func in funcs:
		if func[3] == "DEVICE":
			if current_extension_define != func[1] or current_platform_define != func[2]:
				if current_extension_define != "" or current_platform_define != "":
					cpp_data += "#endif\n"
					tab_level = ""

				if func[1] != "" or func[2] != "":
					tab_level = "\t"

					cpp_data += "\n#if "

					if func[1] != "":
						cpp_data += "defined(" + func[1] + ")"

						if func[2] != "":
							cpp_data += " && "
						else:
							cpp_data += "\n"

					if func[2] != "":
						cpp_data += "defined(" + func[2] + ")\n"

				current_extension_define = func[1]
				current_platform_define  = func[2]

			spacing = " " * (device_func_spacing_intendation - len(func[0]))
			cpp_data += "\t" + func[0] + spacing + " = (PFN_" + func[0] + ")(vkGetDeviceProcAddr(device, " + spacing + "\"" + func[0] + "\"));\n"
	
	if current_extension_define != "" or current_platform_define != "":
	    cpp_data += "#endif\n"

	cpp_data += "}\n"

	return cpp_data

def save_file(contents, filename):
	with open(filename, "w", encoding="utf-8") as out_file:
	    out_file.write(contents)

if __name__ == "__main__":
	spec_text = open_vk_spec("https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/xml/vk.xml") #Get it directly from the master branch
	funcs     = parse_funcs(spec_text)

	cpp_header_data_h   = compile_cpp_header_h(funcs)
	cpp_header_data_cpp = compile_cpp_header_cpp(funcs)
	cpp_class_data_h    = compile_cpp_class_h(funcs)
	cpp_class_data_cpp  = compile_cpp_class_cpp(funcs)

	save_file(cpp_header_data_h,   "VulkanFunctions.hpp")
	save_file(cpp_header_data_cpp, "VulkanFunctions.cpp")
	save_file(cpp_class_data_h,    "VulkanFunctionsLibrary.hpp")
	save_file(cpp_class_data_cpp,  "VulkanFunctionsLibrary.cpp")