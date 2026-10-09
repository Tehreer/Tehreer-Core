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

#ifndef _TEHREER__TEST_PATH_H
#define _TEHREER__TEST_PATH_H

#include <vector>

#include <Tehreer/TRGeometry.h>
#include <Tehreer/TRPath.h>

namespace Tehreer {

struct PathEvent {
    enum Kind { Move, Line, Quad, Cubic, Close };

    Kind kind;
    std::vector<TRPoint> points;
};

inline std::vector<PathEvent> enumeratePath(TRPathRef path,
    const TRAffineTransform *transform = nullptr) {
    using Events = std::vector<PathEvent>;

    TRPathCallbacks callbacks = {};
    callbacks.moveTo = [](void *data, TRFloat x, TRFloat y, TRBoolean *) {
        static_cast<Events *>(data)->push_back({ PathEvent::Move, { { x, y } } });
    };
    callbacks.lineTo = [](void *data, TRFloat x, TRFloat y, TRBoolean *) {
        static_cast<Events *>(data)->push_back({ PathEvent::Line, { { x, y } } });
    };
    callbacks.quadTo = [](void *data, TRFloat cx, TRFloat cy, TRFloat x, TRFloat y, TRBoolean *) {
        static_cast<Events *>(data)->push_back({ PathEvent::Quad, { { cx, cy }, { x, y } } });
    };
    callbacks.cubicTo = [](void *data, TRFloat c1x, TRFloat c1y, TRFloat c2x, TRFloat c2y,
        TRFloat x, TRFloat y, TRBoolean *) {
        static_cast<Events *>(data)->push_back({ PathEvent::Cubic,
            { { c1x, c1y }, { c2x, c2y }, { x, y } } });
    };
    callbacks.close = [](void *data, TRBoolean *) {
        static_cast<Events *>(data)->push_back({ PathEvent::Close, {} });
    };

    Events events;
    TRPathEnumerate(path, transform, &callbacks, &events);

    return events;
}

}

#endif
