/*
 * Copyright (C) 2026 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Tehreer/TRAttribute.h>
#include <Tehreer/TRComposedFrame.h>
#include <Tehreer/TRComposedLine.h>
#include <Tehreer/TRDrawCallbacks.h>
#include <Tehreer/TRFrameResolver.h>
#include <Tehreer/TRGlyphImage.h>
#include <Tehreer/TRGlyphRun.h>
#include <Tehreer/TRPath.h>
#include <Tehreer/TRRenderer.h>
#include <Tehreer/TRReplacement.h>
#include <Tehreer/TRText.h>
#include <Tehreer/TRTypeface.h>
#include <Tehreer/TRTypesetter.h>

extern "C" {
#include <API/TRRenderer.h>
}

#include "TestText.h"
#include "TestTypeface.h"

#include "DrawTests.h"

using namespace std;
using namespace Tehreer;

void DrawTests::run() {
    testGlyphRunImages();
    testRendererIsRestored();
    testForegroundColorFallback();
    testRenderScale();
    testRightToLeftRun();
    testSplitClusters();
    testSplitClusterOfRightToLeftRun();
    testRightToLeftScript();
    testSplitClusterOfRightToLeftScript();
    testDrawStyles();
    testPathOutput();
    testImagesWinOverPaths();
    testBackgrounds();
    testBackgroundsFillTheLine();
    testUnderlineAndStrikethrough();
    testDecorationColors();
    testDecorationsOfSplitClusters();
    testReplacement();
    testLineOrigins();
    testFrameOrigins();
    testMissingCallbacks();
    testMissingRenderer();
}

/*
 * In the test font, the glyphs 'a', 'b' and 'c' are 1114, 1149 and 1072 units wide, and every other
 * character, including the space, shows the notdef glyph that is 908 units wide. The units per em
 * are 2048, so a type size of 100 gives the widths below.
 */
constexpr TRFloat TypeSize = 100.0f;
constexpr TRFloat UnitScale = TypeSize / 2048.0f;
constexpr TRFloat A = 1114.0f * UnitScale;
constexpr TRFloat B = 1149.0f * UnitScale;
constexpr TRFloat C = 1072.0f * UnitScale;
constexpr TRFloat Notdef = 908.0f * UnitScale;

constexpr TRColor Red = TRColorMake(0xFF, 0xFF, 0x00, 0x00);
constexpr TRColor Green = TRColorMake(0xFF, 0x00, 0xFF, 0x00);
constexpr TRColor Blue = TRColorMake(0xFF, 0x00, 0x00, 0xFF);
constexpr TRColor Black = TRColorMake(0xFF, 0x00, 0x00, 0x00);

namespace {

struct Op {
    enum Kind { Image, Path, Rect, Replacement };

    Kind kind = Image;
    TRPoint origin = { 0.0f, 0.0f };
    TRColor color = 0;
    TRRect rect = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
    bool hasClip = false;
    TRRect clip = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
    TRDrawStyle style = TRDrawStyleFill;
    TRFloat scaleX = 1.0f;
    TRFloat scaleY = 1.0f;
    uint32_t width = 0;
    uint32_t height = 0;
    bool hasPath = false;
    TRGlyphRunRef run = nullptr;

