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

from os import name
import urllib.request
import re
from bs4 import BeautifulSoup
import os

start_ogl_function_list_h = """\
#pragma once

#ifdef WIN32
#include <Windows.h>
#endif

#include <GL/GL.h>
#include <GL/glext.h>

#define DECLARE_OGL_FUNCTION(type, name) extern type name;

"""

mid_gl_function_list_h = """\
extern "C"
{
"""

end_gl_function_list_h = """\
}"""

start_ogl_function_list_cpp = """\
#include "OGLFunctions.hpp"

#define DEFINE_OGL_FUNCTION(type, name) type name = nullptr;

extern "C"
{
"""

end_gl_function_list_cpp = """\
}"""

start_ogl_function_loader_inl = """\
void LoadOGLFunctionsFromContext()
{
"""

end_gl_function_loader_inl = """\
}"""

start_wgl_function_list_h = """\
#pragma once

#include <Windows.h>
#include <GL/GL.h>
#include <GL/wgl.h>
#include <GL/wglext.h>

#define DECLARE_WGL_FUNCTION(type, name) extern type name;

extern "C"
{"""

start_wgl_function_list_cpp = """\
#include "OGLFunctionsWin32.hpp"

#define DEFINE_WGL_FUNCTION(type, name) type name = nullptr;

extern "C"
{"""

start_wgl_function_loader_inl = """\
void LoadWGLFunctionsFromContext()
{"""

def open_ogl_spec(url):
	with urllib.request.urlopen(url) as response:
	    spec_data = response.read()
	    return spec_data.decode("utf8")

def func_type_from_name(func_name):
	return "PFN" + func_name.upper() + "PROC"

def parse_ogl_funcs(spec_contents, api):
	spec_soup = BeautifulSoup(spec_contents, features="xml")


	all_func_names = []
	spec_commands_block = spec_soup.find("commands")
	command_blocks = spec_commands_block.find_all("command")
	for command_block in command_blocks:
		proto_block = command_block.find("proto")
		if proto_block is None:
			raise "No function prototype"
		
		name_block = proto_block.find("name")
		if name_block is None:
			print(proto_block)

		all_func_names.append(name_block.string)


	all_core_function_dict  = {}
	removed_function_names = set()
	version_names          = []
	feature_blocks = spec_soup.find_all("feature")
	for feature_block in feature_blocks:
		if feature_block["api"] == api:
			remove_blocks = feature_block.find_all("remove")

			for remove_block in remove_blocks:
				removed_command_blocks = remove_block.find_all("command")

				for removed_command_block in removed_command_blocks:
					removed_function_names.add(removed_command_block["name"])

			feature_version = feature_block["name"]
			version_names.append(feature_version)

			require_blocks = feature_block.find_all("require")
			for require_block in require_blocks:
				require_command_blocks = require_block.find_all("command")
				for require_command_block in require_command_blocks:
					all_core_function_dict[require_command_block["name"]] = feature_version

	all_extension_function_dict        = {}
	supported_extension_function_names = set()
	extensions_block = spec_soup.find("extensions")
	if extensions_block is not None:

		extension_blocks = extensions_block.find_all("extension")
		for extension_block in extension_blocks:
			extension_support_types   = extension_block["supported"].split('|')
			extension_supported_in_gl = (api in extension_support_types)
			
			extension_name = extension_block["name"]

			require_blocks = extension_block.find_all("require")
			for require_block in require_blocks:
				block_supported_in_gl = ("api" not in require_block.attrs.keys() or require_block["api"] == api)

				extension_command_blocks = require_block.find_all("command")
				for extension_command_block in extension_command_blocks:
					all_extension_function_dict[extension_command_block["name"]] = extension_name

					if block_supported_in_gl and extension_supported_in_gl:
						supported_extension_function_names.add(extension_command_block["name"])


	func_names = []
	for func_name in all_func_names:
		function_supported = False
		function_extension = ""
		function_version   = ""

		if func_name in all_core_function_dict.keys():
			function_supported = func_name not in removed_function_names
			function_extension = ""
			function_version   = all_core_function_dict[func_name]

		if not function_supported and func_name in all_extension_function_dict.keys():
			function_supported = (func_name in supported_extension_function_names) and (func_name not in removed_function_names)
			function_extension = all_extension_function_dict[func_name]
			function_version   = "VERSION_EXT"
	
		if function_supported:
			func_names.append((func_name, function_version, function_extension))

	func_names = sorted(func_names, key=lambda func_name: func_name[2])
	func_names = sorted(func_names, key=lambda func_name: func_name[1])

	return func_names, version_names


