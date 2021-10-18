#pragma once

//Type-erased span struct
//Unlike std::span, no information about in-container type is required
template<typename Size>
struct Span
{
	Size Begin;
	Size End;
};

//The span that holds additional type info
template<typename Size, typename TypeInfo>
struct TypedSpan
{
	Size     Begin;
	Size     End;
	TypeInfo Type;
};