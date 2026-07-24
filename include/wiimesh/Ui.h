#pragma once

#include "wiimesh/Types.h"

namespace wiimesh {

class Ui {
public:
    void init();
    void updateInput(AppState &state);
    void draw(const AppState &state);
};

}
