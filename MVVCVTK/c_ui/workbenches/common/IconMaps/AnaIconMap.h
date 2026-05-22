#pragma once
#include "c_ui/workbenches/common/RibbonCommon.h"

// 辅助函数  根据按钮文本加载对应图标
namespace IconMaps10 {
    static const RibbonCommon::IconMapItem kAnaIconMap[] = {
        { QStringLiteral("注解"),  ":/analyisis_icons/icons_other/analysis_icons/annotation.PNG" },
        { QStringLiteral("实时值"),  ":/analyisis_icons/icons_other/analysis_icons/realtime_value_on.PNG" },
        { QStringLiteral("孔隙/夹杂物"), ":/analysis_icons02/icons_other/analysis_icons/Pores_and_inclusions_pull_down_menu/a.PNG" },
        { QStringLiteral("P203"),  ":/analyisis_icons/icons_other/analysis_icons/P203.PNG" },
        { QStringLiteral("P202/P201"),  ":/analyisis_icons/icons_other/analysis_icons/P202_P201.PNG" },
        { QStringLiteral("设计件/实物对比"),  ":/analyisis_icons/icons_other/analysis_icons/design_to_actual_compare.PNG" },
        { QStringLiteral("壁厚"),  ":/analyisis_icons/icons_other/analysis_icons/wall_thickness.PNG" },
        { QStringLiteral("位移"),  ":/analyisis_icons/icons_other/analysis_icons/displacement.PNG" },
        { QStringLiteral("纤维复合材料"),  ":/analyisis_icons/icons_other/analysis_icons/fiber_recombination_material.PNG" },
        { QStringLiteral("泡状/粉末结构"),      ":/analyisis_icons/icons_other/analysis_icons/paozhuang_fenmu_struct.PNG" },
        { QStringLiteral("数字体积相关计算"),        ":/analyisis_icons/icons_other/analysis_icons/data_volume_cal.PNG" },
        { QStringLiteral("灰度值"),        ":/analyisis_icons/icons_other/analysis_icons/grey_value.PNG" },
        { QStringLiteral("数据质量"),        ":/analyisis_icons/icons_other/analysis_icons/data_quality.PNG" },
        { QStringLiteral("切片图面积"),":/analyisis_icons/icons_other/analysis_icons/slice_area.PNG" },
        { QStringLiteral("OCR"),":/analyisis_icons/icons_other/analysis_icons/OCR.PNG" },
        { QStringLiteral("夹紧模拟"),  ":/analyisis_icons/icons_other/analysis_icons/clip_simulation.PNG" },
        { QStringLiteral("结构力学模拟"),  ":/analyisis_icons/icons_other/analysis_icons/struct_simulation.PNG" },
        { QStringLiteral("传递现象"),  ":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/transfer_phenomenon.PNG" },
        { QStringLiteral("电池极片对齐分析"),  ":/analyisis_icons/icons_other/analysis_icons/battery_analysis.PNG" },
        { QStringLiteral("导入集成网格"),  ":/analyisis_icons/icons_other/analysis_icons/input_integration_mesh.PNG" },
        { QStringLiteral("创建集成网格"),  ":/analysis_icons02/icons_other/analysis_icons/create_integration_mesh_pull_down_menu/create_integration_mesh.PNG" },
        { QStringLiteral("评估"),  ":/analysis_icons02/icons_other/analysis_icons/evaluate_pull_down_menu/evaluate.PNG" },
        { QStringLiteral("更新所有分析"),  ":/analyisis_icons/icons_other/analysis_icons/update_analysis.PNG" },
    };
};