//
// SPDX-FileCopyrightText: The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#ifndef MTK_C2_STORE_H
#define MTK_C2_STORE_H

#include <C2Component.h>

namespace android {
std::shared_ptr<C2ComponentStore> GetCodec2MtkComponentStore();
} // namespace android

#endif // MTK_C2_STORE_H
