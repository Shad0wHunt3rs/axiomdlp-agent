// src/main.cpp

#include "foundation/application.h"

int main(int argc, char** argv) {
    axiom::core::Application::Options opts = axiom::core::Application::parse_args(argc, argv);
    axiom::core::Application app(std::move(opts));
    return app.execute();
}