    /* What the renderer was set up with while the callback ran. */
    TRTypefaceRef typeface = nullptr;
    TRFloat typeSize = 0.0f;
    TRColor foregroundColor = 0;
};

struct Recorder {
    TRRendererRef renderer = nullptr;
    vector<Op> ops;
};

Op makeOp(Recorder *recorder, Op::Kind kind) {
    Op op;
    op.kind = kind;
    op.typeface = recorder->renderer->typeface;
    op.typeSize = recorder->renderer->typeSize;
    op.foregroundColor = recorder->renderer->foregroundColor;

    return op;
}

void setClip(Op &op, const TRRect *clip) {
    op.hasClip = (clip != nullptr);
    if (clip) {
        op.clip = *clip;
    }
}

void recordImage(void *userData, TRGlyphImageRef image, TRPoint origin, TRFloat scaleX,
    TRFloat scaleY, TRColor color, const TRRect *clip) {
    auto *recorder = static_cast<Recorder *>(userData);
    Op op = makeOp(recorder, Op::Image);
    op.origin = origin;
    op.scaleX = scaleX;
    op.scaleY = scaleY;
    op.color = color;
    op.width = TRGlyphImageGetWidth(image);
    op.height = TRGlyphImageGetHeight(image);
    setClip(op, clip);
    recorder->ops.push_back(op);
}

void recordPath(void *userData, TRPathRef path, TRPoint origin, TRDrawStyle style, TRColor color,
    const TRRect *clip) {
    auto *recorder = static_cast<Recorder *>(userData);
    Op op = makeOp(recorder, Op::Path);
    op.origin = origin;
    op.style = style;
    op.color = color;
    op.hasPath = (path != nullptr);
    setClip(op, clip);
    recorder->ops.push_back(op);
}

void recordRect(void *userData, TRRect rect, TRColor color) {
    auto *recorder = static_cast<Recorder *>(userData);
    Op op = makeOp(recorder, Op::Rect);
    op.rect = rect;
    op.color = color;
    recorder->ops.push_back(op);
}

void recordReplacement(void *userData, const struct _TRGlyphRun *run, TRPoint origin) {
    auto *recorder = static_cast<Recorder *>(userData);
    Op op = makeOp(recorder, Op::Replacement);
    op.origin = origin;
    op.run = run;
    recorder->ops.push_back(op);
}

TRDrawCallbacks allCallbacks() {
    TRDrawCallbacks callbacks = {};
    callbacks.drawGlyphImage = recordImage;
    callbacks.drawGlyphPath = recordPath;
    callbacks.fillRect = recordRect;
    callbacks.drawReplacement = recordReplacement;

    return callbacks;
}

TRDrawCallbacks imageCallbacks() {
    TRDrawCallbacks callbacks = allCallbacks();
    callbacks.drawGlyphPath = nullptr;

    return callbacks;
}

/* A renderer that records what is drawn into the recorder. */
TRRendererRef createRecordingRenderer(Recorder *recorder, const TRDrawCallbacks &callbacks) {
    TRRendererRef renderer = TRRendererCreate();
    assert(renderer != nullptr);

    TRRendererSetDrawCallbacks(renderer, &callbacks, recorder);
    recorder->renderer = renderer;

    return renderer;
}

struct Placement {
    TRUInteger index;
    TRPoint origin;
    uint32_t width;
    uint32_t height;
};

/* The placements that the renderer gives for the glyphs of a run, set up like the draw does. */
vector<Placement> expectedPlacements(TRGlyphRunRef run, TRGlyphImageKind kind, TRFloat renderScale) {
    TRRendererRef renderer = TRRendererCreate();
    vector<Placement> placements;

    TRRendererSetTypeface(renderer, TRGlyphRunGetTypeface(run));
    TRRendererSetTypeSize(renderer, TRGlyphRunGetTypeSize(run));
    TRRendererSetScaleX(renderer, TRGlyphRunGetScaleX(run));
    TRRendererSetScaleY(renderer, TRGlyphRunGetScaleY(run));
    TRRendererSetWritingDirection(renderer, TRGlyphRunGetWritingDirection(run));
    TRRendererSetRenderScale(renderer, renderScale);

    TRRendererEnumerateGlyphPlacements(renderer, kind, TRGlyphRunGetGlyphIDsPtr(run),
        TRGlyphRunGetGlyphOffsetsPtr(run), TRGlyphRunGetGlyphAdvancesPtr(run),
        TRGlyphRunGetGlyphCount(run),
        [](void *userData, TRUInteger index, TRGlyphImageRef image, TRPoint origin, TRFloat,
            TRFloat, TRBoolean *) {
            static_cast<vector<Placement> *>(userData)->push_back({ index, origin,
                TRGlyphImageGetWidth(image), TRGlyphImageGetHeight(image) });
        }, &placements);

    TRRendererRelease(renderer);

    return placements;
}

vector<Op> filter(const vector<Op> &ops, Op::Kind kind) {
    vector<Op> result;

    for (const Op &op : ops) {
        if (op.kind == kind) {
            result.push_back(op);
        }
    }

    return result;
}

bool near(TRFloat a, TRFloat b, TRFloat tolerance = 1e-3f) {
    return fabsf(a - b) <= tolerance;
}

TRFloat roundPixel(TRFloat value) {
    return floorf(value + 0.5f);
}

bool sameOps(const vector<Op> &first, const vector<Op> &second) {
    bool isSame = (first.size() == second.size());

    for (size_t index = 0; isSame && index < first.size(); index++) {
        const Op &a = first[index];
        const Op &b = second[index];

        isSame = (a.kind == b.kind && a.origin.x == b.origin.x && a.origin.y == b.origin.y
                  && a.color == b.color && a.rect.origin.x == b.rect.origin.x
                  && a.rect.origin.y == b.rect.origin.y && a.rect.size.width == b.rect.size.width
                  && a.rect.size.height == b.rect.size.height && a.hasClip == b.hasClip
                  && a.width == b.width && a.height == b.height);
    }

    return isSame;
}

/* The distance of a code unit, which has to be within the run and its clusters. */
TRFloat distanceOf(TRGlyphRunRef run, TRUInteger codeUnitIndex) {
    TRFloat distance = 0.0f;
    bool isFound = TRGlyphRunGetCodeUnitDistance(run, codeUnitIndex, &distance);
    assert(isFound == true);

    return distance;
}

struct ClipEdges {
    TRFloat farLeft;
    TRFloat farRight;
    TRFloat top;
    TRFloat bottom;
};

/* How far the clip reaches over the ink of the run and its metrics, in pixels. */
constexpr TRFloat ClipSlack = 2.0f;

/* The free edges of the clip of a split cluster, from the ink box and the metrics of the run. */
ClipEdges expectedClipEdges(TRGlyphRunRef run, TRPoint baseline, TRFloat scale) {
    TRRendererRef renderer = TRRendererCreate();
    TRRect ink = TRGlyphRunGetInkBox(run, renderer);
    TRRendererRelease(renderer);

    TRFloat extentLeft = min(distanceOf(run, TRGlyphRunGetCodeUnitStart(run)
                                              - TRGlyphRunGetStartExtraLength(run)),
                             distanceOf(run, TRGlyphRunGetCodeUnitEnd(run)
                                              + TRGlyphRunGetEndExtraLength(run)));
    TRFloat inkLeft = baseline.x + extentLeft + ink.origin.x;
    TRFloat inkRight = inkLeft + ink.size.width;
    TRFloat inkTop = baseline.y + ink.origin.y;
    TRFloat inkBottom = inkTop + ink.size.height;
    TRFloat metricsTop = baseline.y - TRGlyphRunGetAscent(run);
    TRFloat metricsBottom = baseline.y + TRGlyphRunGetDescent(run) + TRGlyphRunGetLeading(run);

    ClipEdges edges;
    edges.farLeft = floorf(inkLeft * scale) - ClipSlack;
    edges.farRight = ceilf(inkRight * scale) + ClipSlack;
    edges.top = floorf(min(metricsTop, inkTop) * scale) - ClipSlack;
    edges.bottom = ceilf(max(metricsBottom, inkBottom) * scale) + ClipSlack;

    return edges;
}

struct ColorValue {
    bool isSet;
    TRColor color;
};

ColorValue readBackground(TRGlyphRunRef run) {
    ColorValue value = { false, 0 };
    value.isSet = TRGlyphRunGetBackgroundColor(run, &value.color);

    return value;
}

ColorValue readDecoration(TRGlyphRunRef run) {
    ColorValue value = { false, 0 };
    value.isSet = TRGlyphRunGetDecorationColor(run, &value.color);

    return value;
}

struct Fixture {
    TRTypefaceRef typeface;
    TRMutableTextRef text;
    TRTypesetterRef typesetter;

    explicit Fixture(const u16string &string, const char *fontName = "Roboto-Regular.abc.ttf",
        TRFloat typeSize = TypeSize) {
        typeface = createTestTypeface(fontName);
        text = makeTestText(string, typeface, typeSize);
        typesetter = nullptr;
    }

    TRTypesetterRef make() {
        if (!typesetter) {
            typesetter = TRTypesetterCreate(text, nullptr, 0);
            assert(typesetter != nullptr);
        }

        return typesetter;
    }

    /* The typesetter is made again, so that it sees the attributes that were set since. */
    TRComposedLineRef line(TRUInteger start, TRUInteger end) {
        if (typesetter) {
            TRTypesetterRelease(typesetter);
            typesetter = nullptr;
        }

        TRComposedLineRef result = TRTypesetterCreateSimpleLine(make(), start, end - start);
        assert(result != nullptr);

        return result;
    }

    TRComposedFrameRef frame(TRFloat width) {
        if (typesetter) {
            TRTypesetterRelease(typesetter);
            typesetter = nullptr;
        }

        TRFrameResolverRef resolver = TRFrameResolverCreate();
        TRFrameResolverSetTypesetter(resolver, make());
        TRFrameResolverSetFrameSize(resolver, width, 10000.0f);

        TRComposedFrameRef result = TRFrameResolverCreateFrame(resolver, 0, TRTextGetLength(text));
        assert(result != nullptr);

        TRFrameResolverRelease(resolver);

        return result;
    }

    void setColor(TRUInteger index, TRUInteger length, TRAttributeType type, TRColor color) {
        TRAttribute attribute = {};
        attribute.type = type;

        if (type == TRAttributeForegroundColor) {
            attribute.value.foregroundColor = color;
        } else if (type == TRAttributeBackgroundColor) {
            attribute.value.backgroundColor = color;
        } else {
            attribute.value.decorationColor = color;
        }

        TRTextSetAttribute(text, index, length, &attribute);
    }

