#pragma once

#include "wiimesh/Types.h"

namespace wiimesh {

class Ui {
public:
    void init();
    void updateInput(AppState &state);
    bool reloadLayout(AppState &state);
    bool applyLayoutLine(AppState &state, const std::string &line);
    bool saveLayout(AppState &state) const;
    std::string exportLayout() const;
    void draw(const AppState &state);
};

}