def create_function_declarations(func_names, use_version_indent, use_extension_indent):
	cpp_declarations = ""

	max_len = max([len(func_type_from_name(func_name)) for func_name in func_names])
	for func_name in func_names:
		func_type = func_type_from_name(func_name)

		if use_version_indent:
			cpp_declarations += "\t"

		if use_extension_indent:
			cpp_declarations += "\t"

		cpp_declarations += "DECLARE_OGL_FUNCTION("
		cpp_declarations += func_type
		cpp_declarations += ","
		cpp_declarations += (max_len - len(func_type) + 1) * ' '
		cpp_declarations += func_name
		cpp_declarations += ")"
		cpp_declarations += "\n"

	return cpp_declarations


def create_function_load_declarations(func_names, use_version_indent, use_extension_indent):
	cpp_declarations = ""

	max_len = max([len(func_type_from_name(func_name)) for func_name in func_names])
	for func_name in func_names:
		func_type = func_type_from_name(func_name)

		if use_version_indent:
			cpp_declarations += "\t"

		if use_extension_indent:
			cpp_declarations += "\t"

		cpp_declarations += "LOAD_OGL_FUNCTION("
		cpp_declarations += func_type
		cpp_declarations += ","
		cpp_declarations += (max_len - len(func_type) + 1) * ' '
		cpp_declarations += func_name
		cpp_declarations += ");"
		cpp_declarations += "\n"

	return cpp_declarations


def create_function_null_definitions(func_names, use_version_indent, use_extension_indent):
	cpp_definitions = ""

	max_len = max([len(func_type_from_name(func_name)) for func_name in func_names])
	for func_name in func_names:
		func_type = func_type_from_name(func_name)

		if use_version_indent:
			cpp_definitions += "\t"

		if use_extension_indent:
			cpp_definitions += "\t"

		cpp_definitions += "DEFINE_OGL_FUNCTION("
		cpp_definitions += func_type
		cpp_definitions += ","
		cpp_definitions += (max_len - len(func_type) + 1) * ' '
		cpp_definitions += func_name
		cpp_definitions += ")"
		cpp_definitions += "\n"

	return cpp_definitions


def create_function_wgl_declarations(func_names, use_extension_indent):
	cpp_declarations = ""

	max_len = max([len(func_type_from_name(func_name)) for func_name in func_names])
	for func_name in func_names:
		func_type = func_type_from_name(func_name)

		if use_extension_indent:
			cpp_declarations += "\t"

		cpp_declarations += "DECLARE_WGL_FUNCTION("
		cpp_declarations += func_type
		cpp_declarations += ","
		cpp_declarations += (max_len - len(func_type) + 1) * ' '
		cpp_declarations += func_name
		cpp_declarations += ")"
		cpp_declarations += "\n"

	return cpp_declarations

def create_function_wgl_definitions(func_names, use_extension_indent):
	cpp_definitions = ""

	max_len = max([len(func_type_from_name(func_name)) for func_name in func_names])
	for func_name in func_names:
		func_type = func_type_from_name(func_name)

		if use_extension_indent:
			cpp_definitions += "\t"

		cpp_definitions += "DEFINE_WGL_FUNCTION("
		cpp_definitions += func_type
		cpp_definitions += ","
		cpp_definitions += (max_len - len(func_type) + 1) * ' '
		cpp_definitions += func_name
		cpp_definitions += ")"
		cpp_definitions += "\n"

	return cpp_definitions