    void setFlag(TRUInteger index, TRUInteger length, TRAttributeType type) {
        TRAttribute attribute = {};
        attribute.type = type;
        attribute.value.underline = TRTrue;

        if (type == TRAttributeStrikethrough) {
            attribute.value.strikethrough = TRTrue;
        }

        TRTextSetAttribute(text, index, length, &attribute);
    }

    ~Fixture() {
        if (typesetter) {
            TRTypesetterRelease(typesetter);
        }
        TRTextRelease(text);
        TRTypefaceRelease(typeface);
    }
};

void computeRoom(void *userData, TRFloat, TRReplacementRoom *room) {
    room->ascent = 30.0f;
    room->descent = 10.0f;
    room->extent = *static_cast<TRFloat *>(userData);
}

}

void DrawTests::testGlyphRunImages() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);
    assert(TRComposedLineGetGlyphRunCount(line) == 1);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRRendererSetForegroundColor(renderer, Red);

    TRPoint origin = { 10.4f, 50.6f };
    TRGlyphRunDraw(run, renderer, origin);

    /* The run draws its glyphs from the origin, whose pixel is rounded once. */
    vector<Placement> placements = expectedPlacements(run, TRGlyphImageKindFill, 1.0f);
    assert(placements.size() == 3);
    assert(recorder.ops.size() == 3);

    for (size_t index = 0; index < 3; index++) {
        const Op &op = recorder.ops[index];

        assert(op.kind == Op::Image);
        assert(op.origin.x == 10.0f + placements[index].origin.x);
        assert(op.origin.y == 51.0f + placements[index].origin.y);
        assert(op.width == placements[index].width && op.height == placements[index].height);
        assert(op.scaleX == 1.0f && op.scaleY == 1.0f);
        assert(op.color == Red);
        assert(op.hasClip == false);
    }

    /* The glyphs are in the order of the writing, and the origin of the run in its line is not added. */
    assert(recorder.ops[1].origin.x > recorder.ops[0].origin.x);
    assert(recorder.ops[2].origin.x > recorder.ops[1].origin.x);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testRendererIsRestored() {
    Fixture f(u"abc");
    TRTypefaceRef other = createTestTypeface("Roboto-Variable.abc.ttf");
    TRComposedLineRef line = f.line(0, 3);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRRendererSetTypeface(renderer, other);
    TRRendererSetTypeSize(renderer, 33.0f);
    TRRendererSetScaleX(renderer, 2.0f);
    TRRendererSetScaleY(renderer, 3.0f);
    TRRendererSetWritingDirection(renderer, TRWritingDirectionRightToLeft);
    TRRendererSetForegroundColor(renderer, Blue);

    f.setColor(0, 3, TRAttributeForegroundColor, Green);
    TRComposedLineRelease(line);
    line = f.line(0, 3);
    run = TRComposedLineGetGlyphRun(line, 0);

    TRGlyphRunDraw(run, renderer, { 0.0f, 20.0f });
    assert(recorder.ops.size() == 3);

    /* The renderer is set up for the run while it draws. */
    for (const Op &op : recorder.ops) {
        assert(op.typeface == f.typeface);
        assert(op.typeSize == TypeSize);
        assert(op.foregroundColor == Green);
        assert(op.color == Green);
    }

    /* And it gets back what it had. */
    assert(renderer->typeface == other);
    assert(renderer->typeSize == 33.0f);
    assert(renderer->scaleX == 2.0f && renderer->scaleY == 3.0f);
    assert(renderer->writingDirection == TRWritingDirectionRightToLeft);
    assert(renderer->foregroundColor == Blue);
    assert(renderer->renderScale == 1.0f);
    assert(renderer->drawCallbacks.drawGlyphImage == recordImage);
    assert(renderer->drawUserData == &recorder);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
    TRTypefaceRelease(other);
}

void DrawTests::testForegroundColorFallback() {
    Fixture f(u"abc");
    f.setColor(1, 1, TRAttributeForegroundColor, Green);
    TRComposedLineRef line = f.line(0, 3);
    assert(TRComposedLineGetGlyphRunCount(line) == 3);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRRendererSetForegroundColor(renderer, Red);

    TRComposedLineDraw(line, renderer, { 0.0f, 40.0f });
    assert(recorder.ops.size() == 3);

    /* A run without a color is drawn with the one of the renderer. */
    assert(recorder.ops[0].color == Red);
    assert(recorder.ops[1].color == Green);
    assert(recorder.ops[2].color == Red);
    assert(renderer->foregroundColor == Red);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testRenderScale() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRRendererSetRenderScale(renderer, 2.0f);

    TRGlyphRunDraw(run, renderer, { 10.4f, 50.6f });

    /* The origin is multiplied by the scale and rounded, and the images are rendered at the scale. */
    vector<Placement> placements = expectedPlacements(run, TRGlyphImageKindFill, 2.0f);
    assert(recorder.ops.size() == 3 && placements.size() == 3);

    for (size_t index = 0; index < 3; index++) {
        assert(recorder.ops[index].origin.x == 21.0f + placements[index].origin.x);
        assert(recorder.ops[index].origin.y == 101.0f + placements[index].origin.y);
        assert(recorder.ops[index].width == placements[index].width);
    }

    assert(renderer->renderScale == 2.0f);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

/* The override makes the letters a run of the right-to-left level that is written from the left. */
static TRUInteger findRightToLeftRun(TRComposedLineRef line) {
    TRUInteger found = TRInvalidIndex;

    for (TRUInteger index = 0; index < TRComposedLineGetGlyphRunCount(line); index++) {
        TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, index);

        if (TRGlyphRunGetBidiLevel(run) == 1 && TRGlyphRunGetGlyphCount(run) > 0) {
            found = index;
        }
    }

    assert(found != TRInvalidIndex);

    return found;
}

/*
 * The test fonts have no glyph of a script that is written from the right with an image, so this is
 * the nearest to it: the letters are at the right-to-left level, which reverses their order, while
 * the script of them is written from the left.
 */
void DrawTests::testRightToLeftRun() {
    Fixture f(u"\u202Eabc");
    TRComposedLineRef line = f.line(0, 4);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, findRightToLeftRun(line));
    assert(TRGlyphRunGetWritingDirection(run) == TRWritingDirectionLeftToRight);
    assert(TRGlyphRunIsBackward(run));
    assert(TRGlyphRunGetGlyphCount(run) == 3);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRGlyphRunDraw(run, renderer, { 100.0f, 50.0f });

    /* The glyphs go from the left, and the last letter is the first glyph. */
    TRFloat width = TRGlyphRunGetWidth(run);
    assert(near(width, A + B + C));
    assert(TRGlyphRunGetGlyphIDsPtr(run)[0] == 3 && TRGlyphRunGetGlyphIDsPtr(run)[2] == 1);

    vector<Placement> placements = expectedPlacements(run, TRGlyphImageKindFill, 1.0f);
    assert(placements.size() == 3 && recorder.ops.size() == 3);

    for (size_t index = 0; index < 3; index++) {
        assert(recorder.ops[index].origin.x == 100.0f + placements[index].origin.x);
        assert(recorder.ops[index].origin.y == 50.0f + placements[index].origin.y);
        assert(recorder.ops[index].hasClip == false);
    }

    assert(recorder.ops[1].origin.x > recorder.ops[0].origin.x);
    assert(recorder.ops[2].origin.x > recorder.ops[1].origin.x);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

