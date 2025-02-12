//
// Created by admin on 2025/1/9.
//

#include "Canvas.h"
#include <regex>
#include "Engine.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkStream.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#include "include/effects/SkGradientShader.h"
#include "include/effects/SkImageFilters.h"
#include "include/ports/SkFontMgr_empty.h"  // 添加这个头文件

#if (OS_PLATFORM == OS_PLATFORM_ANDROID)

#include "include/ports/SkFontMgr_android.h"

#elif (OS_PLATFORM == OS_PLATFORM_IOS)
#include "include/ports/SkFontMgr_mac_ct.h"
#elif (OS_PLATFORM == OS_PLATFORM_WINDOWS)
#include "include/ports/SkFontMgr_win.h"
#endif

namespace arda {

    namespace {
        thread_local sk_sp<SkFontMgr> tls_fontMgr = nullptr;

        sk_sp<SkFontMgr> &GetFontMgr() {
            if (!tls_fontMgr) {
                // 使用系统默认字体管理器
#if (OS_PLATFORM == OS_PLATFORM_ANDROID)
                tls_fontMgr = SkFontMgr_New_Android(nullptr);
#elif (OS_PLATFORM == OS_PLATFORM_IOS)
                tls_fontMgr = SkFontMgr_New_CoreText(nullptr);
#elif (OS_PLATFORM == OS_PLATFORM_WINDOWS)
                tls_fontMgr = SkFontMgr_New_DirectWrite(nullptr);
#else
                tls_fontMgr = SkFontMgr_New_Custom_Empty();
#endif
            }
            return tls_fontMgr;
        }
    }  // namespace

    ImageData createImageData(int width, int height) {
        ImageData result;
        result.width = width;
        result.height = height;
        result.size = width * height * 4;
        result.data = (unsigned char *) malloc(sizeof(unsigned char) * result.size);
        return result;  // 移动语义，避免拷贝
    }

    void CanvasGradient::addColorStop(float offset, uint32_t color) {
        // 确保 offset 在 0-1 范围内
        offset = std::max(0.0f, std::min(1.0f, offset));

        // 插入排序，保持位置有序
        auto it = positions.begin();
        auto colorIt = colors.begin();
        while (it != positions.end() && *it < offset) {
            ++it;
            ++colorIt;
        }

        positions.insert(it, offset);
        colors.insert(colorIt, color);

        // 重新创建 shader
        if (!positions.empty()) {
            const SkPoint pts[2] = {start, end};
            shader = SkGradientShader::MakeLinear(pts, colors.data(), positions.data(),
                                                  colors.size(),
                                                  SkTileMode::kClamp);
        }
    }

// 静态成员初始化
    std::unordered_map<std::string, sk_sp<SkTypeface>> Canvas::_ttfCaches;

    bool Canvas::loadFontFile(const std::string &fontPath, const std::string &fontFamily) {
        //    ARDA_LOG_DEBUG("[Canvas] loadTTF %s %s",fontFamily.c_str(), fontPath.c_str());
        // 检查字体是否已加载
        if (_ttfCaches.find(fontFamily) != _ttfCaches.end()) {
            return true;
        }
        int errcode = 0;
        do {
            Data fileData;
            // 读取ttf数据
            if (!FS->getContents(fontPath, &fileData)) {
                errcode = 1;
                break;
            }
            // 从文件加载字体
            sk_sp<SkData> fontData = SkData::MakeFromMalloc(fileData.getBytes(),
                                                            fileData.getSize());
            if (!fontData) {
                errcode = 2;
                break;
            }
            // 根据加载的ttf 创建字体
            sk_sp<SkTypeface> typeface = GetFontMgr()->makeFromData(fontData, 0);
            if (!typeface) {
                errcode = 3;
                break;
            }
            typeface->ref();
            ARDA_LOG_DEBUG("loadFontFile %s", fontFamily.c_str());
            int countTables = typeface->countTables();
            int countGlyphs = typeface->countGlyphs();
            int unitsPerEm = typeface->getUnitsPerEm();
#if (ARDA_DEBUG == 1)
            ARDA_LOG_DEBUG(" UnitsPerEm: %d", unitsPerEm);
            if (countTables == 0 || countGlyphs == 0) {
                // Typeface命名，直接从ttf中读取
                SkString ttfName;
                typeface->getFamilyName(&ttfName);
                ARDA_LOG_ERROR(
                        "loadFontFile %s typeface(%s) error countTables == 0 || countGlyphs == 0",
                        fontFamily.c_str(), ttfName.c_str());
            }
#endif
            // 存储字体
            _ttfCaches[fontFamily] = std::move(typeface);
            return true;
        } while (0);
#if (ARDA_DEBUG == 1)
        ARDA_LOG_ERROR("loadFontFile %s error %d", fontFamily.c_str(), errcode);
#endif
        return false;
    }

