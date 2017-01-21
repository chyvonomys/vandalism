#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#pragma clang diagnostic ignored "-Wformat-pedantic"
#include "imgui/imgui.cpp"
#pragma clang diagnostic pop
#else
#pragma warning(push)
#pragma warning(disable: 4365)
#pragma warning(disable: 4668)
#pragma warning(disable: 4774)
#pragma warning(disable: 4456)
#include "imgui/imgui.cpp"
#pragma warning(pop)
#endif
