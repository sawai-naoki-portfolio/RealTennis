// TennisSim のログのカテゴリ（ARCH-03 §5.1）。
#pragma once

#include "Logging/LogMacros.h"

TENNISSIM_API DECLARE_LOG_CATEGORY_EXTERN(LogTennisBall, Log, All);  // 打ち出し・バウンド・ネット
TENNISSIM_API DECLARE_LOG_CATEGORY_EXTERN(LogTennisShot, Log, All);    // ソルバの結果・品質・誤差
TENNISSIM_API DECLARE_LOG_CATEGORY_EXTERN(LogTennisStroke, Log, All);  // 打点の探索・ストロークの選択・インパクト
