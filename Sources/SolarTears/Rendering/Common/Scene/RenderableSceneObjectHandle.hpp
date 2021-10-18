#pragma once

#include <cstdint>

enum class SceneObjectType: uint8_t
{
	Static = 0,
	Rigid,

	Count,
	Undefined = Count
};

template<typename InternalIdType>
class RenderableSceneObjectHandleGeneral
{
	static constexpr int BitsPerObjectType  = 4;
	static constexpr int BitsPerBufferIndex = sizeof(InternalIdType) * CHAR_BIT - BitsPerObjectType;

	static constexpr int InternalObjectTypeOffset  = sizeof(InternalIdType) * CHAR_BIT - BitsPerObjectType;
	static constexpr int InternalBufferIndexOffset = sizeof(InternalIdType) * CHAR_BIT - BitsPerObjectType - BitsPerBufferIndex;

	static constexpr InternalIdType ObjectTypeMask  = (InternalIdType)(((1 << BitsPerObjectType)  - 1) << InternalObjectTypeOffset);
	static constexpr InternalIdType BufferIndexMask = (InternalIdType)(((1 << BitsPerBufferIndex) - 1) << InternalBufferIndexOffset);


	static_assert(std::numeric_limits<InternalIdType>::is_integer);
	static_assert((uint8_t)SceneObjectType::Count < (1 << BitsPerObjectType));

public:
	constexpr RenderableSceneObjectHandleGeneral(): RenderableSceneObjectHandleGeneral(UndefinedObjectBufferIndex(), SceneObjectType::Undefined)
	{
	}

	constexpr RenderableSceneObjectHandleGeneral(uint32_t objectBufferIndex, SceneObjectType objectType): Id(((InternalIdType)objectType << InternalObjectTypeOffset) | objectBufferIndex)
	{
		assert(objectBufferIndex < (1 << BitsPerBufferIndex));
	}

	constexpr bool IsValid() const
	{
		return ObjectType() != SceneObjectType::Undefined;
	}

	constexpr uint32_t ObjectBufferIndex() const
	{
		return (uint32_t)((Id & ObjectTypeMask) >> InternalBufferIndexOffset);
	}

	constexpr static uint32_t UndefinedObjectBufferIndex()
	{
		return 0xffffffff & BufferIndexMask;
	}

	constexpr SceneObjectType ObjectType() const
	{
		return (SceneObjectType)((Id & ObjectTypeMask) >> InternalObjectTypeOffset);
	}

	bool operator==(const RenderableSceneObjectHandleGeneral right) const { return Id == right.Id; }
	bool operator!=(const RenderableSceneObjectHandleGeneral right) const { return Id != right.Id; }
	bool operator< (const RenderableSceneObjectHandleGeneral right) const { return Id <  right.Id; }
	bool operator> (const RenderableSceneObjectHandleGeneral right) const { return Id >  right.Id; }

private:
	InternalIdType Id;
};