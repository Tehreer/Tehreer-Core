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

#ifndef _TEHREER_TEXT_ATTRIBUTE_REGISTRY_H
#define _TEHREER_TEXT_ATTRIBUTE_REGISTRY_H

#include <SheenBidi/SBAttributeInfo.h>
#include <SheenBidi/SBAttributeRegistry.h>
#include <SheenBidi/SBTextConfig.h>

#include <API/TRBase.h>
#include <Tehreer/TRAttribute.h>

/*
 * The attributes that decide how text is shaped are in a group of their own, so that a change of
 * the other ones, such as the foreground color, does not split the runs that are shaped.
 */
#define AttributeGroupShaping   ((SBAttributeGroup)1)

typedef struct _AttributeRegistry {
    SBAttributeRegistryRef _registry;
} AttributeRegistry, *AttributeRegistryRef;

/**
 * Returns the SheenBidi attribute ID registered for the given Tehreer attribute type, creating the
 * process-wide attribute registry on first use. Returns `SBAttributeIDNone` for an unknown type.
 */
TR_INTERNAL SBAttributeID AttributeRegistryGetAttributeID(TRAttributeType type);

/**
 * Returns the process-wide text config that uses the attribute registry, creating both on first
 * use. Returns `NULL` if the config could not be created.
 */
TR_INTERNAL SBTextConfigRef AttributeRegistryGetDefaultConfig(void);

#endif
