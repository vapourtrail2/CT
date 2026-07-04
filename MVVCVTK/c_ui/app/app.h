#pragma once
#include <memory>

namespace app {
    class App
    {
    public:
        App();
        int run(int argc, char** argv);
    private:
        void applyGlobalStyle() const;//配置
    };
} // namespace app