def compile_ogl_function_list_h(funcs, versions):
	cpp_data = ""

	cpp_data += start_ogl_function_list_h

	win32_excluded_versions = ["GL_VERSION_1_0", "GL_VERSION_1_1"]
	was_in_win32_version    = False
	for ogl_version in versions:
		win32_version = (ogl_version in win32_excluded_versions)

		if win32_version and not was_in_win32_version:
			cpp_data += "#ifndef WIN32\n"

		if not win32_version and was_in_win32_version:
			cpp_data += "#endif\n\n"

		cpp_data += "#define " + "USE_" + ogl_version + "_FUNCTIONS\n"

		was_in_win32_version = win32_version

	cpp_data += "#define USE_GL_EXTENSION_FUNCTIONS"
	cpp_data += "\n\n"

	cpp_data += mid_gl_function_list_h

	current_version          = ""
	current_extension        = ""
	current_indent_functions = []
	for func in funcs:
		func_name      = func[0]
		func_version   = func[1]
		func_extension = func[2]

		if func_version != current_version:
			if current_version != "":
				cpp_data += create_function_declarations(current_indent_functions, True, current_extension != "")
				current_indent_functions.clear()

				cpp_data += "#endif"
				cpp_data += "\n\n"

			version_define = "USE_" + func_version + "_FUNCTIONS"
			if func_version == "VERSION_EXT":
				version_define = "USE_GL_EXTENSION_FUNCTIONS"

			cpp_data += "#ifdef " + version_define + "\n"
			current_version = func_version

		if func_extension != current_extension:
			if current_extension != "":
				cpp_data += create_function_declarations(current_indent_functions, current_version != "", True)
				current_indent_functions.clear()

				cpp_data += "#endif"
				cpp_data += "\n\n"

			cpp_data += "#ifdef " + func_extension + "\n"
			current_extension = func_extension

		current_indent_functions.append(func_name)
		
	cpp_data += create_function_declarations(current_indent_functions, current_version != "", current_extension != "")

	if current_extension != "":
		cpp_data += "#endif\n"

	if current_version != "":
		cpp_data += "#endif\n"

	cpp_data += end_gl_function_list_h

	return cpp_data

def compile_ogl_function_list_cpp(funcs):
	cpp_data = ""

	cpp_data += start_ogl_function_list_cpp

	current_version          = ""
	current_extension        = ""
	current_indent_functions = []
	for func in funcs:
		func_name      = func[0]
		func_version   = func[1]
		func_extension = func[2]

		if func_version != current_version:
			if current_version != "":
				cpp_data += create_function_null_definitions(current_indent_functions, True, current_extension != "")
				current_indent_functions.clear()

				cpp_data += "#endif"
				cpp_data += "\n\n"

			version_define = "USE_" + func_version + "_FUNCTIONS"
			if func_version == "VERSION_EXT":
				version_define = "USE_GL_EXTENSION_FUNCTIONS"

			cpp_data += "#ifdef " + version_define + "\n"
			current_version = func_version

		if func_extension != current_extension:
			if current_extension != "":
				cpp_data += create_function_null_definitions(current_indent_functions, current_version != "", True)
				current_indent_functions.clear()

				cpp_data += "#endif"
				cpp_data += "\n\n"

			cpp_data += "#ifdef " + func_extension + "\n"
			current_extension = func_extension

		current_indent_functions.append(func_name)
		
	cpp_data += create_function_null_definitions(current_indent_functions, current_version != "", current_extension != "")

	if current_extension != "":
		cpp_data += "#endif\n"

	if current_version != "":
		cpp_data += "#endif\n"

	cpp_data += end_gl_function_list_cpp

	return cpp_data

def compile_ogl_function_loader_inl(funcs):
	cpp_data = ""

	cpp_data += start_ogl_function_loader_inl

	current_version          = ""
	current_extension        = ""
	current_indent_functions = []
	for func in funcs:
		func_name      = func[0]
		func_version   = func[1]
		func_extension = func[2]

		if func_version != current_version:
			if current_version != "":
				cpp_data += create_function_load_declarations(current_indent_functions, True, current_extension != "")
				current_indent_functions.clear()

				cpp_data += "#endif"
				cpp_data += "\n\n"

			version_define = "USE_" + func_version + "_FUNCTIONS"
			if func_version == "VERSION_EXT":
				version_define = "USE_GL_EXTENSION_FUNCTIONS"

			cpp_data += "#ifdef " + version_define + "\n"
			current_version = func_version

		if func_extension != current_extension:
			if current_extension != "":
				cpp_data += create_function_load_declarations(current_indent_functions, current_version != "", True)
				current_indent_functions.clear()

				cpp_data += "#endif"
				cpp_data += "\n\n"

			cpp_data += "#ifdef " + func_extension + "\n"
			current_extension = func_extension

		current_indent_functions.append(func_name)
		
	cpp_data += create_function_load_declarations(current_indent_functions, current_version != "", current_extension != "")

	if current_extension != "":
		cpp_data += "#endif\n"

	if current_version != "":
		cpp_data += "#endif\n"

	cpp_data += end_gl_function_loader_inl

	return cpp_data


