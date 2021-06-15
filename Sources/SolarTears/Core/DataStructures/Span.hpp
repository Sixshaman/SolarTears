#pragma once

//Type-erased span struct
//Unlike std::span, no information about in-container type is required
template<typename Size>
struct Span
{
	Size Begin;
	Size End;
};