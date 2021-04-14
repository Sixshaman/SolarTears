#include "D3D12Shaders.hpp"
#include "D3D12Utils.hpp"
#include "../../Core/Util.hpp"
#include "../../Logging/Logger.hpp"
#include <cassert>
#include <array>
#include <wil/com.h>

#include "../../../3rd party/DirectXShaderCompiler/include/dxc/dxcapi.h"

D3D12::ShaderManager::ShaderManager(LoggerQueue* logger): mLogger(logger)
{
	LoadShaderData();
}

D3D12::ShaderManager::~ShaderManager()
{
}

void D3D12::ShaderManager::LoadShaderData()
{
	wil::com_ptr_nothrow<IDxcUtils> dxcUtils;
	THROW_IF_FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils.put())));
	
	//dxcUtils->LoadFile()
}
