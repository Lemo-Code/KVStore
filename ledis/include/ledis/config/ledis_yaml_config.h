#pragma once

#include "ledis/config/ledis_settings.h"

namespace ledis {

/** 从 YAML 文件加载 LedisSettings；未出现的字段保持 settings 原值。 */
bool LoadLedisSettingsFromYamlFile(const String& path, LedisSettings* settings);

}  // namespace ledis