/*
 * The flag of the United States is a cluster of four code units that shows a single glyph, whose
 * image has the size of the type size here. A line that is cut in the middle of the cluster has the
 * glyph, and clips it to the part that belongs to the line.
 */
constexpr TRFloat FlagSize = 109.0f;
constexpr TRFloat FlagAdvance = 135.717773f;

void DrawTests::testSplitClusters() {
    Fixture f(u"\U0001F1FA\U0001F1F8\U0001F1FA\U0001F1F8", "NotoColorEmoji-CBDT.flags.ttf", FlagSize);

    /* The line starts and ends in the middle of a flag. */
    TRComposedLineRef line = f.line(2, 6);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);
    assert(TRGlyphRunGetStartExtraLength(run) == 2 && TRGlyphRunGetEndExtraLength(run) == 2);
    assert(TRGlyphRunGetGlyphCount(run) == 2);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRPoint origin = { 20.0f, 160.0f };
    TRGlyphRunDraw(run, renderer, origin);

    /* Each glyph is drawn once, from where its cluster starts. */
    vector<Placement> placements = expectedPlacements(run, TRGlyphImageKindFill, 1.0f);
    TRFloat width = TRGlyphRunGetWidth(run);
    TRFloat penX = roundPixel(20.0f - FlagAdvance / 2.0f);
    assert(near(width, FlagAdvance));
    assert(placements.size() == 2 && recorder.ops.size() == 2);
    assert(recorder.ops[0].origin.x == penX + placements[0].origin.x);
    assert(recorder.ops[1].origin.x == penX + placements[1].origin.x);

    /* The first one is cut at the start of the run, and the other one at its end. */
    assert(recorder.ops[0].hasClip && recorder.ops[1].hasClip);

    const TRRect &startClip = recorder.ops[0].clip;
    const TRRect &endClip = recorder.ops[1].clip;
    ClipEdges edges = expectedClipEdges(run, origin, 1.0f);
    TRFloat left = roundPixel(20.0f);
    TRFloat right = roundPixel(20.0f + width);

    /* The cut sides are at the edges of the run, and the free sides reach over the ink. */
    assert(startClip.origin.x == left);
    assert(startClip.origin.x + startClip.size.width == edges.farRight);
    assert(endClip.origin.x == edges.farLeft);
    assert(endClip.origin.x + endClip.size.width == right);

    for (const TRRect &clip : { startClip, endClip }) {
        assert(clip.origin.y == edges.top);
        assert(clip.origin.y + clip.size.height == edges.bottom);
    }

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);

    /* A line that is within a cluster has its only glyph clipped on both sides. */
    line = f.line(1, 3);
    run = TRComposedLineGetGlyphRun(line, 0);
    assert(TRGlyphRunGetStartExtraLength(run) == 1 && TRGlyphRunGetEndExtraLength(run) == 1);

    Recorder inside;
    renderer = createRecordingRenderer(&inside, imageCallbacks());
    TRGlyphRunDraw(run, renderer, origin);

    width = TRGlyphRunGetWidth(run);
    assert(inside.ops.size() == 1 && inside.ops[0].hasClip);
    assert(inside.ops[0].clip.origin.x == roundPixel(20.0f));
    assert(inside.ops[0].clip.origin.x + inside.ops[0].clip.size.width == roundPixel(20.0f + width));

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);

    /* A line that holds whole clusters has no clip at all. */
    line = f.line(0, 8);
    Recorder whole;
    renderer = createRecordingRenderer(&whole, imageCallbacks());
    TRComposedLineDraw(line, renderer, origin);
    assert(whole.ops.size() == 2 && !whole.ops[0].hasClip && !whole.ops[1].hasClip);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testSplitClusterOfRightToLeftRun() {
    /* The override makes the flag a right-to-left run, with the letters of the override apart. */
    Fixture f(u"‮\U0001F1FA\U0001F1F8", "NotoColorEmoji-CBDT.flags.ttf", FlagSize);

    /* The line ends in the middle of the flag, whose rest hangs to the left of the run. */
    TRComposedLineRef line = f.line(0, 3);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, findRightToLeftRun(line));
    assert(TRGlyphRunGetEndExtraLength(run) == 2 && TRGlyphRunGetStartExtraLength(run) == 0);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRPoint origin = { 30.0f, 170.0f };
    TRGlyphRunDraw(run, renderer, origin);

    TRFloat spanLeft = distanceOf(run, TRGlyphRunGetCodeUnitEnd(run)
                                       + TRGlyphRunGetEndExtraLength(run));
    assert(spanLeft < 0.0f);
    assert(recorder.ops.size() == 1 && recorder.ops[0].hasClip);

    /* The part that is in the line is the right one, so the clip starts at the cut and goes right. */
    ClipEdges edges = expectedClipEdges(run, origin, 1.0f);
    const TRRect &clip = recorder.ops[0].clip;
    assert(clip.origin.x == roundPixel(30.0f));
    assert(clip.origin.x + clip.size.width == edges.farRight);
    assert(clip.origin.y == edges.top && clip.origin.y + clip.size.height == edges.bottom);

    vector<Placement> placements = expectedPlacements(run, TRGlyphImageKindFill, 1.0f);
    TRFloat penX = roundPixel(30.0f + spanLeft);
    assert(placements.size() == 1);
    assert(recorder.ops[0].origin.x == penX + placements[0].origin.x);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);

    /* The rest of the flag is in the next line, which keeps the part at the left of the cut. */
    line = f.line(3, 5);
    run = TRComposedLineGetGlyphRun(line, findRightToLeftRun(line));
    assert(TRGlyphRunGetStartExtraLength(run) == 2 && TRGlyphRunGetEndExtraLength(run) == 0);

    Recorder next;
    renderer = createRecordingRenderer(&next, imageCallbacks());
    TRGlyphRunDraw(run, renderer, origin);

    TRFloat width = TRGlyphRunGetWidth(run);
    assert(next.ops.size() == 1 && next.ops[0].hasClip);

    /* The start of a right-to-left run is at its right, so the clip goes from there to the left. */
    edges = expectedClipEdges(run, origin, 1.0f);
    const TRRect &nextClip = next.ops[0].clip;
    assert(nextClip.origin.x == edges.farLeft);
    assert(nextClip.origin.x + nextClip.size.width == roundPixel(30.0f + width));

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

/*
 * The letters of the script of a right-to-left language are written from the right, so the pen of
 * the run starts at its right edge and moves to the left, unlike the runs above.
 */
constexpr TRFloat UrduSize = 40.0f;
static const char *const UrduFont = "NotoNastaliqUrdu-Regular.ttf";

