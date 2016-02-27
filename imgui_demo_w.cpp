#ifdef __clang_
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wshadow"
#include "imgui/imgui_demo.cpp"
#pragma clang diagnostic pop
#else
#pragma warning(push)
#pragma warning(disable: 4365)
#pragma warning(disable: 4640)
#include "imgui/imgui_demo.cpp"
#pragma warning(pop)
#endif
