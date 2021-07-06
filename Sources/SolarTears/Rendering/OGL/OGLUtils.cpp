#include "OGLUtils.hpp"
#include "../../Core/Util.hpp"
#include <format>

void OpenGL::OGLUtils::GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    UNREFERENCED_PARAMETER(length);
	UNREFERENCED_PARAMETER(userParam);

    Utils::SystemDebugMessage("[OPENGL DEBUG] ");
    switch(source)
    {
    case GL_DEBUG_SOURCE_API:
        Utils::SystemDebugMessage("OpenGL");
        break;

    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        Utils::SystemDebugMessage("Window system");
        break;

    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        Utils::SystemDebugMessage("Shader compiler");
        break;

    case GL_DEBUG_SOURCE_THIRD_PARTY:
        Utils::SystemDebugMessage("Third-party function");
        break;

    case GL_DEBUG_SOURCE_APPLICATION:
        Utils::SystemDebugMessage("The application");
        break;

    case GL_DEBUG_SOURCE_OTHER:
        Utils::SystemDebugMessage("An unknown source");
        break;

    default:
        break;
    }

    Utils::SystemDebugMessage(" has reported");
    switch(type)
    {
    case GL_DEBUG_TYPE_ERROR:
        Utils::SystemDebugMessage(" an error");
        break;

    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        Utils::SystemDebugMessage(" a deprecated behavior");
        break;

    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        Utils::SystemDebugMessage(" an undefined behavior");
        break;

    case GL_DEBUG_TYPE_PORTABILITY:
        Utils::SystemDebugMessage(" a portability issue");
        break;

    case GL_DEBUG_TYPE_PERFORMANCE:
        Utils::SystemDebugMessage(" a performance problem");
        break;

    case GL_DEBUG_TYPE_MARKER:
        Utils::SystemDebugMessage(" a marker");
        break;

    case GL_DEBUG_TYPE_PUSH_GROUP:
        Utils::SystemDebugMessage(" a push group start");
        break;

    case GL_DEBUG_TYPE_POP_GROUP:
        Utils::SystemDebugMessage(" a push group end");
        break;

    case GL_DEBUG_TYPE_OTHER:
        Utils::SystemDebugMessage(" an unknown message");
        break;

    default:
        break;
    }

    switch(severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        Utils::SystemDebugMessage(" of high severity: ");
        break;

    case GL_DEBUG_SEVERITY_MEDIUM:
        Utils::SystemDebugMessage(" of medium severity: ");
        break;

    case GL_DEBUG_SEVERITY_LOW:
        Utils::SystemDebugMessage(" of low severity: ");
        break;

    case GL_DEBUG_SEVERITY_NOTIFICATION:
        Utils::SystemDebugMessage(" of verbose severity: ");
        break;

    default:
        break;
    }

    Utils::SystemDebugMessage(message);
    Utils::SystemDebugMessage(std::format(". Id: {} ({:#04x})", id));

    Utils::SystemDebugMessage("\n");
}