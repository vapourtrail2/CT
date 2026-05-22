#pragma once
#include "c_ui/workbenches/common/RibbonCommon.h"

namespace IconMaps02{
    static const RibbonCommon::IconMapItem kEditIconMap[] = {
        { QStringLiteral("撤销"), ":/icons/icons/undo.png" },
        { QStringLiteral("重做"), ":/icons/icons/redo.png" },
        { QStringLiteral("释放内存/清除撤销队列"), ":/icons/icons/free_memory.png" },
        { QStringLiteral("剪切"), ":/icons/icons/cut.png" },
        { QStringLiteral("复制"), ":/icons/icons/copy.png" },
        { QStringLiteral("粘贴"), ":/icons/icons/paste.png" },
        { QStringLiteral("删除"), ":/icons/icons/delete.png" },
        { QStringLiteral("创建对象组"), ":/icons/icons/create_obj_group.png" },
        { QStringLiteral("取消对象组"), ":/icons/icons/cancel_obj_group.png" },
        { QStringLiteral("转换为"), ":/icons/icons/trans_pull_down_menu/trans.png" },
        { QStringLiteral("属性"), ":/icons/icons/property.png" },
        { QStringLiteral("旋转"), ":/icons/icons/spin.png" },
        { QStringLiteral("移动"), ":/icons/icons/move.png" },
        { QStringLiteral("复制可视状态"), ":/icons/icons/copy_visible_status.png" },
        { QStringLiteral("粘贴可视状态"), ":/icons/icons/paste_visible_status.png" },
        { QStringLiteral("复制元信息"), ":/icons/icons/copy_meta.png" },
        { QStringLiteral("粘贴元信息"), ":/icons/icons/paste_meta.png" },
        { QStringLiteral("动态重命名"), ":/icons/icons/dynamic_rename.png" },
    };
}
