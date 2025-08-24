#include "app_state.h"

AppState& GetAppState() {
    static AppState state;
    return state;
}
