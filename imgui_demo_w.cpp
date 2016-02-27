#ifdef __clang_
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wshadow"
#include "imgui/imgui_demo.cpp"
#pragma clang diagnostic pop
#else
#include "imgui/imgui_demo.cpp"
#endif
