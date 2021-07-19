#pragma once

#include <string_view>

inline constexpr int CurrentYear()
{
	return ((__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 + (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0') * 1);
}

inline constexpr int CurrentMonth()
{
	return ((__DATE__[0] == 'J') ? ((__DATE__[1] == 'a') ? 1 : ((__DATE__[2] == 'n') ? 6 : 7)) : ((__DATE__[0] == 'F') ? 2 : ((__DATE__[0] == 'M') ? ((__DATE__[2] == 'r') ? 3 : 5) : ((__DATE__[0] == 'A') ? ((__DATE__[1] == 'p') ? 4 : 8) : ((__DATE__[0] == 'S') ? 9 : ((__DATE__[0] == 'O') ? 10 : ((__DATE__[0] == 'N') ? 11 : 12)))))));
}

inline constexpr int CurrentDay()
{
	return ((__DATE__[4] - '0') * 10 + (__DATE__[5] - '0') * 1);
}

inline constexpr int YearFromIso8601(const std::string_view date)
{
	return ((date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 + (date[3] - '0') * 1);
}

inline constexpr int MonthFromIso8601(const std::string_view date)
{
	return ((date[5] - '0') * 10 + (date[6] - '0') * 1);
}

inline constexpr int DayFromIso8601(const std::string_view date)
{
	return ((date[8] - '0') * 10 + (date[9] - '0') * 1);
}

inline constexpr bool IsYearInPast(const std::string_view date)
{
	return CurrentYear() > YearFromIso8601(date);
}

inline constexpr bool IsMonthInPast(const std::string_view date)
{
	return CurrentMonth() > MonthFromIso8601(date);
}

inline constexpr bool IsDayInPast(const std::string_view date)
{ 
	return CurrentDay() > DayFromIso8601(date);
}

inline constexpr bool IsYearInPresent(const std::string_view date)
{
	return CurrentYear() == YearFromIso8601(date);
}

inline constexpr bool IsMonthInPresent(const std::string_view date)
{
	return CurrentMonth() == MonthFromIso8601(date);
}

inline constexpr bool IsDayInPresent(const std::string_view date)
{
	return CurrentDay() == DayFromIso8601(date);
}

inline constexpr bool IsYearInFuture(const std::string_view date)
{
	return CurrentYear() < YearFromIso8601(date);
}

inline constexpr bool IsMonthInFuture(const std::string_view date)
{
	return CurrentMonth() < MonthFromIso8601(date);
}

inline constexpr bool IsDayInFuture(const std::string_view date)
{
	return CurrentDay() < DayFromIso8601(date);
}

inline constexpr bool IsDateInPast(const std::string_view date)
{
	return IsYearInPresent(date) ? (IsMonthInPresent(date) ? IsDayInPast(date) : IsMonthInPast(date)) : IsYearInPast(date);
}

inline constexpr bool IsDateInPresent(const std::string_view date)
{
	return IsYearInPresent(date) && IsMonthInPresent(date) && IsDayInPresent(date);
}

inline constexpr bool IsDateInFuture(const std::string_view date)
{
	return IsYearInPresent(date) ? (IsMonthInPresent(date) ? IsDayInFuture(date) : IsMonthInFuture(date)) : IsYearInFuture(date);
}