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

#include <Tehreer/Tehreer.h>

#ifdef TR_CONFIG_UNITY

#include <API/TRAttributeList.c>
#include <API/TRComposedFrame.c>
#include <API/TRComposedLine.c>
#include <API/TRFontFeatures.c>
#include <API/TRFontFile.c>
#include <API/TRFrameResolver.c>
#include <API/TRGlyphCache.c>
#include <API/TRGlyphImage.c>
#include <API/TRGlyphRun.c>
#include <API/TRPath.c>
#include <API/TRRenderer.c>
#include <API/TRReplacement.c>
#include <API/TRShapingEngine.c>
#include <API/TRShapingResult.c>
#include <API/TRText.c>
#include <API/TRTypesetter.c>
#include <API/TRTypeface.c>
#include <Core/Allocator.c>
#include <Core/Array.c>
#include <Core/Memory.c>
#include <Core/NameWriter.c>
#include <Core/Object.c>
#include <Core/Once.c>
#include <Font/FaceMetadata.c>
#include <Font/FontData.c>
#include <Graphics/AdvanceCache.c>
#include <Graphics/FreeType.c>
#include <Graphics/GlyphBitmap.c>
#include <Graphics/RenderableFace.c>
#include <Graphics/ShapableFace.c>
#include <Layout/BreakResolver.c>
#include <Layout/CaretUtils.c>
#include <Layout/LineResolver.c>
#include <Layout/ShapeResolver.c>
#include <Layout/TokenResolver.c>
#include <Layout/TextBuffer.c>
#include <Layout/TextRun.c>
#include <SFNT/Utilities.c>
#include <Text/AttributeRegistry.c>
#include <Text/BreakClassifier.c>
#include <Text/CaretEdgesBuilder.c>

#endif
