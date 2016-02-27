#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#include "imgui/imgui.cpp"
#pragma clang diagnostic pop
#else
#pragma warning(push)
#pragma warning(disable: 4365)
#pragma warning(disable: 4668)
#include "imgui/imgui.cpp"
#pragma warning(pop)
#endif
