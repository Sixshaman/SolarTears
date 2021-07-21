#include "SceneDescriptionObject.hpp"
#include "MeshComponent.hpp"

static enum class SceneObjectFlags: uint64_t
{
	Static = 0x1, //The object never moves
};

SceneDescriptionObject::SceneDescriptionObject(uint64_t entityId): mEntityId(entityId)
{
}

SceneDescriptionObject::~SceneDescriptionObject()
{
}

SceneDescriptionObject::SceneDescriptionObject(SceneDescriptionObject&& right) noexcept
{
	*this = std::move(right);
}

SceneDescriptionObject& SceneDescriptionObject::operator=(SceneDescriptionObject&& right) noexcept
{
	mEntityId = right.mEntityId;

	mLocation = std::move(right.mLocation);

	mMeshComponent = std::move(right.mMeshComponent);

	return *this;
}

uint64_t SceneDescriptionObject::GetEntityId() const
{
	return mEntityId;
}

void SceneDescriptionObject::SetLocation(const SceneObjectLocation& location)
{
	mLocation = location;
}

SceneObjectLocation& SceneDescriptionObject::GetLocation()
{
	return mLocation;
}

const SceneObjectLocation& SceneDescriptionObject::GetLocation() const
{
	return mLocation;
}

void SceneDescriptionObject::SetMeshComponent(const SceneObjectMeshComponent& meshComponent)
{
	mMeshComponent = std::make_unique<SceneObjectMeshComponent>(meshComponent);
}

SceneObjectMeshComponent* SceneDescriptionObject::GetMeshComponent() const
{
	return mMeshComponent.get();
}

bool SceneDescriptionObject::IsStatic() const
{
	return mFlags & (uint64_t)SceneObjectFlags::Static;
}

void SceneDescriptionObject::SetStatic(bool isStatic)
{
	mFlags ^= (-(uint64_t)isStatic ^ mFlags) & (uint64_t)SceneObjectFlags::Static;
}