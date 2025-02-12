//
// Created by admin on 2025/1/9.
//

#ifndef ARDA_CANVAS_H
#define ARDA_CANVAS_H

#include <memory>
#include <string>
#include <vector>
#include <include/core/SkPath.h>
#include <include/core/SkShader.h>
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkSurface.h"
#include "texture/Image.h"
#include "core/ObjectWrap.h"
#include "base/Data.h"

#define ToCanvasContext2D(jsobj) ObjectWrap::Unwrap<arda::Canvas>(jsobj)
#define ToCanvasGradient(jsobj) ObjectWrap::Unwrap<arda::CanvasGradient>(jsobj)
#define ToCanvasPattern(jsobj) ObjectWrap::Unwrap<arda::CanvasPattern>(jsobj)

namespace arda {

struct ImageData {
    uint8_t *data;
    uint32_t size;
    int width;
    int height;
};

ImageData createImageData(int width, int height);

class CanvasGradient : public ObjectWrap {
public:
    void addColorStop(float offset, uint32_t color);

    SkPoint start;
    SkPoint end;
    std::vector<SkColor> colors;
    std::vector<float> positions;
    sk_sp<SkShader> shader;
};

class CanvasPattern : public ObjectWrap {
public:
    sk_sp<SkImage> image;
    SkTileMode tmx;
    SkTileMode tmy;
    sk_sp<SkShader> shader;
};

class Canvas : public ObjectWrap {
public:
    struct FontProperties {
        std::string family{"sans-serif"};
        float size{16.0f};
        bool bold{false};
        bool italic{false};
        bool oblique{false};
        bool smallCaps{false};
    };
    //    /**
    //* @brief 线段连接样式
    //*/
    //    enum class LineJoin {
    //        Miter,  // 尖角连接
    //        Round,  // 圆角连接
    //        Bevel   // 斜角连接
    //    };
    //
    //
    //    enum class Direction {
    //        Inherit,  // 继承当前方向
    //        LTR,      // 从左到右
    //        RTL       // 从右到左
    //    };
    //    // 文本相关
    //    // 文本对齐方式
    //    enum class TextAlign { Left, Center, Right, End, Start };
    //
    //    // 文本基线
    //    enum class TextBaseline { Top, Hanging, Middle, Alphabetic, Ideographic, Bottom };

    Canvas(int width, int height);

    virtual ~Canvas();

    // 新增接口
    void setTextAlign(const std::string &align);

    const std::string &getTextAlign();

    void setTextBaseline(const std::string &baseline);

    const std::string &getTextBaseline();

    /**
     * @brief 在画布上绘制填充文本
     * @details 使用当前的字体、对齐方式和基线在指定位置绘制文本
     *
     * @param text 要绘制的文本
     * @param x 文本起始点的x坐标
     * @param y 文本起始点的y坐标
     * @param maxWidth 可选，文本的最大宽度。如果文本超过此宽度，将被压缩
     */
    void fillText(const std::string &text, float x, float y, float maxWidth = 0.0f);
    const std::string &getFont();
    void setFont(const std::string_view &fontVal);

    void setFont(const std::string &family, float size, bool bold = false, bool italic = false,
                 bool oblique = false, bool smallCaps = false);

    float measureText(const std::string &text);

    /**
     * @brief 在画布上绘制描边文本
     * @details 使用当前的字体、对齐方式和基线在指定位置绘制文本轮廓
     *
     * @param text 要绘制的文本
     * @param x 文本起始点的x坐标
     * @param y 文本起始点的y坐标
     * @param maxWidth 可选，文本的最大宽度。如果文本超过此宽度，将被压缩
     */
    void strokeText(const std::string &text, float x, float y, float maxWidth = 0.0f);

    // 矩形操作
    void fillRect(float x, float y, float width, float height);

    void clearRect(float x, float y, float width, float height);

    void strokeRect(float x, float y, float width, float height);

    // 路径操作
    void beginPath();

    void moveTo(float x, float y);

    void lineTo(float x, float y);

    void stroke();

    void fill();

    void closePath();

    // 样式设置
    void setFillStyle(uint32_t color);
    void setFillStyle(const CanvasGradient *gradient);
    void setFillStyle(const CanvasPattern *pattern);

    void setStrokeStyle(uint32_t color);

    void setLineWidth(float width);

    float getLineWidth();

    // 像素操作
    void getImageData(int sx, int sy, int sw, int sh, ImageData *output);

    void putImageData(const ImageData &imageData, int dx, int dy, int dirtyX, int dirtyY,
                      int dirtyWidth, int dirtyHeight);

    // 清除整个画布
    void clear(uint32_t color = SK_ColorTRANSPARENT);

    // 变换相关
    void setTransform(float a, float b, float c, float d, float e, float f);

    void transform(float a, float b, float c, float d, float e, float f);

    void translate(float x, float y);

    void setWidth(int width);

    void setHeight(int height);

    int getWidth() const {
        return _width;
    }

    int getHeight() const {
        return _height;
    }

    // 阴影相关方法
    void setShadowBlur(float blur);

    void setShadowColor(uint32_t color);

    void setShadowOffsetX(float offsetX);
    float getShadowOffsetX(){return _shadowOffsetX;}
    void setShadowOffsetY(float offsetY);
    float getShadowOffsetY(){return _shadowOffsetY;}

    void createLinearGradient(CanvasGradient *canvasGradient, float x0, float y0, float x1,
                              float y1);

