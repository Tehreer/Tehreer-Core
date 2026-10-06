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
#include <API/TRFontFile.c>
#include <API/TRText.c>
#include <API/TRTypeface.c>
#include <Core/Allocator.c>
#include <Core/Memory.c>
#include <Core/NameWriter.c>
#include <Core/Object.c>
#include <Core/Once.c>
#include <Font/FaceMetadata.c>
#include <Graphics/FreeType.c>
#include <Graphics/GlyphBitmap.c>
#include <Graphics/RenderableFace.c>
#include <Graphics/ShapableFace.c>
#include <SFNT/Utilities.c>
#include <Text/AttributeRegistry.c>

#endif