    int __uuid = 1;

    Canvas::Canvas(int width, int height)
            : _width(width), _height(height), _needRecreate(true), uuid(__uuid++) {
        // ensureValidSurface();
        // updateFont();
        // 初始化默认画笔样式
        _fillPaint.setStyle(SkPaint::kFill_Style);
        _fillPaint.setAntiAlias(true);

        _strokePaint.setStyle(SkPaint::kStroke_Style);
        _strokePaint.setAntiAlias(true);
        _strokePaint.setStrokeWidth(1.0f);
    }

    Canvas::~Canvas() {
        // SkSurface 和 SkCanvas 由 sk_sp 智能指针管理
    }

    void Canvas::setWidth(int width) {
        if (_width != width) {
            _width = width;
            _needRecreate = true;
        }
    }

    void Canvas::setHeight(int height) {
        if (_height != height) {
            _height = height;
            _needRecreate = true;
        }
    }

    void Canvas::ensureValidSurface() {
        if (_needRecreate) {
            if (_width == 0 || _height == 0) {
                //            ARDA_LOG_ERROR("[Canvas] size error!");
                _width = std::max(_width, 1);
                _height = std::max(_height, 1);
            }
            const SkImageInfo &info =
                    SkImageInfo::Make(_width, _height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
            _surface = SkSurfaces::Raster(info);
            _canvas = _surface->getCanvas();
            _canvas->clear(SK_ColorTRANSPARENT);  // 重要：初始化为透明背景
            _needRecreate = false;
        }
    }

    void Canvas::updateFont() {
        //    ARDA_LOG_DEBUG("[Canvas]%d updateFont family: %s ,fontsize: %f", uuid,
        //    _fontProps.family.c_str(), _fontProps.size); _fontProps.family = "Arial";
        // 获取字体
        sk_sp<SkTypeface> typeface = nullptr;
        // 首先查找是否有已加载的自定义字体
        auto it = _ttfCaches.find(_fontProps.family);
        if (it != _ttfCaches.end()) {
            typeface = it->second;
#if (ARDA_DEBUG == 1)
            ARDA_LOG_DEBUG("[Canvas]%d USE TTF %s", uuid, _fontProps.family.c_str());
#endif
        } else {
            // 如果没有找到自定义字体，使用系统字体
            // 2. 创建字体样式描述
            SkFontStyle::Weight weight =
                    _fontProps.bold ? SkFontStyle::kBold_Weight : SkFontStyle::kNormal_Weight;

            SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;
            if (_fontProps.italic) {
                slant = SkFontStyle::kItalic_Slant;  // 真正的斜体
            } else if (_fontProps.oblique) {
                slant = SkFontStyle::kOblique_Slant;  // 倾斜变形
            }

            SkFontStyle style(weight, SkFontStyle::kNormal_Width, slant);
            typeface = GetFontMgr()->matchFamilyStyle(_fontProps.family.c_str(), style);
        }
        // 确保typeface有效
        if (!typeface) {
            // 添加调试信息
            ARDA_LOG_ERROR("[Canvas] %d updateFont Failed to create typeface ! %s", uuid,
                           _fontProps.family.c_str());
            return;
        }
        // 设置字体属性
        // 处理 small-caps（需要字体支持 OpenType 特性）
        if (_fontProps.smallCaps) {
            constexpr SkFontArguments::VariationPosition::Coordinate coords[] = {
                    {SkSetFourByteTag('s', 'm', 'c', 'p'), 1.0f}  // smcp 特性
            };
            SkFontArguments args;
            args.setVariationDesignPosition({coords, 1});
            _font = SkFont(typeface->makeClone(args), _fontProps.size);
        } else {
            _font = SkFont(typeface, _fontProps.size);
        }

        // 2. 确保缩放正确
        _font.setScaleX(1.0f);
        // 1. Bold - 通过描边或缩放模拟粗体
        if (_fontProps.bold && !typeface->isBold()) {
            _font.setEmbolden(true);  // Skia内置的加粗
        }

        // 2. Italic/Oblique - 通过倾斜模拟
        if ((_fontProps.italic || _fontProps.oblique) && !typeface->isItalic()) {
            _font.setSkewX(-0.25f);  // 向右倾斜约14度
        }
        //    _font.setSkewX(0.0f);
        //        _font.setEdging(SkFont::Edging::kAlias);
    }

    void
    Canvas::drawText(const std::string &text, float x, float y, float maxWidth, SkPaint &paint) {
        ensureValidSurface();
        updateShadowEffect(paint);
        if (_font.getSize() != _fontProps.size) {
//            updateFont();
        }
        // 测量文本实际宽度
        float textWidth = measureText(text);
        calculateTextPosition(textWidth, x, y, tempPoint);
        ARDA_LOG_DEBUG("[Canvas]%d drawText: %s, textWidth : %f", uuid, text.data(), textWidth);
        // 如果指定了maxWidth且文本超出限制
        if (maxWidth > 0 && textWidth > maxWidth) {
            // 计算缩放比例
            float scale = maxWidth / textWidth;

            // 保存当前状态
            _canvas->save();
            // 应用水平缩放
            _canvas->translate(tempPoint.fX, tempPoint.fY);
            _canvas->scale(scale, 1.0f);
            _canvas->translate(-tempPoint.fX, -tempPoint.fY);
            // 根据方向调整文本
            if (_direction == "rtl") {
                tempPoint.fX += textWidth;
                _font.setForceAutoHinting(true);
                _font.setHinting(SkFontHinting::kFull);
            }
            // 绘制文本
            sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromText(text.data(), text.length(), _font);
            _canvas->drawTextBlob(blob, tempPoint.fX, tempPoint.fY, paint);
//            _canvas->drawSimpleText(text.data(),text.size(),SkTextEncoding::kUTF8,tempPoint.fX, tempPoint.fY,_font,paint);
            // 恢复状态
            _canvas->restore();
        } else {
            // 正常绘制文本
            if (_direction == "rtl") {
                tempPoint.fX += textWidth;
                _font.setForceAutoHinting(true);
                _font.setHinting(SkFontHinting::kFull);
            }
//            _canvas->drawSimpleText(text.data(),text.size(),SkTextEncoding::kUTF8,tempPoint.fX, tempPoint.fY,_font,paint);

            sk_sp<SkTextBlob> blob = SkTextBlob::MakeFromText(text.data(), text.length(), _font);
            _canvas->drawTextBlob(blob, tempPoint.fX, tempPoint.fY, paint);
        }
    }

    void Canvas::fillText(const std::string &text, float x, float y, float maxWidth) {
        drawText(text, x, y, maxWidth, _fillPaint);
    }

    void Canvas::strokeText(const std::string &text, float x, float y, float maxWidth) {
        drawText(text, x, y, maxWidth, _strokePaint);
    }

    const std::string &Canvas::getFont() {
        return _fontStr;
    }

    void Canvas::setFont(const std::string_view &fontVal) {
        ARDA_LOG_DEBUG("[Canvas]%d setFont : %s", uuid, fontVal.data());
        if (_fontStr == fontVal) {
            return;
        }
        _fontStr = fontVal;
        std::string fontName = "sans-serif";
        std::string fontSizeStr = "30";
        std::regex re("\\s*((\\d+)([\\.]\\d+)?)px\\s+([^\\r\\n]*)");
        std::match_results<std::string::const_iterator> results;
        if (std::regex_search(_fontStr.cbegin(), _fontStr.cend(), results, re)) {
            fontSizeStr = results[2].str();
            // support get font name from `60px American` or `60px "American abc-abc_abc"`
            // support get font name contain space,example `times new roman`
            // if regex rule that does not conform to the rules,such as Chinese,it defaults to
            // sans-serif
            std::match_results<std::string::const_iterator> fontResults;
            std::regex fontRe("([\\w\\s-]+|\"[\\w\\s-]+\"$)");
            std::string rest4 = results[4].str();
            if (std::regex_match(rest4, fontResults, fontRe)) {
                fontName = results[4].str();
            }
        }
        float fontSize = atof(fontSizeStr.c_str());
        bool isBold = _fontStr.find("bold", 0) != std::string::npos;
        bool isItalic = _fontStr.find("italic", 0) != std::string::npos;
        bool isSmallCaps = _fontStr.find("small-caps", 0) != std::string::npos;
        bool isOblique = _fontStr.find("oblique", 0) != std::string::npos;
        setFont(fontName, fontSize, isBold, isItalic, isOblique, isSmallCaps);
    }

    void
    Canvas::setFont(const std::string &family, float size, bool bold, bool italic, bool oblique,
                    bool smallCaps) {
        _fontProps.family = family;
        _fontProps.size = size;
        _fontProps.bold = bold;
        _fontProps.italic = italic;
        _fontProps.oblique = oblique;
        _fontProps.smallCaps = smallCaps;
        updateFont();
    }

    float Canvas::measureText(const std::string &text) {
        if (text.empty()) {
            return 0;
        }
        if (!_font.getTypeface()) {
#if (ARDA_DEBUG == 1)
            ARDA_LOG_ERROR("[Canvas] No typeface set!");
#endif
            updateFont();  // 尝试重新设置字体
        }
        SkRect size;
         _font.measureText(text.data(), text.size(), SkTextEncoding::kUTF8, &size);
        //    _font.setSubpixel(true);
        //    _font.setHinting(SkFontHinting::kFull);
        //    _font.setEdging(SkFont::Edging::kAntiAlias);

        auto *typeface = _font.getTypeface();
        SkString ttfName;
        typeface->getFamilyName(&ttfName);
        int countTables = typeface->countTables();
        int countGlyphs = typeface->countGlyphs();
        float totalWidth = size.width();
        SkFontMetrics metrics;
        _font.getMetrics(&metrics);
        if (totalWidth > 1000){
            _font.dump();
            ARDA_LOG_ERROR("Content Size is too large! %s",text.data());
        }
        if (text.length() > 0 && totalWidth < 0.01) {
            _font.dump();
            totalWidth = _font.measureText(text.data(), text.size(), SkTextEncoding::kUTF8);
            ARDA_LOG_ERROR("%s && totalWidth < 0.01", text.data());
        }
        return totalWidth;
    }

    void Canvas::fillRect(float x, float y, float width, float height) {
        ensureValidSurface();
        updateShadowEffect(_fillPaint);
        _canvas->drawRect(SkRect::MakeXYWH(x, y, width, height), _fillPaint);
    }

    void Canvas::clearRect(float x, float y, float width, float height) {
        ensureValidSurface();
        SkPaint clearPaint;
        clearPaint.setBlendMode(SkBlendMode::kClear);
        _canvas->drawRect(SkRect::MakeXYWH(x, y, width, height), clearPaint);
    }

    void Canvas::strokeRect(float x, float y, float width, float height) {
        ensureValidSurface();
        updateShadowEffect(_strokePaint);
        _canvas->drawRect(SkRect::MakeXYWH(x, y, width, height), _strokePaint);
    }

    void Canvas::beginPath() {
        _currentPath.reset();
    }

    void Canvas::moveTo(float x, float y) {
        _currentPath.moveTo(x, y);
    }

    void Canvas::lineTo(float x, float y) {
        _currentPath.lineTo(x, y);
    }

    void Canvas::stroke() {
        ensureValidSurface();
        updateShadowEffect(_strokePaint);
        _canvas->drawPath(_currentPath, _strokePaint);
    }

    void Canvas::fill() {
        ensureValidSurface();
        updateShadowEffect(_fillPaint);
        _canvas->drawPath(_currentPath, _fillPaint);
    }

    void Canvas::closePath() {
        _currentPath.close();
    }

    void Canvas::setFillStyle(uint32_t color) {
        _fillPaint.setShader(nullptr);  // 先清除任何现有的 shader
        _fillPaint.setColor(color);
    }

    void Canvas::setFillStyle(const CanvasGradient *gradient) {
        if (gradient->shader) {
            _fillPaint.setShader(gradient->shader);
            _fillPaint.setColor(SK_ColorBLACK);  // 设置一个默认颜色
        }
    }

    void Canvas::setFillStyle(const CanvasPattern *pattern) {
        if (pattern->shader) {
            _fillPaint.setShader(pattern->shader);
            _fillPaint.setColor(SK_ColorBLACK);  // 设置一个默认颜色
        }
    }

    void Canvas::setStrokeStyle(uint32_t color) {
        _strokePaint.setColor(color);
    }

    void Canvas::setLineWidth(float width) {
        _strokePaint.setStrokeWidth(width);
    }

    float Canvas::getLineWidth() {
        return _strokePaint.getStrokeWidth();
    }

    void Canvas::getImageData(int sx, int sy, int sw, int sh, ImageData *output) {
        ensureValidSurface();

        // 处理负宽度：向左延伸
        if (sw < 0) {
            sx += sw;  // 调整起始x坐标
            sw = -sw;  // 转为正值
        }

        // 处理负高度：向上延伸
        if (sh < 0) {
            sy += sh;  // 调整起始y坐标
            sh = -sh;  // 转为正值
        }

        // 确保坐标在画布范围内
        sx = std::max(0, std::min(sx, _width));
        sy = std::max(0, std::min(sy, _height));

        // 确保不超出画布边界
        sw = std::min(sw, _width - sx);
        sh = std::min(sh, _height - sy);

        // 如果最终区域无效，返回空数据
        if (sw <= 0 || sh <= 0) {
            output->width = 0;
            output->height = 0;
            output->size = 0;
            output->data = nullptr;
            return;
        }

        // 设置输出参数
        output->width = sw;
        output->height = sh;
        size_t rowBytes = sw * 4;
        uint32_t datasize = rowBytes * sh;
        if (datasize != output->size) {
            output->size = sw * sh * 4;
            output->data = (unsigned char *) malloc(sizeof(unsigned char) * output->size);
        }
        // 读取像素数据
        SkImageInfo info = SkImageInfo::Make(sw, sh, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);

        _surface->readPixels(info, output->data, rowBytes, sx, sy);
    }

    void Canvas::clear(uint32_t color) {
        ensureValidSurface();
        _canvas->clear(color);
    }

    void Canvas::setTextAlign(const std::string &align) {
        _textAlign = align;
    }

    void Canvas::setTextBaseline(const std::string &baseline) {
        _textBaseline = baseline;
    }

    void Canvas::calculateTextPosition(float textWidth, float x, float y, SkPoint &size) {
        size.set(x, y);
        // 处理水平对齐
        if (_textAlign != "left") {
            if (_textAlign == "center") {
                size.fX -= textWidth / 2;
            } else if (_textAlign == "right") {
                size.fX -= textWidth;
            }
        }

        // 处理基线对齐
        SkFontMetrics metrics;
        _font.getMetrics(&metrics);
        if (_textBaseline == "top") {
            size.fY += -metrics.fTop;
        } else if (_textBaseline == "hanging") {
            size.fY += -(metrics.fAscent + metrics.fTop) / 2;
        } else if (_textBaseline == "middle") {
            size.fY += (metrics.fDescent - metrics.fAscent) / 2;
        } else if (_textBaseline == "alphabetic") {
            //
        } else if (_textBaseline == "ideographic") {
            size.fY += metrics.fDescent;
        } else if (_textBaseline == "bottom") {
            size.fY += metrics.fBottom;
        }
    }

    void Canvas::putImageData(const ImageData &imageData, int dx, int dy, int dirtyX, int dirtyY,
                              int dirtyWidth, int dirtyHeight) {
        ensureValidSurface();

        // 参数验证和默认值处理
        dirtyX = std::max(0, dirtyX);
        dirtyY = std::max(0, dirtyY);

        // 如果未指定脏区域尺寸，使用整个图像
        if (dirtyWidth <= 0) {
            dirtyWidth = imageData.width;
        }
        if (dirtyHeight <= 0) {
            dirtyHeight = imageData.height;
        }

        // 确保脏区域不超出源图像边界
        dirtyWidth = std::min(dirtyWidth, imageData.width - dirtyX);
        dirtyHeight = std::min(dirtyHeight, imageData.height - dirtyY);

        // 如果脏区域无效，直接返回
        if (dirtyWidth <= 0 || dirtyHeight <= 0) {
            return;
        }

        // 计算源数据的起始位置
        const uint8_t *srcData = imageData.data + (dirtyY * imageData.width + dirtyX) * 4;

        // 创建只包含脏区域的SkPixmap
        SkImageInfo dirtyInfo =
                SkImageInfo::Make(dirtyWidth, dirtyHeight, kRGBA_8888_SkColorType,
                                  kUnpremul_SkAlphaType);

        // 计算每行的字节数
        size_t srcRowBytes = imageData.width * 4;

        SkPixmap dirtyPixmap(dirtyInfo, srcData,
                             srcRowBytes  // 使用原始图像的行宽度
        );

        // 写入像素数据
        _surface->writePixels(dirtyPixmap,
                              dx + dirtyX,  // 目标位置需要考虑脏区域偏移
                              dy + dirtyY);
    }

    void Canvas::setTransform(float a, float b, float c, float d, float e, float f) {
        ensureValidSurface();
        // 2. 设置新的变换矩阵
        SkMatrix matrix;
        matrix.setAll(a, c, e,  // a c e
                      b, d, f,  // b d f
                      0, 0, 1   // 0 0 1 (perspective components)
        );

        // 3. 应用新的变换
        _canvas->setMatrix(matrix);
    }

    void Canvas::transform(float a, float b, float c, float d, float e, float f) {
        ensureValidSurface();

        // 创建新的变换矩阵
        SkMatrix newMatrix;
        newMatrix.setAll(a, c, e,  // a c e
                         b, d, f,  // b d f
                         0, 0, 1   // 0 0 1 (perspective components)
        );

        // 将新矩阵与当前变换矩阵相乘（连接变换）
        _canvas->concat(newMatrix);
    }

    void Canvas::translate(float x, float y) {
        ensureValidSurface();
        _canvas->translate(x, y);
    }

    void Canvas::updateShadowEffect(SkPaint &paint) const {
        if (_shadowBlur > 0 || _shadowOffsetX != 0 || _shadowOffsetY != 0) {
            // 创建阴影效果
            paint.setImageFilter(SkImageFilters::DropShadow(
                    _shadowOffsetX,   // dx
                    _shadowOffsetY,   // dy
                    _shadowBlur / 2,  // sigmaX (模糊度的一半会得到更接近Web Canvas的效果)
                    _shadowBlur / 2,  // sigmaY
                    _shadowColor,     // 阴影颜色
                    nullptr           // 输入滤镜
            ));
        } else {
            // 如果没有阴影效果，清除滤镜
            paint.setImageFilter(nullptr);
        }
    }

    void Canvas::setShadowBlur(float blur) {
        if (_shadowBlur != blur) {
            _shadowBlur = blur;
        }
    }

    void Canvas::setShadowColor(uint32_t color) {
        if (_shadowColor != color) {
            _shadowColor = color;
        }
    }

    void Canvas::setShadowOffsetX(float offsetX) {
        if (_shadowOffsetX != offsetX) {
            _shadowOffsetX = offsetX;
        }
    }

    void Canvas::setShadowOffsetY(float offsetY) {
        if (_shadowOffsetY != offsetY) {
            _shadowOffsetY = offsetY;
        }
    }

    void Canvas::createLinearGradient(CanvasGradient *canvasGradient, float x0, float y0, float x1,
                                      float y1) {
        canvasGradient->start = SkPoint::Make(x0, y0);
        canvasGradient->end = SkPoint::Make(x1, y1);
    }

    void Canvas::createPattern(CanvasPattern *pattern, const sk_sp<SkImage> &image,
                               const std::string &repetition) {
        pattern->image = image;

        // 解析重复模式
        if (repetition == "repeat") {
            pattern->tmx = SkTileMode::kRepeat;
            pattern->tmy = SkTileMode::kRepeat;
        } else if (repetition == "repeat-x") {
            pattern->tmx = SkTileMode::kRepeat;
            pattern->tmy = SkTileMode::kClamp;
        } else if (repetition == "repeat-y") {
            pattern->tmx = SkTileMode::kClamp;
            pattern->tmy = SkTileMode::kRepeat;
        } else {  // "no-repeat" 或其他
            pattern->tmx = SkTileMode::kClamp;
            pattern->tmy = SkTileMode::kClamp;
        }

        // 创建 shader
        if (image) {
            pattern->shader = image->makeShader(pattern->tmx, pattern->tmy,
                                                SkSamplingOptions()  // 默认采样选项
            );
        }
    }

    void Canvas::arc(float x, float y, float radius, float startAngle, float endAngle,
                     bool counterclockwise) {
        // 确保角度是按照顺时针方向
        if (counterclockwise) {
            std::swap(startAngle, endAngle);
        }

        // 转换为度数
        float startDegrees = startAngle * 180.0f / M_PI;
        float endDegrees = endAngle * 180.0f / M_PI;

        // 计算扫描角度
        float sweepDegrees = endDegrees - startDegrees;
        if (counterclockwise) {
            sweepDegrees = -sweepDegrees;
        }

        // 如果路径为空，移动到起点
        if (_currentPath.isEmpty()) {
            float startX = x + radius * std::cos(startAngle);
            float startY = y + radius * std::sin(startAngle);
            _currentPath.moveTo(startX, startY);
        }

        // 添加圆弧
        SkRect rect = SkRect::MakeXYWH(x - radius, y - radius, radius * 2, radius * 2);
        _currentPath.arcTo(rect, startDegrees, sweepDegrees, false);
    }

    void Canvas::arcTo(float x1, float y1, float x2, float y2, float radius) {
        // 如果路径为空，移动到当前点
        if (_currentPath.isEmpty()) {
            _currentPath.moveTo(x1, y1);
        }

        // 添加圆弧
        _currentPath.arcTo(x1, y1, x2, y2, radius);
    }

    void Canvas::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y) {
        // 如果路径为空，移动到第一个控制点
        if (_currentPath.isEmpty()) {
            _currentPath.moveTo(cp1x, cp1y);
        }

        // 添加三次贝塞尔曲线
        _currentPath.cubicTo(cp1x, cp1y, cp2x, cp2y, x, y);
    }