void DrawTests::testRightToLeftScript() {
    Fixture f(u"ابر", UrduFont, UrduSize);
    TRComposedLineRef line = f.line(0, 3);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);
    assert(TRComposedLineGetGlyphRunCount(line) == 1);
    assert(TRGlyphRunGetBidiLevel(run) == 1);
    assert(TRGlyphRunGetWritingDirection(run) == TRWritingDirectionRightToLeft);
    assert(!TRGlyphRunIsBackward(run));

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRRendererSetRenderScale(renderer, 2.0f);
    TRGlyphRunDraw(run, renderer, { 10.4f, 50.6f });

    /* The pen starts at the right edge of the run, whose width is the extent of its clusters. */
    TRFloat width = TRGlyphRunGetWidth(run);
    TRFloat penX = roundPixel((10.4f + width) * 2.0f);
    TRFloat penY = roundPixel(50.6f * 2.0f);
    vector<Placement> placements = expectedPlacements(run, TRGlyphImageKindFill, 2.0f);
    assert(!placements.empty() && recorder.ops.size() == placements.size());

    for (size_t index = 0; index < placements.size(); index++) {
        assert(recorder.ops[index].origin.x == penX + placements[index].origin.x);
        assert(recorder.ops[index].origin.y == penY + placements[index].origin.y);
        assert(recorder.ops[index].hasClip == false);
    }

    /* The first letter is the rightmost, and the glyphs go to the left. */
    assert(recorder.ops.front().origin.x > recorder.ops.back().origin.x);
    assert(recorder.ops.front().origin.x <= penX);

    /* The paths follow the same pen, without rounding it. */
    Recorder paths;
    TRDrawCallbacks callbacks = {};
    callbacks.drawGlyphPath = recordPath;
    TRRendererRef pathRenderer = createRecordingRenderer(&paths, callbacks);
    TRGlyphRunDraw(run, pathRenderer, { 10.4f, 50.6f });
    assert(!paths.ops.empty());
    assert(paths.ops.front().origin.x <= (10.4f + width) * 2.0f);
    assert(paths.ops.front().origin.x > paths.ops.back().origin.x);
    TRRendererRelease(pathRenderer);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

/*
 * A letter and its mark are a cluster of two code units, with several glyphs. When a line is cut
 * between them, the line at the right of the cut has the first code unit.
 */
