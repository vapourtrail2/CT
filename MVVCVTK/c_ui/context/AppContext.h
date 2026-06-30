#pragma once
#include "c_ui/command/Commands.h"

// 应用中枢：被注入到各处的“唯一共享对象”。
// 现在先持有命令表；后续会持有 Dataset（打开的数据）/ State 句柄等，
// 让面板和选项卡只依赖“它一个”
class AppContext {
public:
	Commands& getCommands() {
		return commands_;
	}
	const Commands& getCommands() const{
		return commands_;
	}//什么意思

private:
	Commands commands_;
};