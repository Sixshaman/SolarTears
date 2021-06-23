#include "D3D12DeviceFeatures.hpp"
#include <wil/com.h>
#include <array>

template<typename T>
static constexpr D3D12_FEATURE FeatureForType;

#define DECLARE_FEATURE(FEATURE_TYPE) template<> constexpr D3D12_FEATURE FeatureForType<D3D12_FEATURE_DATA_ ## FEATURE_TYPE> = D3D12_FEATURE_ ## FEATURE_TYPE;

DECLARE_FEATURE(D3D12_OPTIONS)
DECLARE_FEATURE(D3D12_OPTIONS1)
DECLARE_FEATURE(D3D12_OPTIONS2)
DECLARE_FEATURE(D3D12_OPTIONS3)
DECLARE_FEATURE(D3D12_OPTIONS4)
DECLARE_FEATURE(D3D12_OPTIONS5)
DECLARE_FEATURE(D3D12_OPTIONS6)
DECLARE_FEATURE(D3D12_OPTIONS7)

DECLARE_FEATURE(ARCHITECTURE1)
DECLARE_FEATURE(FEATURE_LEVELS)

DECLARE_FEATURE(FORMAT_SUPPORT)
DECLARE_FEATURE(FORMAT_INFO)
DECLARE_FEATURE(MULTISAMPLE_QUALITY_LEVELS)
DECLARE_FEATURE(GPU_VIRTUAL_ADDRESS_SUPPORT)
DECLARE_FEATURE(SHADER_MODEL)
DECLARE_FEATURE(ROOT_SIGNATURE)
DECLARE_FEATURE(SHADER_CACHE)
DECLARE_FEATURE(EXISTING_HEAPS)
DECLARE_FEATURE(CROSS_NODE)

template<typename T>
void GetFeature(ID3D12Device* device, T* featureData)
{
	THROW_IF_FAILED(device->CheckFeatureSupport(FeatureForType<T>, featureData, sizeof(T)));
}

D3D12::DeviceFeatures::DeviceFeatures(ID3D12Device* device)
{
	InitAllFeatures(device);
}

D3D12::DeviceFeatures::~DeviceFeatures()
{
}

const D3D12_FEATURE_DATA_D3D12_OPTIONS& D3D12::DeviceFeatures::GetOptions() const
{
	return mOptions;
}

const D3D12_FEATURE_DATA_D3D12_OPTIONS1& D3D12::DeviceFeatures::GetOptions1() const
{
	return mOptions1;
}

const D3D12_FEATURE_DATA_D3D12_OPTIONS2& D3D12::DeviceFeatures::GetOptions2() const
{
	return mOptions2;
}

const D3D12_FEATURE_DATA_D3D12_OPTIONS3& D3D12::DeviceFeatures::GetOptions3() const
{
	return mOptions3;
}

const D3D12_FEATURE_DATA_D3D12_OPTIONS4& D3D12::DeviceFeatures::GetOptions4() const
{
	return mOptions4;
}

const D3D12_FEATURE_DATA_D3D12_OPTIONS5& D3D12::DeviceFeatures::GetOptions5() const
{
	return mOptions5;
}

const D3D12_FEATURE_DATA_D3D12_OPTIONS6& D3D12::DeviceFeatures::GetOptions6() const
{
	return mOptions6;
}

const D3D12_FEATURE_DATA_D3D12_OPTIONS7& D3D12::DeviceFeatures::GetOptions7() const
{
	return mOptions7;
}

const D3D12_FEATURE_DATA_ARCHITECTURE1& D3D12::DeviceFeatures::GetArchitecture1Features() const
{
	return mArchitectureFeatures;
}

const D3D12_FEATURE_DATA_FEATURE_LEVELS& D3D12::DeviceFeatures::GetFeatureLevelsFeatures() const
{
	return mFeatureLevelsFeatures;
}

const D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT& D3D12::DeviceFeatures::GetVirtualAddressFeatures() const
{
	return mVirtualAddressFeatures;
}

const D3D12_FEATURE_DATA_SHADER_MODEL& D3D12::DeviceFeatures::GetShaderModelFeatures() const
{
	return mShaderModelFeatures;
}

const D3D12_FEATURE_DATA_ROOT_SIGNATURE& D3D12::DeviceFeatures::GetRootSignatureFeatures() const
{
	return mRootSignatureFeatures;
}

const D3D12_FEATURE_DATA_SHADER_CACHE& D3D12::DeviceFeatures::GetShaderCacheFeatures() const
{
	return mShaderCacheFeatures;
}

const D3D12_FEATURE_DATA_EXISTING_HEAPS& D3D12::DeviceFeatures::GetExistingHeapsFeatures() const
{
	return mExistingHeapsFeatures;
}

const D3D12_FEATURE_DATA_CROSS_NODE& D3D12::DeviceFeatures::GetCrossNodeFeatures() const
{
	return mCrossNodeFeatures;
}

D3D12_FEATURE_DATA_FORMAT_SUPPORT D3D12::DeviceFeatures::GetFormatSupportFeatures(ID3D12Device* device, DXGI_FORMAT format) const
{
	D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport;
	formatSupport.Format = format;

	GetFeature(device, &formatSupport);
	return formatSupport;
}

D3D12_FEATURE_DATA_FORMAT_INFO D3D12::DeviceFeatures::GetFormatInfoFeatures(ID3D12Device* device, DXGI_FORMAT format) const
{
	D3D12_FEATURE_DATA_FORMAT_INFO formatInfo;
	formatInfo.Format = format;

	GetFeature(device, &formatInfo);
	return formatInfo;
}

D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS D3D12::DeviceFeatures::GetMultisampleFeatures(ID3D12Device* device, DXGI_FORMAT format, UINT sampleCount) const
{
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS multisampleInfo;
	multisampleInfo.Format      = format;
	multisampleInfo.SampleCount = sampleCount;
	multisampleInfo.Flags       = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

	GetFeature(device, &multisampleInfo);
	return multisampleInfo;
}

void D3D12::DeviceFeatures::InitAllFeatures(ID3D12Device* device)
{
	GetFeature(device, &mOptions);
	GetFeature(device, &mOptions1);
	GetFeature(device, &mOptions2);
	GetFeature(device, &mOptions3);
	GetFeature(device, &mOptions4);
	GetFeature(device, &mOptions5);
	GetFeature(device, &mOptions6);
	GetFeature(device, &mOptions7);

	//TODO: mGPU?
	mArchitectureFeatures.NodeIndex = 0;
	GetFeature(device, &mArchitectureFeatures);

	std::array featureLevels = {D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1};
	mFeatureLevelsFeatures.pFeatureLevelsRequested = featureLevels.data();
	mFeatureLevelsFeatures.NumFeatureLevels        = (UINT)featureLevels.size();
	GetFeature(device, &mFeatureLevelsFeatures);

	mShaderModelFeatures.HighestShaderModel = D3D_SHADER_MODEL_6_6;
	GetFeature(device, &mShaderModelFeatures);

	mRootSignatureFeatures.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	GetFeature(device, &mRootSignatureFeatures);

	GetFeature(device, &mVirtualAddressFeatures);
	GetFeature(device, &mShaderCacheFeatures);
	GetFeature(device, &mExistingHeapsFeatures);
	GetFeature(device, &mCrossNodeFeatures);
}