#pragma once

#include <d3d12.h>

namespace D3D12
{
	class DeviceFeatures
	{
	public:
		DeviceFeatures(ID3D12Device* device);
		~DeviceFeatures();

		const D3D12_FEATURE_DATA_D3D12_OPTIONS&  GetOptions()  const;
		const D3D12_FEATURE_DATA_D3D12_OPTIONS1& GetOptions1() const;
		const D3D12_FEATURE_DATA_D3D12_OPTIONS2& GetOptions2() const;
		const D3D12_FEATURE_DATA_D3D12_OPTIONS3& GetOptions3() const;
		const D3D12_FEATURE_DATA_D3D12_OPTIONS4& GetOptions4() const;
		const D3D12_FEATURE_DATA_D3D12_OPTIONS5& GetOptions5() const;
		const D3D12_FEATURE_DATA_D3D12_OPTIONS6& GetOptions6() const;
		const D3D12_FEATURE_DATA_D3D12_OPTIONS7& GetOptions7() const;

		const D3D12_FEATURE_DATA_ARCHITECTURE1& GetArchitecture1Features() const;

		const D3D12_FEATURE_DATA_FEATURE_LEVELS& GetFeatureLevelsFeatures() const;

		const D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT& GetVirtualAddressFeatures() const;
		const D3D12_FEATURE_DATA_SHADER_MODEL&                GetShaderModelFeatures()    const;
		const D3D12_FEATURE_DATA_ROOT_SIGNATURE&              GetRootSignatureFeatures()  const;
		const D3D12_FEATURE_DATA_SHADER_CACHE&                GetShaderCacheFeatures()    const;
		const D3D12_FEATURE_DATA_EXISTING_HEAPS&              GetExistingHeapsFeatures()  const;
		const D3D12_FEATURE_DATA_CROSS_NODE&                  GetCrossNodeFeatures()      const;

		D3D12_FEATURE_DATA_FORMAT_SUPPORT              GetFormatSupportFeatures(ID3D12Device* device, DXGI_FORMAT format)  const;
		D3D12_FEATURE_DATA_FORMAT_INFO                 GetFormatInfoFeatures(ID3D12Device* device,    DXGI_FORMAT format)  const;

		D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS GetMultisampleFeatures(ID3D12Device* device, DXGI_FORMAT format, UINT sampleCount) const;

	private:
		void InitAllFeatures(ID3D12Device* device);

	private:
		D3D12_FEATURE_DATA_D3D12_OPTIONS  mOptions;
		D3D12_FEATURE_DATA_D3D12_OPTIONS1 mOptions1;
		D3D12_FEATURE_DATA_D3D12_OPTIONS2 mOptions2;
		D3D12_FEATURE_DATA_D3D12_OPTIONS3 mOptions3;
		D3D12_FEATURE_DATA_D3D12_OPTIONS4 mOptions4;
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 mOptions5;
		D3D12_FEATURE_DATA_D3D12_OPTIONS6 mOptions6;
		D3D12_FEATURE_DATA_D3D12_OPTIONS7 mOptions7;

		D3D12_FEATURE_DATA_ARCHITECTURE1  mArchitectureFeatures;

		D3D12_FEATURE_DATA_FEATURE_LEVELS mFeatureLevelsFeatures;

		D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT mVirtualAddressFeatures;
		D3D12_FEATURE_DATA_SHADER_MODEL                mShaderModelFeatures;
		D3D12_FEATURE_DATA_ROOT_SIGNATURE              mRootSignatureFeatures;
		D3D12_FEATURE_DATA_SHADER_CACHE                mShaderCacheFeatures;
		D3D12_FEATURE_DATA_EXISTING_HEAPS              mExistingHeapsFeatures;
		D3D12_FEATURE_DATA_CROSS_NODE                  mCrossNodeFeatures;
	};
}