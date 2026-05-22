#pragma once
#include "c_ui/workbenches/common/RibbonCommon.h"

// 辅助函数  根据按钮文本加载对应图标
namespace IconMaps07 {
    static const RibbonCommon::IconMapItem kGeoIconMap[] = {
        { QStringLiteral("自动"),  ":/geometry_icons/icons_other/geometry_icons/auto.PNG" },
        { QStringLiteral("点"),  ":/geometry_icons/icons_other/geometry_icons/point.PNG" },
        { QStringLiteral("线"), ":/geometry_icons/icons_other/geometry_icons/line.PNG" },
        { QStringLiteral("圆"),  ":/geometry_icons/icons_other/geometry_icons/circle.PNG" },
        { QStringLiteral("角圆"),  ":/geometry_icons/icons_other/geometry_icons/corner_circle.PNG" },
        { QStringLiteral("平面"),  ":/geometry_icons/icons_other/geometry_icons/plane.PNG" },
        { QStringLiteral("球体"),  ":/geometry_icons/icons_other/geometry_icons/sqhere.PNG" },
        { QStringLiteral("圆柱"),  ":/geometry_icons/icons_other/geometry_icons/cylinder.PNG" },
        { QStringLiteral("圆锥"),  ":/geometry_icons/icons_other/geometry_icons/cone.PNG" },
        { QStringLiteral("环面"),      ":/geometry_icons/icons_other/geometry_icons/torus.PNG" },
        { QStringLiteral("自由造型线"),        ":/geometry_icons/icons_other/geometry_icons/freedom_modeling_line.PNG" },
        { QStringLiteral("自由造型表面"),        ":/geometry_icons/icons_other/geometry_icons/freedom_modeling_surface.PNG" },
        { QStringLiteral("定义几何元素"),        ":/geometry_icons/icons_other/geometry_icons/define_geometry_element.png" },
        { QStringLiteral("显示拟合点"),":/geometry_icons/icons_other/geometry_icons/display_fit_point.PNG" },
        { QStringLiteral("隐藏拟合点"),":/geometry_icons/icons_other/geometry_icons/hide_fit_point.PNG" },
        { QStringLiteral("切换拟合点可见性"),  ":/geometry_icons/icons_other/geometry_icons/switch_fit_point_visibility.PNG" },
        { QStringLiteral("更新CM对象"),  ":/geometry_icons02/icons_other/geometry_icons/update_CM_obj.PNG" },
        { QStringLiteral("删除未使用的CM对象"),  ":/geometry_icons/icons_other/geometry_icons/delete_unused_CM_obj.PNG" },
        { QStringLiteral("替换几何元素"),  ":/geometry_icons/icons_other/geometry_icons/replace_geometry_element.PNG" },
        { QStringLiteral("相交"),  ":/geometry_icons/icons_other/geometry_icons/intersect.PNG" },
        { QStringLiteral("对称元素"),  ":/geometry_icons/icons_other/geometry_icons/symmetry_element.PNG" },
        { QStringLiteral("修改尺寸"),  ":/geometry_icons/icons_other/geometry_icons/modify_size.PNG" },
        { QStringLiteral("组合"),  ":/geometry_icons/icons_other/geometry_icons/combine.PNG" },
        { QStringLiteral("转换"),  ":/geometry_icons/icons_other/geometry_icons/transformation.PNG" },
        { QStringLiteral("规则元素"),  ":/geometry_icons/icons_other/geometry_icons/regularize_element.PNG" },
        { QStringLiteral("投影"),  ":/geometry_icons/icons_other/geometry_icons/projection.PNG" },
        { QStringLiteral("提取最小/最大点"),  ":/geometry_icons/icons_other/geometry_icons/extract_max_min_value.PNG" },
        { QStringLiteral("提取中轴线"),  ":/geometry_icons/icons_other/geometry_icons/extract_mid_axis.PNG" },
        { QStringLiteral("转化并导出"),  ":/geometry_icons02/icons_other/geometry_icons/transformation_and_export_pull_down_menu/save_fit_point_info.PNG" },
    };
};