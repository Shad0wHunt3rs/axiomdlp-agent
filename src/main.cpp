// src/main.cpp

#include "core/application.h"

int main(int argc, char** argv) {
    auto opts = axiom::core::Application::parse_args(argc, argv);
    axiom::core::Application app(std::move(opts));
    return app.execute();
}