#pragma once

#include <imgui.h>

namespace sample::draw {

    bool BeginNode(ImVec2 const& size);
    void EndNode();

    void BeginNodeHeader();
    void EndNodeHeader();

}
