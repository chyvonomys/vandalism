#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#include "imgui/imgui.cpp"
#pragma clang diagnostic pop
#else
#include "imgui/imgui.cpp"
#endif
