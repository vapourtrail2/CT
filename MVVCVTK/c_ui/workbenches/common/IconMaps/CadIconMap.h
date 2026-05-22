#pragma once
#include "c_ui/workbenches/common/RibbonCommon.h"

// 辅助函数  根据按钮文本加载对应图标
namespace IconMaps09 {
    static const RibbonCommon::IconMapItem kCadIconMap[] = {
        { QStringLiteral("简化表面网格"),  ":/cad_icons/icons_other/CAD_surfacemesh_icons/simplify_surface_mesh.PNG" },
        { QStringLiteral("删除孤立的分量"),  ":/cad_icons/icons_other/CAD_surfacemesh_icons/delete_lonely_component.PNG" },
        { QStringLiteral("翻转表面方向"), ":/cad_icons/icons_other/CAD_surfacemesh_icons/reverse_surface_direction.PNG" },
        { QStringLiteral("重新计算CAD网格"),  ":/cad_icons/icons_other/CAD_surfacemesh_icons/re-cal_CAD_mesh.PNG" },
        { QStringLiteral("合并表面网格对象"),  ":/cad_icons/icons_other/CAD_surfacemesh_icons/merge_surface_mesh_obj.PNG" },
        { QStringLiteral("变形网格"),  ":/cad_icons/icons_other/CAD_surfacemesh_icons/deformation_mesh.PNG" },
        { QStringLiteral("模具修正"),  ":/cad_icons/icons_other/CAD_surfacemesh_icons/mold_modify.PNG" },
        { QStringLiteral("补偿网格"),  ":/cad_icons/icons_other/CAD_surfacemesh_icons/compensation_mesh.PNG" },
        { QStringLiteral("迭代补偿网格"),  ":/cad_icons/icons_other/CAD_surfacemesh_icons/lteration_compensation_mesh.PNG" },
        { QStringLiteral("变形场"),      ":/cad_icons/icons_other/CAD_surfacemesh_icons/deformation_field.PNG" },
    };
}; 

 