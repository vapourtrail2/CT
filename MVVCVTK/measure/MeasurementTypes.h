#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace measure {

using Point3 = std::array<double, 3>; //例如Point p1{ 1.1 ,2.2 , 3.2}

enum class MeasureTool {
    None,
    Line,
    Circle3Point,
    Arc3Point
};

enum class MeasureView {
    Axial,
    Coronal,
    Sagittal
};

struct MeasureRequest {
    MeasureTool tool = MeasureTool::Line;
    MeasureView view = MeasureView::Axial;
};//表示一次测量请求

struct MeasurementPlane {//虽然是在二维视图里测量，但程序内部仍然使用三维坐标，所以必须记录这张二维平面在三维空间中的位置和方向。
    Point3 origin{ 0.0, 0.0, 0.0 };//Z切片的一个点
	Point3 normal{ 0.0, 0.0, 1.0 };//法向量
	Point3 u{ 1.0, 0.0, 0.0 };//平面上的第一个方向
	Point3 v{ 0.0, 1.0, 0.0 };//平面上的第二个方向
	double tolerance = 1e-6;
    double sliceTolerance = 0.5;
};  
//两个作用 鼠标射线和二维切片求交，得到点击点 二是把三维物理点投影到平面的 u/v 二维坐标中，用于计算圆和圆弧。

struct LineResult {
    double length = 0.0;
};

struct CircleResult {
    Point3 center{ 0.0, 0.0, 0.0 };
	double radius = 0.0;//半径
	double diameter = 0.0;//直径
	double circumference = 0.0;//周长
};

struct ArcResult {
    Point3 center{ 0.0, 0.0, 0.0 };
    double radius = 0.0;
	double radiusAngle = 0.0;
    double length = 0.0;
};

using MeasurementResult = std::variant<LineResult, CircleResult, ArcResult>;//保存三个测量结果其中一种

struct MeasurementEntity {
    std::uint64_t id = 0;//测量编号
    MeasureTool type = MeasureTool::None;
    MeasureView where2DViewer = MeasureView::Axial;
    MeasurementPlane plane; 
    std::vector<Point3> physicalPoints;
    MeasurementResult result = LineResult{};
};

struct MeasurementDraft {
    MeasureRequest request;// 当前要画什么：线/圆/圆弧，以及在哪个2D视图
    std::optional<MeasurementPlane> plane;// 第一次点击后记录的切片平面；还没点击时为空
    std::vector<Point3> physicalPoints;// 正式点击的点  p1 p2
	std::optional<Point3> previewPoint;// 鼠标正在移动的位置 ，仅预览
};

}
