#pragma once

class Renderer;

class IInputProvider {
public:
    virtual ~IInputProvider() = default;

    virtual void processInput(Renderer& renderer) = 0;
};