def compile_wgl_function_list_h(funcs):
	cpp_data = ""

	cpp_data += start_wgl_function_list_h

	current_extension = ""
	current_indent_functions = []
	for func in funcs:
		func_name      = func[0]
		func_extension = func[2]

		if func_extension != current_extension:
			if current_extension != "":
				cpp_data += create_function_wgl_declarations(current_indent_functions, True)
				current_indent_functions.clear()

				cpp_data += "#endif"
				cpp_data += "\n"

			current_extension = func_extension

			if current_extension != "":
				cpp_data += "\n#ifdef " + func_extension + "\n"

		if current_extension != "":
			current_indent_functions.append(func_name)
		
	if current_extension != "":
		cpp_data += create_function_wgl_declarations(current_indent_functions, current_extension != "")
		cpp_data += "#endif\n"

	cpp_data += end_gl_function_list_h

	return cpp_data


def compile_wgl_function_list_cpp(funcs):
	cpp_data = ""

	cpp_data += start_wgl_function_list_cpp

	current_extension = ""
	current_indent_functions = []
	for func in funcs:
		func_name      = func[0]
		func_extension = func[2]

		if func_extension != current_extension:
			if current_extension != "":
				cpp_data += create_function_wgl_definitions(current_indent_functions, True)
				current_indent_functions.clear()

				cpp_data += "#endif"
				cpp_data += "\n"

			current_extension = func_extension

			if current_extension != "":
				cpp_data += "\n#ifdef " + func_extension + "\n"

		if current_extension != "":
			current_indent_functions.append(func_name)
		
	if current_extension != "":
		cpp_data += create_function_wgl_definitions(current_indent_functions, current_extension != "")
		cpp_data += "#endif\n"

	cpp_data += end_gl_function_list_cpp

	return cpp_data


def compile_wgl_function_loader_inl(funcs):
	cpp_data = ""

	cpp_data += start_wgl_function_loader_inl

	current_extension = ""
	current_indent_functions = []
	for func in funcs:
		func_name      = func[0]
		func_extension = func[2]

		if func_extension != current_extension:
			if current_extension != "":
				cpp_data += create_function_load_declarations(current_indent_functions, False, True)
				current_indent_functions.clear()

				cpp_data += "#endif"
				cpp_data += "\n"

			current_extension = func_extension

			if current_extension != "":
				cpp_data += "\n#ifdef " + func_extension + "\n"

		if current_extension != "":
			current_indent_functions.append(func_name)
		
	if current_extension != "":
		cpp_data += create_function_load_declarations(current_indent_functions, False, current_extension != "")
		cpp_data += "#endif\n"

	cpp_data += end_gl_function_loader_inl

	return cpp_data


def save_file(contents, filename):
	with open(filename, "w", encoding="utf-8") as out_file:
	    out_file.write(contents)

if __name__ == "__main__":
	gl_spec_text          = open_ogl_spec("https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/master/xml/gl.xml") #Get it directly from the master branch
	gl_funcs, gl_versions = parse_ogl_funcs(gl_spec_text, "gl")

	function_list_data_h     = compile_ogl_function_list_h(gl_funcs, gl_versions)
	function_list_data_cpp   = compile_ogl_function_list_cpp(gl_funcs)
	function_loader_data_inl = compile_ogl_function_loader_inl(gl_funcs)

	save_file(function_list_data_h,     "OGLFunctions.hpp")
	save_file(function_list_data_cpp,   "OGLFunctions.cpp")
	save_file(function_loader_data_inl, "OGLFunctionLoader.inl")


	if not os.path.exists("Platform"):
		os.mkdir("Platform")


	wgl_spec_text = open_ogl_spec("https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/master/xml/wgl.xml") #Get it directly from the master branch
	wgl_funcs, wgl_versions = parse_ogl_funcs(wgl_spec_text, "wgl")

	wgl_function_list_data_h     = compile_wgl_function_list_h(wgl_funcs)
	wgl_function_list_data_cpp   = compile_wgl_function_list_cpp(wgl_funcs)
	wgl_function_loader_data_inl = compile_wgl_function_loader_inl(wgl_funcs)

	save_file(wgl_function_list_data_h,     "Platform/OGLFunctionsWin32.hpp")
	save_file(wgl_function_list_data_cpp,   "Platform/OGLFunctionsWin32.cpp")
	save_file(wgl_function_loader_data_inl, "Platform/OGLFunctionLoaderWin32.inl")