void DrawTests::testSplitClusterOfRightToLeftScript() {
    Fixture f(u"ابَرا", UrduFont, UrduSize);
    TRPoint origin = { 30.0f, 90.0f };

    /* The first line ends between the letter and its mark, which is at the left end of the line. */
    TRComposedLineRef line = f.line(0, 2);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);
    assert(TRGlyphRunGetStartExtraLength(run) == 0 && TRGlyphRunGetEndExtraLength(run) == 1);
    assert(TRGlyphRunGetWritingDirection(run) == TRWritingDirectionRightToLeft);

    Recorder first;
    TRRendererRef renderer = createRecordingRenderer(&first, imageCallbacks());
    TRGlyphRunDraw(run, renderer, origin);

    /* The glyphs of the cluster are the ones that the cluster map gives for its code units. */
    const TRUInteger *clusterMap = TRGlyphRunGetClusterMapPtr(run);
    TRUInteger glyphStart = clusterMap[1];
    TRUInteger glyphEnd = clusterMap[3];
    assert(clusterMap[2] == glyphStart && glyphEnd > glyphStart);

    vector<Placement> placements = expectedPlacements(run, TRGlyphImageKindFill, 1.0f);
    ClipEdges edges = expectedClipEdges(run, origin, 1.0f);
    TRFloat extentRight = max(distanceOf(run, 0), distanceOf(run, 3));
    TRFloat penX = roundPixel(origin.x + extentRight);
    assert(!placements.empty() && first.ops.size() == placements.size());

    size_t clipped = 0;
    for (size_t index = 0; index < placements.size(); index++) {
        bool isInCluster = (placements[index].index >= glyphStart && placements[index].index < glyphEnd);

        assert(first.ops[index].origin.x == penX + placements[index].origin.x);
        assert(first.ops[index].hasClip == isInCluster);

        if (isInCluster) {
            /* The cut is at the left edge of the run, and the clip goes from there to the right. */
            const TRRect &clip = first.ops[index].clip;
            assert(clip.origin.x == roundPixel(origin.x));
            assert(clip.origin.x + clip.size.width == edges.farRight);
            assert(clip.origin.y == edges.top && clip.origin.y + clip.size.height == edges.bottom);
            clipped += 1;
        }
    }
    assert(clipped > 0);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);

    /* The second line starts between them, so the cluster is cut at the right edge of its run. */
    line = f.line(2, 5);
    run = TRComposedLineGetGlyphRun(line, 0);
    assert(TRGlyphRunGetStartExtraLength(run) == 1 && TRGlyphRunGetEndExtraLength(run) == 0);

    Recorder second;
    renderer = createRecordingRenderer(&second, imageCallbacks());
    TRGlyphRunDraw(run, renderer, origin);

    TRFloat width = TRGlyphRunGetWidth(run);
    edges = expectedClipEdges(run, origin, 1.0f);
    placements = expectedPlacements(run, TRGlyphImageKindFill, 1.0f);
    clusterMap = TRGlyphRunGetClusterMapPtr(run);
    assert(!placements.empty() && second.ops.size() == placements.size());

    /* The map of this run starts at the first code unit of the cluster, which is the one before. */
    glyphStart = clusterMap[0];
    glyphEnd = clusterMap[2];
    clipped = 0;
    for (size_t index = 0; index < placements.size(); index++) {
        bool isInCluster = (placements[index].index >= glyphStart && placements[index].index < glyphEnd);

        assert(second.ops[index].hasClip == isInCluster);

        if (isInCluster) {
            const TRRect &clip = second.ops[index].clip;
            assert(clip.origin.x == edges.farLeft);
            assert(clip.origin.x + clip.size.width == roundPixel(origin.x + width));
            clipped += 1;
        }
    }
    assert(clipped > 0);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testDrawStyles() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);
    vector<Placement> fill = expectedPlacements(run, TRGlyphImageKindFill, 1.0f);
    vector<Placement> stroke = expectedPlacements(run, TRGlyphImageKindStroke, 1.0f);
    assert(fill.size() == 3 && stroke.size() == 3);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRRendererSetForegroundColor(renderer, Red);
    TRRendererSetStrokeColor(renderer, Blue);

    /* Fill is the default. */
    TRGlyphRunDraw(run, renderer, { 0.0f, 40.0f });
    assert(recorder.ops.size() == 3);
    assert(recorder.ops[0].color == Red && recorder.ops[0].width == fill[0].width);

    recorder.ops.clear();
    TRRendererSetDrawStyle(renderer, TRDrawStyleStroke);
    TRGlyphRunDraw(run, renderer, { 0.0f, 40.0f });
    assert(recorder.ops.size() == 3);
    for (size_t index = 0; index < 3; index++) {
        assert(recorder.ops[index].color == Blue);
        assert(recorder.ops[index].width == stroke[index].width);
        assert(recorder.ops[index].origin.x == stroke[index].origin.x);
    }

    /* Both are drawn in order: the fill of all the glyphs, and then the stroke of them. */
    recorder.ops.clear();
    TRRendererSetDrawStyle(renderer, TRDrawStyleFillStroke);
    TRGlyphRunDraw(run, renderer, { 0.0f, 40.0f });
    assert(recorder.ops.size() == 6);
    for (size_t index = 0; index < 3; index++) {
        assert(recorder.ops[index].color == Red && recorder.ops[index].width == fill[index].width);
        assert(recorder.ops[index + 3].color == Blue);
        assert(recorder.ops[index + 3].width == stroke[index].width);
    }

    assert(renderer->drawStyle == TRDrawStyleFillStroke);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testPathOutput() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);

    TRDrawCallbacks callbacks = {};
    callbacks.drawGlyphPath = recordPath;

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, callbacks);
    TRRendererSetForegroundColor(renderer, Red);
    TRRendererSetStrokeColor(renderer, Blue);
    TRRendererSetRenderScale(renderer, 2.0f);

    TRGlyphRunDraw(run, renderer, { 10.4f, 50.6f });

    /* The paths are not rounded, and are placed by the advances in pixels. */
    const TRFloat *advances = TRGlyphRunGetGlyphAdvancesPtr(run);
    assert(recorder.ops.size() == 3);
    for (size_t index = 0; index < 3; index++) {
        const Op &op = recorder.ops[index];
        TRFloat advance = 0.0f;
        for (size_t before = 0; before < index; before++) {
            advance += advances[before] * 2.0f;
        }

        assert(op.kind == Op::Path && op.hasPath);
        assert(near(op.origin.x, 10.4f * 2.0f + advance));
        assert(near(op.origin.y, 50.6f * 2.0f));
        assert(op.style == TRDrawStyleFill && op.color == Red && !op.hasClip);
    }

    /* The stroke is told apart by the style. */
    recorder.ops.clear();
    TRRendererSetDrawStyle(renderer, TRDrawStyleFillStroke);
    TRGlyphRunDraw(run, renderer, { 10.4f, 50.6f });
    assert(recorder.ops.size() == 6);
    for (size_t index = 0; index < 3; index++) {
        assert(recorder.ops[index].style == TRDrawStyleFill && recorder.ops[index].color == Red);
        assert(recorder.ops[index + 3].style == TRDrawStyleStroke);
        assert(recorder.ops[index + 3].color == Blue);
        assert(recorder.ops[index + 3].origin.x == recorder.ops[index].origin.x);
    }

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testImagesWinOverPaths() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, allCallbacks());
    TRGlyphRunDraw(run, renderer, { 0.0f, 40.0f });

    assert(filter(recorder.ops, Op::Image).size() == 3);
    assert(filter(recorder.ops, Op::Path).empty());

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testBackgrounds() {
    Fixture f(u"abc");
    f.setColor(1, 1, TRAttributeBackgroundColor, Green);
    TRComposedLineRef line = f.line(0, 3);
    assert(TRComposedLineGetGlyphRunCount(line) == 3);

    assert(readBackground(TRComposedLineGetGlyphRun(line, 0)).isSet == false);
    ColorValue background = readBackground(TRComposedLineGetGlyphRun(line, 1));
    assert(background.isSet == true && background.color == Green);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRComposedLineDraw(line, renderer, { 5.0f, 20.0f });

    /* Each run draws its glyphs, and the background of the second one is drawn before its own. */
    assert(recorder.ops.size() == 4);
    assert(recorder.ops[0].kind == Op::Image);
    assert(recorder.ops[1].kind == Op::Rect);
    assert(recorder.ops[2].kind == Op::Image);
    assert(recorder.ops[3].kind == Op::Image);

    /* It covers the extent of the run, from the top of the line to its bottom. */
    const Op &backgroundOp = recorder.ops[1];
    TRFloat top = 20.0f + TRComposedLineGetTop(line);
    TRFloat bottom = 20.0f + TRComposedLineGetBottom(line);
    assert(backgroundOp.color == Green);
    assert(backgroundOp.rect.origin.x == roundPixel(5.0f + A));
    assert(backgroundOp.rect.origin.x + backgroundOp.rect.size.width == roundPixel(5.0f + A + B));
    assert(backgroundOp.rect.origin.y == roundPixel(top));
    assert(backgroundOp.rect.origin.y + backgroundOp.rect.size.height == roundPixel(bottom));

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testBackgroundsFillTheLine() {
    Fixture f(u"abc");
    f.setColor(0, 1, TRAttributeBackgroundColor, Green);
    setTestFloat(f.text, 2, 1, TRAttributeTypeSize, 2.0f * TypeSize);
    TRComposedLineRef line = f.line(0, 3);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRComposedLineDraw(line, renderer, { 0.0f, 300.0f });

    /* The background is as tall as the line, which the bigger run makes taller than its own run. */
    vector<Op> rects = filter(recorder.ops, Op::Rect);
    assert(rects.size() == 1);
    assert(TRComposedLineGetHeight(line) > TRGlyphRunGetHeight(run));
    assert(rects[0].rect.size.height == roundPixel(300.0f + TRComposedLineGetBottom(line))
                                      - roundPixel(300.0f + TRComposedLineGetTop(line)));

    /* A run drawn alone uses its own metrics. */
    recorder.ops.clear();
    TRGlyphRunDraw(run, renderer, { 0.0f, 300.0f });
    rects = filter(recorder.ops, Op::Rect);
    assert(rects.size() == 1);
    assert(rects[0].rect.origin.y == roundPixel(300.0f - TRGlyphRunGetAscent(run)));
    assert(rects[0].rect.origin.y + rects[0].rect.size.height
           == roundPixel(300.0f + TRGlyphRunGetDescent(run) + TRGlyphRunGetLeading(run)));

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testUnderlineAndStrikethrough() {
    Fixture f(u"abc");
    f.setFlag(1, 1, TRAttributeUnderline);
    f.setFlag(1, 1, TRAttributeStrikethrough);
    TRComposedLineRef line = f.line(0, 3);
    assert(TRComposedLineGetGlyphRunCount(line) == 3);

    TRGlyphRunRef plain = TRComposedLineGetGlyphRun(line, 0);
    TRGlyphRunRef decorated = TRComposedLineGetGlyphRun(line, 1);
    assert(!TRGlyphRunHasUnderline(plain) && !TRGlyphRunHasStrikethrough(plain));
    assert(TRGlyphRunHasUnderline(decorated) && TRGlyphRunHasStrikethrough(decorated));

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRRendererSetForegroundColor(renderer, Red);
    TRComposedLineDraw(line, renderer, { 5.0f, 90.0f });

    /* The decorations are drawn after the glyphs of their run, the underline first. */
    assert(recorder.ops.size() == 5);
    assert(recorder.ops[0].kind == Op::Image);
    assert(recorder.ops[1].kind == Op::Image);
    assert(recorder.ops[2].kind == Op::Rect);
    assert(recorder.ops[3].kind == Op::Rect);
    assert(recorder.ops[4].kind == Op::Image);

    /* Their bands come from the metrics of the typeface, as far from the baseline as they say. */
    TRTypefaceRef typeface = f.typeface;
    TRFloat unitScale = TypeSize / (TRFloat)TRTypefaceGetUnitsPerEM(typeface);
    TRFloat underlineTop = 90.0f - TRTypefaceGetUnderlinePosition(typeface) * unitScale;
    TRFloat underlineBottom = underlineTop + TRTypefaceGetUnderlineThickness(typeface) * unitScale;
    TRFloat strikeTop = 90.0f - TRTypefaceGetStrikeoutPosition(typeface) * unitScale;
    TRFloat strikeBottom = strikeTop + TRTypefaceGetStrikeoutThickness(typeface) * unitScale;
    TRFloat left = roundPixel(5.0f + A);
    TRFloat right = roundPixel(5.0f + A + B);

    const TRRect &underline = recorder.ops[2].rect;
    assert(underline.origin.x == left && underline.origin.x + underline.size.width == right);
    assert(underline.origin.y == roundPixel(underlineTop));
    assert(underline.size.height == max(1.0f, roundPixel(underlineBottom) - roundPixel(underlineTop)));

    const TRRect &strikethrough = recorder.ops[3].rect;
    assert(strikethrough.origin.x == left && strikethrough.origin.x + strikethrough.size.width == right);
    assert(strikethrough.origin.y == roundPixel(strikeTop));
    assert(strikethrough.size.height == max(1.0f, roundPixel(strikeBottom) - roundPixel(strikeTop)));

    /* The underline is below the baseline and the strikethrough is above it. */
    assert(underline.origin.y >= 90.0f);
    assert(strikethrough.origin.y + strikethrough.size.height <= 90.0f);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testDecorationColors() {
    Fixture f(u"abc");
    f.setFlag(0, 1, TRAttributeUnderline);
    f.setFlag(1, 1, TRAttributeUnderline);
    f.setFlag(2, 1, TRAttributeUnderline);
    f.setColor(1, 2, TRAttributeForegroundColor, Green);
    f.setColor(2, 1, TRAttributeDecorationColor, Blue);
    TRComposedLineRef line = f.line(0, 3);
    assert(TRComposedLineGetGlyphRunCount(line) == 3);

    assert(readDecoration(TRComposedLineGetGlyphRun(line, 1)).isSet == false);
    ColorValue decoration = readDecoration(TRComposedLineGetGlyphRun(line, 2));
    assert(decoration.isSet == true && decoration.color == Blue);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRRendererSetForegroundColor(renderer, Red);
    TRComposedLineDraw(line, renderer, { 0.0f, 90.0f });

    /* The colors are those of the decoration, the run, and the renderer, in this order. */
    vector<Op> rects = filter(recorder.ops, Op::Rect);
    assert(rects.size() == 3);
    assert(rects[0].color == Red);
    assert(rects[1].color == Green);
    assert(rects[2].color == Blue);

    /* The glyph of the last run is still drawn with the foreground color. */
    assert(filter(recorder.ops, Op::Image).back().color == Green);

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testDecorationsOfSplitClusters() {
    Fixture f(u"a\U0001F600" "b");
    f.setFlag(0, 4, TRAttributeUnderline);
    f.setColor(0, 4, TRAttributeBackgroundColor, Green);
    TRComposedLineRef line = f.line(2, 4);
    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 0);
    assert(TRGlyphRunGetStartExtraLength(run) == 1);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, imageCallbacks());
    TRGlyphRunDraw(run, renderer, { 20.0f, 60.0f });

    /* The background and the underline only span the part of the run that is in the line. */
    vector<Op> rects = filter(recorder.ops, Op::Rect);
    assert(rects.size() == 2);

    TRFloat width = TRGlyphRunGetWidth(run);
    assert(near(width, Notdef / 2.0f + B));

    for (const Op &rect : rects) {
        assert(rect.rect.origin.x == roundPixel(20.0f));
        assert(rect.rect.origin.x + rect.rect.size.width == roundPixel(20.0f + width));
    }

    /* The background is drawn before the glyphs, and the underline after them. */
    assert(recorder.ops.front().kind == Op::Rect && recorder.ops.back().kind == Op::Rect);
    TRFloat top = 60.0f - TRGlyphRunGetAscent(run);
    TRFloat bottom = 60.0f + TRGlyphRunGetDescent(run) + TRGlyphRunGetLeading(run);
    assert(rects[0].rect.origin.y == roundPixel(top));
    assert(rects[0].rect.size.height == roundPixel(bottom) - roundPixel(top));

    TRFloat underlineTop = 60.0f - TRTypefaceGetUnderlinePosition(f.typeface) * UnitScale;
    TRFloat underlineBottom = underlineTop + TRTypefaceGetUnderlineThickness(f.typeface) * UnitScale;
    assert(rects[1].rect.origin.y == roundPixel(underlineTop));
    assert(rects[1].rect.size.height == max(1.0f, roundPixel(underlineBottom) - roundPixel(underlineTop)));

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testReplacement() {
    Fixture f(u"a\uFFFCb");
    TRFloat extent = 40.0f;
    TRReplacementCallbacks replacementCallbacks = { computeRoom, nullptr };
    TRReplacementRef replacement = TRReplacementCreate(&replacementCallbacks, &extent,
        TRReplacementKindInline);

    TRAttribute attribute = {};
    attribute.type = TRAttributeReplacement;
    attribute.value.replacement = replacement;
    TRTextSetAttribute(f.text, 1, 1, &attribute);

    f.setColor(1, 1, TRAttributeBackgroundColor, Green);
    f.setFlag(1, 1, TRAttributeUnderline);
    TRComposedLineRef line = f.line(0, 3);
    assert(TRComposedLineGetGlyphRunCount(line) == 3);

    Recorder recorder;
    TRRendererRef renderer = createRecordingRenderer(&recorder, allCallbacks());
    TRComposedLineDraw(line, renderer, { 10.0f, 80.0f });

    /* The replacement run has the background and the replacement, and no glyphs or decorations. */
    assert(recorder.ops.size() == 4);
    assert(recorder.ops[0].kind == Op::Image);
    assert(recorder.ops[1].kind == Op::Rect && recorder.ops[1].color == Green);
    assert(recorder.ops[2].kind == Op::Replacement);
    assert(recorder.ops[3].kind == Op::Image);

    TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, 1);
    assert(recorder.ops[2].run == run);
    assert(TRGlyphRunGetReplacement(recorder.ops[2].run) == replacement);
    assert(recorder.ops[2].origin.x == roundPixel(10.0f + A));
    assert(recorder.ops[2].origin.y == 80.0f);
    assert(recorder.ops[1].rect.size.width == roundPixel(10.0f + A + 40.0f) - roundPixel(10.0f + A));

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
    TRReplacementRelease(replacement);
}

void DrawTests::testLineOrigins() {
    Fixture f(u"abc");
    f.setColor(1, 1, TRAttributeBackgroundColor, Green);
    f.setFlag(2, 1, TRAttributeUnderline);
    TRComposedLineRef line = f.line(0, 3);

    /* A line draws its runs where they are in it, from the origin of its container. */
    Recorder lineRecorder;
    TRRendererRef renderer = createRecordingRenderer(&lineRecorder, allCallbacks());
    TRPoint container = { 7.0f, 9.0f };
    TRComposedLineDraw(line, renderer, container);

    Recorder runRecorder;
    TRDrawCallbacks callbacks = allCallbacks();
    TRRendererSetDrawCallbacks(renderer, &callbacks, &runRecorder);
    runRecorder.renderer = renderer;

    TRPoint lineOrigin = TRComposedLineGetOrigin(line);
    for (TRUInteger index = 0; index < TRComposedLineGetGlyphRunCount(line); index++) {
        TRGlyphRunRef run = TRComposedLineGetGlyphRun(line, index);
        TRPoint runOrigin = TRGlyphRunGetOrigin(run);
        TRPoint baseline = { container.x + lineOrigin.x + runOrigin.x,
                             container.y + lineOrigin.y + runOrigin.y };

        /* The background of a run drawn alone is as tall as the run, which is the line here. */
        TRGlyphRunDraw(run, renderer, baseline);
    }

    assert(!lineRecorder.ops.empty());
    assert(sameOps(lineRecorder.ops, runRecorder.ops));

    TRRendererRelease(renderer);
    TRComposedLineRelease(line);
}

void DrawTests::testFrameOrigins() {
    Fixture f(u"abc abc abc");
    TRComposedFrameRef frame = f.frame(A + B + C + 5.0f);
    TRUInteger lineCount = TRComposedFrameGetLineCount(frame);
    assert(lineCount > 1);

    Recorder frameRecorder;
    TRRendererRef renderer = createRecordingRenderer(&frameRecorder, imageCallbacks());
    TRPoint frameOrigin = { 12.0f, 34.0f };
    TRDrawCallbacks callbacks = imageCallbacks();
    TRComposedFrameDraw(frame, renderer, frameOrigin);

    /* The frame draws its lines one after another, with its origin as theirs. */
    Recorder lineRecorder;
    TRRendererSetDrawCallbacks(renderer, &callbacks, &lineRecorder);
    lineRecorder.renderer = renderer;

    for (TRUInteger index = 0; index < lineCount; index++) {
        TRComposedLineDraw(TRComposedFrameGetLine(frame, index), renderer, frameOrigin);
    }

    assert(!frameRecorder.ops.empty());
    assert(sameOps(frameRecorder.ops, lineRecorder.ops));

    /* The glyphs of a later line are lower, by the origin of the line. */
    TRComposedLineRef second = TRComposedFrameGetLine(frame, 1);
    assert(TRComposedLineGetOrigin(second).y > TRComposedLineGetOrigin(TRComposedFrameGetLine(frame, 0)).y);

    Recorder runRecorder;
    TRRendererSetDrawCallbacks(renderer, &callbacks, &runRecorder);
    runRecorder.renderer = renderer;

    TRGlyphRunRef run = TRComposedLineGetGlyphRun(second, 0);
    TRPoint baseline = { frameOrigin.x + TRComposedLineGetOrigin(second).x + TRGlyphRunGetOrigin(run).x,
                         frameOrigin.y + TRComposedLineGetOrigin(second).y + TRGlyphRunGetOrigin(run).y };
    TRGlyphRunDraw(run, renderer, baseline);
    assert(!runRecorder.ops.empty());

    bool isFound = false;
    for (const Op &op : frameRecorder.ops) {
        isFound = isFound || (op.origin.y == runRecorder.ops[0].origin.y
                              && op.origin.x == runRecorder.ops[0].origin.x);
    }
    assert(isFound);

    /* The frame is drawn again with another origin by moving everything with it. */
    Recorder moved;
    TRRendererSetDrawCallbacks(renderer, &callbacks, &moved);
    moved.renderer = renderer;
    TRComposedFrameDraw(frame, renderer, { frameOrigin.x + 100.0f, frameOrigin.y + 200.0f });
    assert(moved.ops.size() == frameRecorder.ops.size());
    assert(moved.ops[0].origin.x == frameRecorder.ops[0].origin.x + 100.0f);
    assert(moved.ops[0].origin.y == frameRecorder.ops[0].origin.y + 200.0f);

    TRRendererRelease(renderer);
    TRComposedFrameRelease(frame);
}

void DrawTests::testMissingCallbacks() {
    Fixture f(u"abc");
    f.setColor(0, 3, TRAttributeBackgroundColor, Green);
    f.setFlag(0, 3, TRAttributeUnderline);
    TRComposedLineRef line = f.line(0, 3);

    /* A renderer without callbacks draws nothing. */
    Recorder none;
    TRRendererRef renderer = createRecordingRenderer(&none, TRDrawCallbacks());
    TRRendererSetDrawCallbacks(renderer, nullptr, nullptr);
    TRComposedLineDraw(line, renderer, { 0.0f, 50.0f });
    assert(none.ops.empty());
    TRRendererRelease(renderer);

    /* Only the rectangles are drawn when there is no function for the glyphs. */
    Recorder rects;
    TRDrawCallbacks callbacks = {};
    callbacks.fillRect = recordRect;
    renderer = createRecordingRenderer(&rects, callbacks);
    TRComposedLineDraw(line, renderer, { 0.0f, 50.0f });
    assert(rects.ops.size() == 2);
    assert(rects.ops[0].kind == Op::Rect && rects.ops[1].kind == Op::Rect);
    TRRendererRelease(renderer);

    /* And the other way round: no rectangles for the backgrounds and the decorations. */
    Recorder images;
    callbacks = {};
    callbacks.drawGlyphImage = recordImage;
    renderer = createRecordingRenderer(&images, callbacks);
    TRComposedLineDraw(line, renderer, { 0.0f, 50.0f });
    assert(images.ops.size() == 3);
    assert(filter(images.ops, Op::Image).size() == 3);
    TRRendererRelease(renderer);

    TRComposedLineRelease(line);
}

void DrawTests::testMissingRenderer() {
    Fixture f(u"abc");
    TRComposedLineRef line = f.line(0, 3);
    TRComposedFrameRef frame = f.frame(1000.0f);

    /* Nothing happens, and nothing is crashed. */
    TRGlyphRunDraw(TRComposedLineGetGlyphRun(line, 0), nullptr, { 0.0f, 0.0f });
    TRComposedLineDraw(line, nullptr, { 0.0f, 0.0f });
    TRComposedFrameDraw(frame, nullptr, { 0.0f, 0.0f });

    TRComposedFrameRelease(frame);
    TRComposedLineRelease(line);
}

#ifdef STANDALONE_TESTING

int main() {
    DrawTests tests;
    tests.run();

    return 0;
}

#endif
