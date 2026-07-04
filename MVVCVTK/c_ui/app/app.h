#pragma once
#include <memory>

class App
{
    public:
        App();
        int run(int argc, char** argv);
    private:
        void applyGlobalStyle() const;//配置
 };