    void createPattern(CanvasPattern *canvasPattern, const sk_sp<SkImage> &image,
                       const std::string &repetition);

    // 圆弧相关方法
    void arc(float x, float y, float radius, float startAngle, float endAngle,
             bool counterclockwise = false);

    void arcTo(float x1, float y1, float x2, float y2, float radius);

    // 贝塞尔曲线
    void bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);

    void setDirection(const std::string &direction);

    const std::string &getDirection();

    // 图像绘制
    void drawImage(const sk_sp<SkImage> &image, float dx, float dy);

    void drawImage(const sk_sp<SkImage> &image, float dx, float dy, float dw, float dh);

    void drawImage(const sk_sp<SkImage> &image, float sx, float sy, float sw, float sh, float dx,
                   float dy, float dw, float dh);

    void drawImage(const Image *image, float dx, float dy);

    void save();     // 保存当前状态
    void restore();  // 恢复之前的状态

    /**
     * @brief 缩放当前绘图
     * @details 在水平和垂直方向上缩放当前绘图。如果两个参数相同，则执行均匀缩放
     *
     * @param scaleX 水平缩放因子。1.0表示原始大小，>1放大，<1缩小
     * @param scaleY 垂直缩放因子。如果未指定，则使用scaleX值
     *
     * @note 缩放会影响之后的所有绘图操作，包括描边宽度
     * @note 缩放是累积的，多次调用会产生复合效果
     */
    void scale(float scaleX, float scaleY = 0.0f);

    /**
     * @brief 在当前路径中添加一个矩形
     * @details 创建一个矩形路径，但不会立即渲染。需要调用 stroke() 或 fill() 来渲染
     *
     * @param x 矩形左上角的 x 坐标
     * @param y 矩形左上角的 y 坐标
     * @param width 矩形的宽度
     * @param height 矩形的高度
     *
     * @note 如果当前路径已经存在，这个矩形会被添加到现有路径中
     * @note 这个方法不会清除之前的路径
     */
    void rect(float x, float y, float width, float height);

    /**
     * @brief 旋转当前绘图
     * @details 将当前绘图旋转指定的弧度。旋转中心点为画布的原点(0,0)
     *
     * @param angle 顺时针旋转的弧度。如果想使用角度，需要先转换为弧度：degrees * Math.PI / 180
     *
     * @note 旋转是累积的，多次调用会产生复合效果
     * @note 旋转会影响之后的所有绘图操作
     * @note 通常配合 translate() 使用来改变旋转中心点
     */
    void rotate(float angle);

    /**
     * @brief 设置线段连接样式
     * @param join 连接样式
     */
    void setLineJoin(const std::string &join);

    /**
     * @brief 获取当前线段连接样式
     * @return LineJoin 当前的连接样式
     */
    const std::string &getLineJoin();

    /**
     * @brief 重置画布状态到默认值
     * @details 重置所有状态，包括变换矩阵、样式设置、阴影等
     */
    void reset();

    bool getData(Data * output);

    /**
     * @brief 加载TTF字体文件
     * @param fontPath 字体文件路径
     * @param fontFamily 字体族名称，用于后续引用该字体
     * @return bool 是否加载成功
     */
    static bool loadFontFile(const std::string& fontPath, const std::string& fontFamily);
private:
    /**
     * @brief 内部文本渲染函数
     * @param text 要渲染的文本
     * @param x 文本x坐标
     * @param y 文本y坐标
     * @param maxWidth 最大宽度
     * @param paint 使用的画笔(fill或stroke)
     */
    void drawText(const std::string &text, float x, float y, float maxWidth, SkPaint &paint);

    // 辅助函数：计算文本位置偏移
    void calculateTextPosition(float textWidth, float x, float y, SkPoint &size);

    // 确保surface是最新的
    void ensureValidSurface();

    // 辅助函数：更新画笔的阴影效果
    void updateShadowEffect(SkPaint &paint) const;

    // 辅助函数：根据字体属性创建字体
    void updateFont();

private:
    int uuid;
    sk_sp<SkSurface> _surface;
    SkCanvas *_canvas;
    SkFont _font;
    SkPaint _fillPaint;
    SkPaint _strokePaint;
    SkPath _currentPath;
    int _width;
    int _height;
    std::string _textAlign;
    std::string _textBaseline;
    std::string _direction;
    std::string _lineJoin;  // 默认使用尖角连接
    std::string _fontStr;
    bool _needRecreate{false};  // 标记是否需要重新创建surface

    // 阴影属性
    float _shadowBlur{0.0f};
    uint32_t _shadowColor{SK_ColorTRANSPARENT};
    float _shadowOffsetX{0.0f};
    float _shadowOffsetY{0.0f};
    FontProperties _fontProps;
    SkPoint tempPoint;
    // 保存画布状态的结构体
    struct CanvasState {
        SkPaint fillPaint;
        SkPaint strokePaint;
        SkFont font;
        FontProperties fontProps;
        float shadowBlur;
        uint32_t shadowColor;
        float shadowOffsetX;
        float shadowOffsetY;
        std::string textAlign;
        std::string textBaseline;
        std::string direction;
        std::string lineJoin;
    };
    std::vector<CanvasState> _stateStack;  // 状态栈

    // 存储已加载的字体
    static std::unordered_map<std::string, sk_sp<SkTypeface>> _ttfCaches;
};

}  // namespace arda

#endif  // ARDA_CANVAS_H
