namespace
{
	template<typename T>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MAX_VALID;

	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<ID3D12RootSignature*> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_STREAM_OUTPUT_DESC> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_BLEND_DESC> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_RASTERIZER_DESC> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_DEPTH_STENCIL_DESC> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_INPUT_LAYOUT_DESC> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_INDEX_BUFFER_STRIP_CUT_VALUE> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_PRIMITIVE_TOPOLOGY_TYPE> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_RT_FORMAT_ARRAY> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<DXGI_SAMPLE_DESC> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_CACHED_PIPELINE_STATE> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_PIPELINE_STATE_FLAGS> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_DEPTH_STENCIL_DESC1> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1;
	
	template<>
	constexpr static D3D12_PIPELINE_STATE_SUBOBJECT_TYPE TypeOfSubobject<D3D12_VIEW_INSTANCING_DESC> = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING;
}

template<typename T>
inline void D3D12::D3D12Utils::StateSubobjectHelper::AddSubobjectGeneric(const T& subobject)
{
	AllocateStreamData(sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE) + sizeof(T));

	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE subobjectType = TypeOfSubobject<T>;
	assert(subobjectType != D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MAX_VALID);

	AddPadding();
	AddStreamData(&subobjectType);
	AddStreamData(&subobject);
}

template<typename T>
void D3D12::D3D12Utils::StateSubobjectHelper::AddStreamData(const T* data)
{
	size_t dataAlignment = alignof(T);
	size_t dataSize      = sizeof(T);

	size_t currentSize = AlignMemory(mStreamBlobSize, dataAlignment);
	assert(currentSize + dataSize <= mStreamBlobCapacity);

	memcpy(mSubobjectStreamBlob + currentSize, data, dataSize);
	mStreamBlobSize = currentSize + dataSize;
}