    void Canvas::setDirection(const std::string &direction) {
        if (_direction != direction) {
            _direction = direction;
        }
    }

    void Canvas::drawImage(const sk_sp<SkImage> &image, float dx, float dy) {
        ensureValidSurface();
        if (image) {
            updateShadowEffect(_fillPaint);
            _canvas->drawImage(image, dx, dy, SkSamplingOptions(), &_fillPaint);
        }
    }

    void Canvas::drawImage(const sk_sp<SkImage> &image, float dx, float dy, float dw, float dh) {
        ensureValidSurface();
        if (image) {
            updateShadowEffect(_fillPaint);
            SkRect dst = SkRect::MakeXYWH(dx, dy, dw, dh);
            _canvas->drawImageRect(image, dst, SkSamplingOptions());
        }
    }

    void Canvas::drawImage(const sk_sp<SkImage> &image, float sx, float sy, float sw, float sh,
                           float dx, float dy, float dw, float dh) {
        ensureValidSurface();
        if (image) {
            updateShadowEffect(_fillPaint);
            SkRect src = SkRect::MakeXYWH(sx, sy, sw, sh);
            SkRect dst = SkRect::MakeXYWH(dx, dy, dw, dh);
            _canvas->drawImageRect(image, src, dst, SkSamplingOptions(), &_fillPaint,
                                   SkCanvas::SrcRectConstraint::kFast_SrcRectConstraint);
        }
    }

