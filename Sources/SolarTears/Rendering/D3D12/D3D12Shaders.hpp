#pragma once

#include <d3d12.h>
#include <unordered_map>
#include <memory>

class LoggerQueue;

namespace D3D12
{
	class ShaderManager
	{
	public:
		ShaderManager(LoggerQueue* logger);
		~ShaderManager();

	private:
		void LoadShaderData();

	private:
		LoggerQueue* mLogger;
	};
}