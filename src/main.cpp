#include "hephaiston/Application.h"

#include <exception>
#include <iostream>

int main() {
    try {
        hephaiston::Application app;
        if (!app.initialize()) {
            return 1;
        }
        app.run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
}
