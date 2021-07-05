#pragma once

#ifdef WIN32
#include <Windows.h>
#endif

#include <GL/GL.h>
#include <GL/glext.h>
#include <cstdint>

#define VoidPtrOffset(offset) ((void*)((ptrdiff_t)(offset)))

namespace OpenGL
{
	namespace OGLUtils
	{
		template<typename T>
		constexpr GLenum FormatForIndexType = GL_UNSIGNED_INT;

		template<>
		constexpr GLenum FormatForIndexType<uint32_t> = GL_UNSIGNED_INT;

		template<>
		constexpr GLenum FormatForIndexType<uint16_t> = GL_UNSIGNED_SHORT;

		template<>
		constexpr GLenum FormatForIndexType<uint8_t> = GL_UNSIGNED_BYTE;


		void GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
	}
}