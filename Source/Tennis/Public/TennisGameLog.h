// Tennis（ゲーム本体）のログのカテゴリ（ARCH-03 §5.1）。シミュレーションのカテゴリは TennisSimLog.h。
#pragma once

#include "Logging/LogMacros.h"

TENNIS_API DECLARE_LOG_CATEGORY_EXTERN(LogTennisMove, Log, All);   // 選手の移動・範囲
TENNIS_API DECLARE_LOG_CATEGORY_EXTERN(LogTennisInput, Log, All);  // 操作：球種の予約・離す・取り消し、Context の切り替え
TENNIS_API DECLARE_LOG_CATEGORY_EXTERN(LogTennisCamera, Log, All); // カメラ：モードの始まり、追う選手・コートがない