    sk_sp<SkImage> convertArdaImageToSkImage(const arda::Image *ardaImage) {
        // 假设 arda::Image 提供了这些方法
        const uint8_t *pixels = ardaImage->getData();
        int width = ardaImage->getWidth();
        int height = ardaImage->getHeight();
        // 创建 SkImage
        return SkImages::RasterFromData(
                SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType),
                SkData::MakeWithoutCopy(pixels, width * height * 4),  // 数据
                width * 4                                             // stride: 每行字节数
        );
    }

    void Canvas::drawImage(const arda::Image *image, float dx, float dy) {
        ensureValidSurface();
        sk_sp<SkImage> skImage = convertArdaImageToSkImage(image);
        if (skImage) {
            updateShadowEffect(_fillPaint);
            _canvas->drawImage(skImage, dx, dy, SkSamplingOptions(), &_fillPaint);
        }
    }

    void Canvas::restore() {
        ensureValidSurface();
        _canvas->restore();  // 恢复 Skia 画布状态

        // 如果状态栈为空，直接返回
        if (_stateStack.empty()) {
            return;
        }

        // 从栈顶恢复状态
        CanvasState state = _stateStack.back();
        _stateStack.pop_back();

        // 恢复所有状态
        _fillPaint = state.fillPaint;
        _strokePaint = state.strokePaint;
        _font = state.font;
        _fontProps = state.fontProps;
        _textAlign = state.textAlign;
        _textBaseline = state.textBaseline;
        _direction = state.direction;
        _shadowBlur = state.shadowBlur;
        _shadowColor = state.shadowColor;
        _shadowOffsetX = state.shadowOffsetX;
        _shadowOffsetY = state.shadowOffsetY;
        setLineJoin(state.lineJoin);
        updateFont();
    }

    void Canvas::save() {
        ensureValidSurface();
        _canvas->save();  // 保存 Skia 画布状态

        // 创建当前状态的副本并压入栈
        CanvasState state;
        state.fillPaint = _fillPaint;
        state.strokePaint = _strokePaint;
        state.font = _font;
        state.fontProps = _fontProps;
        state.textAlign = _textAlign;
        state.textBaseline = _textBaseline;
        state.direction = _direction;
        state.shadowBlur = _shadowBlur;
        state.shadowColor = _shadowColor;
        state.shadowOffsetX = _shadowOffsetX;
        state.shadowOffsetY = _shadowOffsetY;
        state.lineJoin = _lineJoin;

        _stateStack.push_back(state);
    }

    void Canvas::scale(float scaleX, float scaleY) {
        ensureValidSurface();

        // 如果未指定scaleY，使用scaleX值（均匀缩放）
        if (scaleY == 0.0f) {
            scaleY = scaleX;
        }

        // 应用缩放变换
        _canvas->scale(scaleX, scaleY);
    }

    void Canvas::rect(float x, float y, float width, float height) {
        // 如果宽度或高度为0，则不添加任何路径
        if (width == 0 || height == 0) {
            return;
        }

        // 按照顺时针方向添加矩形路径
        _currentPath.moveTo(x, y);                   // 左上角
        _currentPath.lineTo(x + width, y);           // 右上角
        _currentPath.lineTo(x + width, y + height);  // 右下角
        _currentPath.lineTo(x, y + height);          // 左下角
        _currentPath.close();                        // 闭合路径
    }

    void Canvas::rotate(float angle) {
        ensureValidSurface();

        // 应用旋转变换
        _canvas->rotate(angle * 180.0f / M_PI);  // Skia使用角度，需要将弧度转换为角度
    }

    void Canvas::setLineJoin(const std::string &join) {
        _lineJoin = join;
        // 更新描边画笔的连接样式
        if (join == "round") {
            _strokePaint.setStrokeJoin(SkPaint::kRound_Join);
        } else if (join == "bevel") {
            _strokePaint.setStrokeJoin(SkPaint::kBevel_Join);
        } else {
            _strokePaint.setStrokeJoin(SkPaint::kMiter_Join);
        }
    }

    const std::string &Canvas::getLineJoin() {
        return _lineJoin;
    }

    const std::string &Canvas::getTextBaseline() {
        return _textBaseline;
    }

    const std::string &Canvas::getTextAlign() {
        return _textAlign;
    }

    const std::string &Canvas::getDirection() {
        return _direction;
    }

    void Canvas::reset() {
        ensureValidSurface();

        // 清空状态栈
        _stateStack.clear();

        // 重置变换矩阵
        _canvas->resetMatrix();

        // 重置路径
        _currentPath.reset();

        // 重置填充样式
        _fillPaint.reset();
        _fillPaint.setStyle(SkPaint::kFill_Style);
        _fillPaint.setColor(SK_ColorTRANSPARENT);
        _fillPaint.setAntiAlias(true);

        // 重置描边样式
        _strokePaint.reset();
        _strokePaint.setStyle(SkPaint::kStroke_Style);
        _strokePaint.setColor(SK_ColorTRANSPARENT);
        _strokePaint.setStrokeWidth(1.0f);
        _strokePaint.setStrokeJoin(SkPaint::kMiter_Join);
        _strokePaint.setAntiAlias(true);

        // 重置字体属性
        _fontProps = FontProperties();
        _fontStr = "";  // 清空字体字符串
        //    _font = SkFont();
        // 重置阴影属性
        _shadowBlur = 0;
        _shadowColor = SK_ColorTRANSPARENT;
        _shadowOffsetX = 0;
        _shadowOffsetY = 0;

        // 重置线条样式
        _lineJoin = "miter";  // 使用字符串类型的默认值
        // 重置文本对齐
        _textAlign = "left";
        _textBaseline = "alphabetic";
        _direction = "ltr";

        // 更新字体
        updateFont();
    }

    bool Canvas::getData(Data *output) {
        ImageData imageData;
        getImageData(0, 0, _width, _height, &imageData);
        output->fastSet(imageData.data, imageData.size);
        return output->getSize() > 0;
    }

}  // namespace arda
