# 調査報告 06：UE 5.8 のストロークとロコモーションの API（Chooser・Motion Warping・モンタージュ・PoseSearch）

| 項目 | 内容 |
|---|---|
| 文書 ID | RES-06 |
| 調査日 | 2026-11-12 |
| 調査方法 | (1) インストール済みエンジン（`C:\Program Files\Epic Games\UE_5.8`、5.8.4）の `.uplugin`・`Build.cs`・ヘッダー・実装（`.cpp`）を直接読んだ。表の「場所」は、各節のプラグイン（またはエンジン本体）のフォルダからの短いパス。`Engine/` からの全体のパスは §8。(2) Epic の文書を Web で確認した（エンジンのソースと食い違う場合はソースを正とする）。(3) Unreal MCP・エディタ・ビルドは使っていない。確かめられなかったことは **未確認** と書く |
| 対象の作業 | W-3.4（Chooser Table・`UStrokeData`・`StrokeMetaBaker`）、W-3.5（再生速度・Motion Warping・インパクトの判定）、W-3.6（Motion Matching のロコモーション、仮のデータ） |
| 関連 | [RES-04](04_UE58_Technology_Assessment.md)（プラグインの成熟度と採用判断）、[DD-03](../04_DetailedDesign/03_Player_Movement_Stroke_Detail.md) §5・§6、[ART-02](../07_Art_Animation/02_Animation_List.md) §3・§5 |

---

## 1. 要約（設計に効くもの）

1. **root motion のモンタージュの再生中は、CMC が `CalcVelocity` を呼ばない**。ストローク中は `TennisMovement::Step` が効かず（今の `UTennisMovementComponent::CalcVelocity` も root motion の間は CMC の既定に戻す）、体の位置はアニメの root motion とワープが動かす（§6 G-1）
2. **Skew Warp は「区間の終わり（EndTime）」にルートを目標へ置く**。DD-03 の区間はインパクトの 0.05 s 前に終わるので、目標の式に「区間の終わり → インパクト」の root motion を足す必要がある（G-2）
3. **スイートスポットは骨格のソケットではない**（`SM_Racket` のソケット）。`StrokeMetaBaker` は手のソケット・ラケットの `grip`・`racket_sweetspot` を合成し、メッシュの −90° の回転も入れる（G-3）
4. **Motion Matching の軌跡の予測は CMC 自身の計算をまねる**。`TennisMovement` とは加減速の式が違い、向きはコントローラーの向きから作る（G-4）
5. Chooser は C++ から構造体を入力にして評価でき、結果に `UStrokeData` を返せる。T-STRK-002 は全組み合わせを総当たりで評価するテストで書ける（§2.5）

## 2. Chooser（W-3.4）

### 2.1 プラグインとモジュール

| 項目 | 内容 | 場所 |
|---|---|---|
| プラグイン | `Chooser`（正式版。Beta・Experimental の印なし、既定で無効）。依存：`GameplayTagsEditor`・`BlendStack`・`GameplayInsights` | `Plugins/Chooser/Chooser.uplugin` |
| モジュール | 実行時 `Chooser`。ほかに `ChooserUncooked`（UncookedOnly）・`ChooserEditor`（Editor）・`ProxyTable` 系 | 同上 |
| Build.cs | `Tennis`：`PrivateDependencyModuleNames` に `"Chooser"`。T-STRK-002 のテストを書く `TennisTests` にも `"Chooser"` | `Plugins/Chooser/Source/Chooser/Chooser.Build.cs` |

### 2.2 C++ から評価する

| API | 内容 | 場所 |
|---|---|---|
| `UChooserTable::EvaluateChooser(FChooserEvaluationContext&, const UChooserTable*, FObjectChooserIteratorCallback)` | 本体（static、`CHOOSER_API`）。全列のフィルタを通った行を順に試し、コールバックが `Stop` を返したら終わる | `Chooser/Public/Chooser.h`、`Private/Chooser.cpp` |
| `UChooserFunctionLibrary::EvaluateChooser(const UObject* ContextObject, const UChooserTable*, TSubclassOf<UObject>)` | 入力が UObject 1 つのときの簡易版。結果の型が合わない行は `Failed` として次の行へ進む | `Chooser/Public/ChooserFunctionLibrary.h` |
| `FChooserEvaluationContext::AddStructParam(T&)`・`AddObjectParam(UObject*)` | 入力を足す。構造体は参照で持つので、評価が終わるまで生かす。**足す順番は、表の Parameters（`ContextData`）の順番と同じにする**（列の束縛は `ContextIndex` で引く） | `Chooser/Public/IObjectChooser.h`、`ChooserPropertyAccess.h` |
| `FChooserEvaluationContext::RandomStream` | `FRandomStream*`。Randomize の列は、これが null だと `FMath::FRandRange` を使う | `Private/RandomizeColumn.cpp` |

```cpp
// 入力（Tennis/Animation/TennisStrokeChooserTypes.h の想定）。列の束縛は Blueprint から見えるプロパティだけ（SPropertyAccessChainWidget.cpp：CPF_BlueprintVisible）
USTRUCT(BlueprintType)
struct FTennisStrokeChooserInput
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke") EStrokeSide Side = EStrokeSide::Forehand;  // UENUM(BlueprintType) にしておく（G-7）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke") EStrokeHeightBand Height = EStrokeHeightBand::Mid;
    // BackhandStyle・Reach・Family・Stance も同じ
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stroke") float TimeBudget = 0.f;  // s。FloatRange の列
};

UStrokeData* SelectStroke(const UChooserTable* Table, FTennisStrokeChooserInput& In, FRandomStream& Rng)
{
    FChooserEvaluationContext Context;
    Context.AddStructParam(In);   // ContextData[0] が FTennisStrokeChooserInput の Struct Parameter
    Context.RandomStream = &Rng;  // Randomize の列（変種の選択）を決定論にする
    UStrokeData* Result = nullptr;
    UChooserTable::EvaluateChooser(Context, Table, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda(
        [&Result](UObject* Obj)
        {
            Result = Cast<UStrokeData>(Obj);
            return Result ? FObjectChooserBase::EIteratorStatus::Stop : FObjectChooserBase::EIteratorStatus::Failed;
        }));
    return Result;  // null なら警告を出し、既定のストロークに戻す（開発ルール集（非公開） §5 堅牢性）
}
```

### 2.3 結果に `UStrokeData` を返す

- 表の Result Type を「Object Of Type」、Result Class（`UChooserSignature::OutputObjectType`）を `UStrokeData` にする。行の結果は `FAssetChooser`（ハードの参照）か `FSoftAssetChooser`（ソフトの参照）。`UDataAsset` の派生も UObject なので返せる（`Public/ObjectChooser_Asset.h`）
- **ソフトの参照は使わない**：UObject のコールバックで評価すると `TSoftObjectPtr::Get()` になり、読み込まれていなければ null が返る（`Chooser.cpp` の `EvaluateChooser` の UObject 版、`ObjectChooser_Asset.cpp`）。111 本でも `UStrokeData` は小さいので、ハードの参照でよい（モンタージュを `UStrokeData` からどう参照するかは別に決める）

### 2.4 列の動き（設計に関係するもの）

| 項目 | 内容 | 場所 |
|---|---|---|
| Enum の列 | 行ごとに `MatchEqual`・`MatchNotEqual`・`MatchAny`。値は `uint8` で読む。DD-03 の「Any」はこれで書ける | `Public/EnumColumn.h` |
| Float Range の列 | 行ごとに `Min`・`Max`（`float`）と `bNoMin`・`bNoMax`。**両端を含む**（`>=` と `<=`） | `Public/FloatRangeColumn.h`、`Private/FloatRangeColumn.cpp` |
| 行の順番 | コスト（Pose Match などの列）がなければ、条件を通った行のうち**上の行が先**。「最後の行」は表の下に置けば、それより上で決まらないときだけ選ばれる | `Chooser.cpp`（`EvaluateChooser`） |
| フォールバック | `UChooserTable::FallbackResult`：どの行も通らないときの結果（エディタの表の「Fallback」の行）。設定しなければ null | `Public/Chooser.h` |
| 無効な行・列 | エディタで無効にした行・列は、未クックのデータでは飛ばす（`IsRowDisabled`・`bDisabled`） | 同上 |

### 2.5 T-STRK-002（全組み合わせに結果がある）のテスト

- エディタで動くテスト（`TennisTests`）で、表を読み込み、入力の全組み合わせを総当たりで評価する。列挙の値は `StaticEnum<…>()->NumEnums() - 1` で回し、`TimeBudget` は各行の境界（`Min`・`Max` とその前後）を使う。2（サイド）× 2（バックハンド）× 5（高さ）× 4（到達）× 8（球種）× 4（スタンス）＝ 2,560 通り × 時間の点の数で、1 回の評価は軽い
- **フォールバックで通ってしまわないようにする**：`FallbackResult` を設定するなら、テストはそれに落ちた組み合わせを数える。方法は (a) 結果が `FallbackResult` の資産（`FallbackResult.GetPtr<FAssetChooser>()->Asset`）と同じか比べる、(b) エディタでは評価の後の `GetDebugSelectedRows()` が `ChooserColumn_SpecialIndex_Fallback`（−2）かを見る（`Public/IChooserColumn.h`。デバッグ用の値なので (a) を優先）
- 結果の型（`UStrokeData`）と、結果の `UStrokeData` の Montage が null でないことも同じテストで見る

## 3. Motion Warping（W-3.5）

### 3.1 プラグインと状態

| 項目 | 内容 | 場所 |
|---|---|---|
| プラグイン | `MotionWarping`、**`IsBetaVersion: true`**（VersionName 0.1、既定で無効） | `Plugins/Animation/MotionWarping/MotionWarping.uplugin` |
| モジュール | 実行時 `MotionWarping`（公開の依存 `Core`・`CoreUObject`・`Engine`・`NetCore`） | `Source/MotionWarping/MotionWarping.Build.cs` |
| 5.8 で Experimental の印 | Blueprint 版の `FindWarpTarget(FName, FMotionWarpingTarget&, bool&)`、軌跡の予測（`PredictionAdapter`・`BeginTrajectoryPredictions`）、Switch Off Condition、`EWarpTargetLocationOffsetDirection`、`URootMotionModifier_Blueprintable`。下で使うものには印がない | `Public/MotionWarpingComponent.h`、`Public/RootMotionModifier.h` |
| 複製 | 目標（`WarpTargets`）は `Replicated`。コンポーネントは既定で複製する（`SetIsReplicatedByDefault(true)`） | `Private/MotionWarpingComponent.cpp` |

### 3.2 目標 `HitPoint` を足す・更新する

```cpp
// UTennisStrokeComponent::PlayAndWarp() の中だけで呼ぶ（DD-03 §6.2）。位置はワールドの cm
MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(TEXT("HitPoint"), TargetLocationCm, TargetRotation);
// または AddOrUpdateWarpTargetFromTransform(TEXT("HitPoint"), FTransform(Rot, Loc))。
// 0.05 s ごとの読み直しも同じ関数。終わったら RemoveWarpTarget(TEXT("HitPoint"))
```

- 区間の途中で目標が変わると、`URootMotionModifier_Warp::OnTargetTransformChanged` がその時点の位置を新しい起点にして、残りの区間で目標へ寄せ直す（`Private/RootMotionModifier.cpp`）。DD-03 §6.1 の読み直しはそのまま使える
- 目標がない区間は、修飾子を `Disabled` にしてワープしない（ログは Verbose）

### 3.3 区間（NotifyState）の決まり方

| 項目 | 内容 | 場所 |
|---|---|---|
| 区間 | モンタージュの `UAnimNotifyState_MotionWarping`。中の `RootMotionModifier`（Instanced）に Skew Warp を選び、`WarpTargetName = HitPoint` | `Public/AnimNotifyState_MotionWarping.h` |
| 探す場所 | **モンタージュ自身の Notify だけ**。モンタージュの中のシーケンスの Notify は `bSearchForWindowsInAnimsWithinMontages` が true のときだけ（コンストラクタで初期化しておらず、既定は false） | `MotionWarpingComponent.cpp`（`UpdateWithContext`） |
| 修飾子ができる条件 | `PreviousPosition >= StartTime && PreviousPosition < EndTime`。途中から再生しても、開始が `EndTime` より前なら区間に入る。**`EndTime` より後から始めるとワープしない** | 同上 |
| 対象のモンタージュ | キャラクターの **root motion のモンタージュ**（`ACharacter::GetRootMotionAnimMontageInstance`）だけ。CMC の `ProcessRootMotionPreConvertToWorld` から呼ばれる | `Private/MotionWarpingCharacterAdapter.cpp` |
| 区間を読む関数 | `UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Anim, TEXT("HitPoint"), OutWindows)`（`StartTime`・`EndTime`）。`StrokeMetaBaker` もこれで `WarpWindow` を読めば、実行時と同じ場所を見る | `Public/MotionWarpingComponent.h` |
| コードから足す | `URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(Comp, Anim, Start, End, WarpTargetName, …)` で、NotifyState なしで区間を足せる | `Public/RootMotionModifier_SkewWarp.h` |

### 3.4 Skew Warp の動き

| 項目 | 内容（`URootMotionModifier_Warp`・`URootMotionModifier_SkewWarp`） |
|---|---|
| 目標の意味 | 「**区間の終わりに**ルートがあるべき場所」（`RootMotionModifier.cpp` の `Update` のコメント） |
| ルートの位置 | `bWarpToFeetLocation = true`（既定）。キャラクターのアダプタはカプセルの底（`GetActorLocation − Up × 半高`）をルートとする |
| 高さ | `bIgnoreZAxis = true`（既定）。目標の Z は使わない（打点の高さはストロークの選択と IK で合わせる） |
| アニメに移動がないとき | 区間の root motion の移動がほぼ 0 なら、起点から目標へ `AddTranslationEasingFunc` で補間して移動を足す |
| 区間の後の移動 | `bSubtractRemainingRootMotion` は「区間の終わり → **アニメの終わり**」の root motion を引く。インパクトまでではない |
| 向き | `bWarpRotation`、`RotationType`（Default＝目標の向き／Facing＝目標を向く）、`RotationMethod`（Slerp・SlerpWithClampedRate・ConstantRate・Scale）、`WarpMaxRotationRate`。±35° の制限は呼ぶ側で目標の向きを丸める |
| 速さの上限 | `MaxSpeedClampRatio`（アニメの速さの何倍まで。0 は制限なし。protected の UPROPERTY なのでアセット側で設定） |
| アニメの中の打点 | `WarpPointAnimProvider`（None・Static・Bone）。Bone は**骨の名前**で引く（`FBoneContainer::GetPoseBoneIndexForBoneName`）ので、静的メッシュのソケット（`racket_sweetspot`）は使えない |

## 4. モンタージュの時刻・Notify・`StrokeMetaBaker`

### 4.1 再生・再生速度・位置（`UAnimInstance`、`Runtime/Engine/Classes/Animation/AnimInstance.h`）

| したいこと | API | 注意 |
|---|---|---|
| 開始位置と速度を指定して再生 | `Montage_Play(Montage, InPlayRate, EMontagePlayReturnType::MontageLength, InTimeToStartMontageAt, bStopAllMontages)`。ブレンドを指定するなら `Montage_PlayWithBlendIn`・`Montage_PlayWithBlendSettings` | 開始位置は `[0, 長さ]` に丸める（`AnimInstance.cpp` の `Montage_PlayInternal`） |
| 再生中に速度を変える | `Montage_SetPlayRate(Montage, NewPlayRate)` | — |
| 今の位置 | `Montage_GetPosition(Montage)`、`GetActiveInstanceForMontage(Montage)->GetPosition()` | 位置はアニメの Tick（描画のフレーム）で進む |
| 実際の速度 | `Montage_GetEffectivePlayRate`＝`PlayRate × Montage->RateScale`。`Montage_GetPlayRate` は `RateScale` を含まない | DD-03 §6.1 の式は `RateScale = 1` が前提（G-9） |
| root motion を一時的に止める | `FAnimMontageInstance::PushDisableRootMotion()`・`PopDisableRootMotion()` | `Classes/Animation/AnimMontage.h` |
| 終わりの通知 | `Montage_SetEndDelegate`・`OnMontageEnded` | — |

### 4.2 `UAnimNotify_TennisImpact` から C++ へ

```cpp
// 5.8 の Notify は 3 引数だけ（2 引数の版はない：Classes/Animation/AnimNotifies/AnimNotify.h）
void UAnimNotify_TennisImpact::Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Anim, const FAnimNotifyEventReference& Ref)
{
    Super::Notify(Mesh, Anim, Ref);
    UTennisStrokeComponent* Stroke = Mesh && Mesh->GetOwner() ? Mesh->GetOwner()->FindComponentByClass<UTennisStrokeComponent>() : nullptr;
    if (Stroke == nullptr) { return; }  // アニメの編集画面のプレビューでも呼ばれる（bShouldFireInEditor）
    const FAnimNotifyEvent* Event = Ref.GetNotify();
    Stroke->OnImpactNotify(Event ? Event->GetTriggerTime() : Ref.GetCurrentAnimationTime());  // Notify の正確な時刻を優先
}
```

- `FAnimNotifyEventReference`（`Runtime/Engine/Public/Animation/AnimNotifyQueue.h`）：`GetNotify()`（`FAnimNotifyEvent`）、`GetCurrentAnimationTime()`（発火したフレームのアニメの時刻）、`GetMirrorDataTable()`、`IsActiveContext()`
- `FAnimNotifyEvent`（`Runtime/Engine/Public/Animation/AnimTypes.h`）：`GetTriggerTime()`・`GetEndTriggerTime()`、`MontageTickType`（既定 `Queued`。`BranchingPoint` にするとモンタージュを進める途中で発火）、`TriggerWeightThreshold`、`NotifyTriggerChance`（既定 1）、`bTriggerOnDedicatedServer`（既定 true）

### 4.3 `StrokeMetaBaker`：Notify の時刻と、その時刻のソケットの Transform

| 読むもの | API | 場所 |
|---|---|---|
| Notify の時刻 | `UAnimSequenceBase::Notifies`（public）を回し、`Notify` が `UAnimNotify_TennisImpact`・`RecoverStart` の印のものの `GetTriggerTime()` | `Classes/Animation/AnimSequenceBase.h` |
| ワープの区間 | §3.3 の `GetMotionWarpingWindowsForWarpTargetFromAnimation` | — |
| 時刻 t の姿勢（コンポーネント空間） | **訂正（2026-11-13）：`Anim` にモンタージュを渡すと落ちる**（`UAnimMontage::GetAnimationPose` は `check(false)`）。モンタージュは、スロットのトラックの `GetSegmentAtTime(t)` からアニメーションと `ConvertTrackPosToAnimPos(t)` を取り出して渡す（DD-03 §5.4）。`UAnimPoseExtensions::GetAnimPoseAtTime(Anim, t, Options, Pose)` → `GetBonePose(Pose, Name, EAnimPoseSpaces::World)`・`GetSocketPose(Pose, Name, EAnimPoseSpaces::World)`。`World` は**コンポーネント空間**（ヘッダーのコメント）。`Options.OptionalSkeletalMesh` を渡すと、そのメッシュのソケットも引ける | `Source/Editor/AnimationBlueprintLibrary/Public/AnimPose.h`（**エディタ専用のモジュール** `AnimationBlueprintLibrary`） |
| 区間の root motion | `UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Anim, Start, End)`、`ExtractRootTransformFromAnimation(Anim, t)`（モンタージュは最初のトラック） | `MotionWarping/Public/MotionWarpingComponent.h` |
| ラケットのソケット | `UStaticMesh::FindSocket(Name)` の `RelativeLocation`・`RelativeRotation`（`racket_sweetspot`・`grip`） | — |

```cpp
// TennisEditor（Build.cs に "AnimationBlueprintLibrary"・"MotionWarping" を足す。ARCH-02 §2.3 の一覧にはまだない）
FAnimPose Pose;
FAnimPoseEvaluationOptions Opt;            // 既定：EvaluationType Raw、bIncorporateRootMotionIntoPose true
Opt.OptionalSkeletalMesh = PlayerMesh;     // 手のソケット racket_attach_r は骨格のソケット
UAnimPoseExtensions::GetAnimPoseAtTime(Montage, ImpactTime, Opt, Pose);
const FTransform HandCS = UAnimPoseExtensions::GetSocketPose(Pose, TEXT("racket_attach_r"), EAnimPoseSpaces::World);
const FTransform RootCS = UAnimPoseExtensions::GetBonePose(Pose, TEXT("root"), EAnimPoseSpaces::World);
// ラケット：grip を手のソケットに重ねる（UTennisRacketComponent::SetRacketMesh と同じ）→ racket_sweetspot
const FTransform SweetCS = SweetSocketRel * GripSocketRel.Inverse() * HandCS;
const FTransform SweetFromRoot = SweetCS.GetRelativeTransform(RootCS);  // ルート基準（メッシュの空間）
// GetSocketPose・GetBonePose は名前がないと単位行列を返すので、先に GetSocketNames・GetBoneNames で有無を確かめ、なければエラーにする
```

## 5. PoseSearch・Motion Matching（W-3.6）

### 5.1 プラグインと依存

| 項目 | 内容 | 場所 |
|---|---|---|
| プラグイン | `PoseSearch`（正式版、既定で無効）。**依存として `AnimationWarping`・`BlendStack`・`Chooser`・`MotionWarping`（Beta）・`RigVM`・`GameplayInsights`（エディタ）を有効にする** | `Plugins/Animation/PoseSearch/PoseSearch.uplugin` |
| モジュール | 実行時 `PoseSearch`（PreDefault）、`PoseSearchEditor`（UncookedOnly）。`AnimationWarping` の実行時のモジュール名は `AnimationWarpingRuntime` | 同上、`Plugins/Animation/AnimationWarping/AnimationWarping.uplugin` |
| C++ の依存 | 軌跡の型 `FTransformTrajectory`・`FTransformTrajectorySample` は **Engine**（`Runtime/Engine/Public/Animation/TrajectoryTypes.h`）。C++ で軌跡を作るだけなら `PoseSearch` のモジュールに依存しなくてよい | — |

### 5.2 最小のアセットと AnimGraph のノード

| もの | 内容 | 場所 |
|---|---|---|
| `UPoseSearchSchema` | 骨格（`Skeletons`。左利き用の `MirrorDataTable` もここ）、`SampleRate`（既定 30）、チャンネル（新規作成時は Trajectory と Pose の 2 つ：Epic の文書） | `PoseSearch/Public/PoseSearch/PoseSearchSchema.h` |
| `UPoseSearchDatabase` | `Schema` と、アニメの一覧（`FPoseSearchDatabaseAnimationAsset::AnimAsset`：シーケンス・ブレンドスペース・モンタージュ。5.7 で 1 つの型にまとまった）。既定で各アニメの最後の 0.3 s を除く（`ExcludeFromDatabaseParameters`） | `PoseSearchDatabase.h` |
| Motion Matching のノード | `FAnimNode_MotionMatching`（`Database` のピン。`SetDatabaseToSearch`・`SetDatabasesToSearch` で差し替え） | `AnimNode_MotionMatching.h` |
| Pose History のノード | `FAnimNode_PoseSearchHistoryCollector`。`Trajectory`（`FTransformTrajectory`）のピンに軌跡を入れる。**`bGenerateTrajectory` は Category が Experimental** | `AnimNode_PoseSearchHistoryCollector.h` |
| root motion | 検索のデータは `ExtractRootMotionFromRange`（root の骨の動き）から作る。**ロコモーションのアニメには root motion が要る**（Epic の文書。`bEnableRootMotion` が false でも root の骨が動いていれば取り出せる：`AnimSequence.cpp`） | `Private/PoseSearchAssetSampler.cpp` |

### 5.3 軌跡

| 方法 | 状態 | 内容 |
|---|---|---|
| `UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(AnimInstance, TrajectoryData, …)` | 正式（印なし） | 予測は **CMC の地上の移動の計算をまねる**（`UpdatePrediction_SimulateCharacterMovement`）。入力は CMC の `GetMaxSpeed`・`GetMaxBrakingDeceleration`・`GroundFriction`・`GetCurrentAcceleration`・`Velocity`。`bOrientRotationToMovement` が false のときの向きは `GetViewRotation().Yaw` の変化から作る（`Private/PoseSearchTrajectoryLibrary.cpp` の `FPoseSearchTrajectoryData::UpdateData`） |
| `PoseSearchGenerateTransformTrajectoryWithPredictor`・`IPoseSearchTrajectoryPredictorInterface` | **Experimental**（UINTERFACE に印） | 自前の予測を差し込む口 |
| `PoseSearchGenerateWarpedTransformTrajectory`、`UpdatePrediction_SimulateMontage` | **Experimental**（`UE_EXPERIMENTAL(5.8)`） | モンタージュとワープを含む予測 |
| `UCharacterTrajectoryComponent` | **Experimental**（`MotionTrajectory` プラグイン） | Epic の文書の古い説明はこれを前提にしている |
| 自前で `FTransformTrajectory` を作り、Pose History の `Trajectory` に入れる | Engine の型だけ | 過去の位置と、`TennisMovement::Step` で先を進めた位置を入れる（G-4）。サンプルは SkeletalMeshComponent のワールドの空間、時刻 0 は前のフレーム（ヘッダーのコメント） |

### 5.4 仮のデータ（`Content/Tennis/Characters/Animations/Locomotion/Mannequin/`）で足りるか

| 項目 | 内容 |
|---|---|
| ある | `MM_Idle`、`MF_Unarmed_Walk_*`・`MF_Unarmed_Jog_*`（各 8 方向）、`BS_Idle_Walk_Run` の 18 個 |
| root motion | アセットの名前の表（バイナリの文字列）に `bEnableRootMotion` と `RootMotionRootLock` がある（Walk・Jog の 16 個。`MM_Idle` は `bEnableRootMotion` だけ）ので、root motion が有効になっている可能性がある。**root の骨が実際に動くかは未確認**（エディタで見る） |
| ない | 歩き出し・止まり・切り返し（pivot）・サイドステップ・クロスオーバー・スプリットステップ。DD-03 §6.5 の `PSD_Locomotion` の中身はモーションキャプチャ（ART-02 §3）まで揃わない |
| 速さ | ジョグの速さが最高速度 5.5 m/s・ダッシュ 5.9 m/s に届くかは**未確認**（届かなければ、速い移動では足が滑る） |
| 判断 | 8 方向の歩き・走り・待機の繰り返しだけなので、Motion Matching を組む・軌跡をつなぐ・見た目を確かめる「仮」には足りる。遷移はブレンドになる。root の骨が動かない場合は、W-3.6 を 2 次元のブレンドスペース（8 方向の歩き・走り）で始め、Motion Matching は本番のデータで組むほうが単純 |

### 5.5 Game Animation Sample（5.8）の方式

ローカルにサンプルがないので、Web の情報だけ（**内容は未確認**。Epic のブログは取得できなかった）。

- 5.8 版は、Mover の Pawn のブレンドスタックを新しい Chooser（改良した Pose Match の列）で組み直した。Chooser に複数の Pose Search Database を入れ、Pose Match の列で最も合うアニメを選ぶ（RES-04 §2、Epic のフィード）。列の型はエンジンにある（`PoseSearch/Public/PoseSearch/Chooser/PoseSearchChooserColumn.h`）
- 必要なもの：PoseSearch・Chooser・MotionWarping・AnimationWarping・BlendStack（§5.1 の依存）。Mover を使う部分は Experimental（RES-04）。500 本ほどのアニメ（2024 年の初版の報道）
- このプロジェクトに入れるのは「Schema・Database・Motion Matching のノード・Pose History のノード・自前の軌跡・Orientation／Stride Warping」の最小の組。サンプルの Mover と、Experimental の軌跡の関数は使わない

## 6. 設計とのずれ（DD-03 §5・§6、ART-02 との照合）

設計書は変えていない。対応は W-3.4〜W-3.6 の設計の PR で決める（開発ルール集（非公開） §6：設計を先に更新する）。

| ID | 設計の前提 | 5.8 の実際（場所） | 影響 | 提案 |
|---|---|---|---|---|
| G-1 | ストローク中も移動は `TennisMovement::Step`（自動移動、フォロースルー中 ×0.40）。§6.5：テイクバックの間は下半身を Motion Matching | CMC の `PhysWalking` は `HasAnimRootMotion()` の間 `CalcVelocity` を呼ばない（`Runtime/Engine/Private/Components/CharacterMovementComponent.cpp`）。モンタージュの root motion は、移動が 0 でも `FRootMotionMovementParams::Accumulate` で「root motion あり」になる（`Classes/Animation/AnimationAsset.h`、`Private/Animation/AnimMontage.cpp`）。Motion Warping は root motion のモンタージュにだけ効く（§3.3）。なお、ふだんの移動も `UTennisMovementComponent::CalcVelocity` が CMC の `DeltaTime` で `Step` を 1 回呼ぶだけで、240 Hz の固定の刻みではない（`Source/Tennis/Private/Player/TennisMovementComponent.cpp`） | ストロークの間は、自動移動・フォロースルーの制限・`TennisMovement` の加減速が効かず、体はアニメの root motion とワープで動く。インパクトの距離（打つか空振りか、品質）は、どちらの案でも描画のフレームの時間に左右される（決定論の約束、開発ルール集（非公開） §6）。テイクバックの間もカプセルはモンタージュの root motion で動くので、下半身の Motion Matching の足の運びと合わない（足が滑る） | 案 A：設計どおり root motion＋Motion Warping。体の位置はフレーム依存と認め、判定の距離に余裕を持たせる。案 B：ストロークのモンタージュは root motion なし（その場）にし、体は `TennisMovement` の自動移動で打点へ運ぶ。スイートスポット＝体の位置＋焼いたオフセット。Motion Warping は使わない。B の利点は「移動のモデルが 1 つ（`TennisMovement`）のまま体を動かす」こと。決定論まで得るには、移動を固定の刻み（240 Hz）で進める仕組みが別に要る。決めるのは W-3.5 の設計の PR |
| G-2 | 区間はテイクバックの開始〜インパクトの 0.05 s 前。目標＝`BallPos(T_hit) − Rotate(Yaw, SweetSpotOffset)`（インパクトの瞬間のオフセット） | 目標は「区間の終わりのルートの位置」（§3.4）。`bSubtractRemainingRootMotion` はアニメの終わりまでを引く | 区間の終わり〜インパクトの 0.05 s のルートの移動（踏み込み）の分だけ、打点がずれる（1 m/s で 5 cm。T-STRK-003 の平均 < 5 cm に効く） | `StrokeMetaBaker` が「インパクトのスイートスポットを、**区間の終わりのルート**から見たオフセット」で焼く（§4.3 の `RootCS` を `WarpWindow.End` の時刻で取る）。または実行時に `ExtractRootMotionFromAnimation(Montage, End, ImpactTime)` を足す |
| G-3 | ART-02 §5：「`racket_sweetspot` のソケットの位置（ルート基準）」を読む | `racket_sweetspot` は `SM_Racket` のソケット。骨格には `racket_attach_r`・`_l` だけ。`GetSocketPose` は骨格とスケルタルメッシュのソケットだけを探し、ない名前は単位行列を黙って返す（`AnimPose.cpp`）。`ATennisCharacter` はメッシュを Yaw −90° 回して置く（`Source/Tennis/Private/Player/TennisCharacter.cpp`） | そのまま読むと、オフセットが 0 になって気づかない。メッシュの空間とアクターの空間の 90° の差を落とすと、打点が横にずれる | §4.3 の合成（手のソケット × `grip` の逆 × `racket_sweetspot`）。焼く空間（メッシュかアクターか）を `UStrokeData` に書く。名前がなければエラー。左利きは `racket_attach_l` で別に焼くか、ミラーの規則を決める（G-8） |
| G-4 | §6.5：軌跡は CMC の速度の履歴と自動移動の目標から作る（GASP 5.8 の方式） | 正式の生成関数は CMC 自身の加減速（摩擦・ブレーキ）をまね、向きは `GetViewRotation().Yaw`（§5.3）。自前の予測の口は Experimental | `TennisMovement`（一定の加速 8 m/s²・ブレーキ 10 m/s²・方向転換で 35% を失う）と予測が合わず、選ぶアニメがずれる。中継カメラの向きと体の向き（ネット）は違う | 軌跡は C++ で作る：過去は位置の履歴、未来は `TennisMovement::Step` を同じ入力で進めた位置（自動移動中はその目標）、向きはネットの方向。Pose History の `bGenerateTrajectory` は使わず、`Trajectory` のピンに入れる（R-07 の答え） |
| G-5 | ワープの呼び出しは `PlayAndWarp()` の 1 か所（Beta の差し替えに備える） | 区間は各モンタージュの NotifyState（アセット）に入る。中のシーケンスの NotifyState は既定で見ない（§3.3） | 111 本のモンタージュに `UAnimNotifyState_MotionWarping` を置くと、差し替えるときにアセットを全部直す | 区間の印は自前の NotifyState（`HitPoint` の窓）にし、`PlayAndWarp()` が `AddRootMotionModifierSkewWarp` で修飾子を足す（区間は `UStrokeData::WarpWindow`）。または NotifyState を置くなら、モンタージュ自身に置き、ベイカーも同じ所を読む |
| G-6 | RES-04 §4.2：Motion Warping（Beta）は代替に切り替えられる | `PoseSearch.uplugin` が `MotionWarping` を依存として有効にする | Motion Matching を使う限り、Beta のプラグインは出荷物から外せない（外せるのはコードの依存だけ） | RES-04 の「代替策」の意味を「コードの依存を 1 か所にする」に直す |
| G-7 | 入力の列挙（`EStrokeSide`・`EHitKind`・`EBackhandStyle`）をそのまま Chooser の列に使う | 列の束縛は Blueprint から見えるプロパティだけ（§2.2）。今の列挙は `UENUM()`（`Source/TennisSim/Public/Reach/TennisReach.h`）で、`BlueprintType` がない。UHT は `BlueprintType` のない列挙のプロパティも `BlueprintReadWrite` にできる（`Programs/Shared/EpicGames.UHT/Types/Properties/UhtEnumProperty.cs`） | Chooser のエディタの束縛の選択に、`BlueprintType` のない列挙が出るかは**未確認** | 迷わないように `UENUM(BlueprintType)` にする（`uint8` の列挙なので付けられる。TennisSim は Engine に依存しないが、UHT の印だけ）。入力は平らな USTRUCT 1 つにする |
| G-8 | 左利きはミラー（Mirror Data Table）で作る（DD-03 §9、ART-02 §1） | モンタージュの root motion は `ExtractRootMotionFromTrackRange(…, FAnimExtractContext())` で、ミラーの表を渡していない（`AnimMontage.cpp`） | AnimGraph のミラーで姿勢を反転しても、root motion とワープは反転しない可能性が高い（**未確認**） | G-1 の案 B なら root motion を使わないので影響しない。案 A なら左利き用のモンタージュをミラーして焼き、別のアセットにする |
| G-9 | `PlayRate = clamp(ImpactTime / avail, 0.80, 1.25)` | 実際の速度は `PlayRate × RateScale`（§4.1） | アセットの `RateScale` が 1 でないと、時刻がずれる | ベイカーが `RateScale == 1` を検査する。時刻の積分は `Montage_GetEffectivePlayRate` を使う |
| G-10 | §5.1：「最後の行」で未定義の組み合わせをなくす | 表に `FallbackResult` がある（§2.4） | フォールバックを設定すると T-STRK-002 が常に通る | §2.5 のとおり、フォールバックに落ちた組み合わせを数える |
| G-11 | §5.1 の HeightBand：VeryLow < 0.4、Low 0.4〜0.7、Mid 0.7〜1.3、High 1.3〜1.9、Overhead > 2.2 | （設計の中の問題） | 1.9〜2.2 m がどの帯にも入らない。§4.5 のボレー（0.3〜2.2 m）の 2.0 m で帯が決まらない | 帯の境界を設計で埋める。帯への写像は C++（列挙）なので、T-STRK-002 とは別に写像のテストを作る |
| G-12 | 変種の選び方（未定） | Randomize の列は `RandomStream` がないと `FMath::FRandRange`（§2.2） | ストロークの選び方がシードで再現できない | `Context.RandomStream` に選手の `FRandomStream` を渡す |
| G-13 | UML-02：`UStrokeData` に Side・Height・Reach・Family、Chooser の行にも同じ条件 | — | 同じ知識が 2 か所にある（DRY） | どちらかに寄せる：`UStrokeData` から条件を外す、または `TennisEditor` のツールが `UStrokeData` から表の行を作る・`ValidateChooser` が両者の一致を検査する |
| G-14 | UML-02：`UAnimNotify_TennisImpact::Notify(M, A)` | 5.8 は 3 引数（`FAnimNotifyEventReference`）だけ（§4.2） | 2 引数で override するとコンパイルが通らない | クラス図を 3 引数に直す |
| G-15 | W-3.5 でモンタージュを再生する | 今の選手は `GetMesh()->PlayAnimation(Idle, true)`（単一ノード。`TennisCharacter.cpp`）で、Slot がない | モンタージュの姿勢が出ない | W-3.6 の `ABP_TennisPlayer`（Slot `UpperBody`・`FullBody`）を W-3.5 より先に作るか、W-3.5 で最小の AnimBP を作る。**解決（2026-11-13、#140）**：Slot だけの最小の `ABP_TennisPlayer` を選手に使う（DD-03 §6.5） |
| G-16 | `TennisEditor.Build.cs`（ARCH-02 §2.3）の依存 | `UAnimPoseExtensions` は `AnimationBlueprintLibrary`（エディタ専用）、区間と root motion の関数は `MotionWarping` | — | `TennisEditor` に `"AnimationBlueprintLibrary"`・`"MotionWarping"`（案 A のとき）・`"Chooser"`（G-13 の表を作るツールのとき）を足す |

## 7. 未確認の一覧

| 項目 | 確かめ方 |
|---|---|
| 仮のロコモーション（Walk・Jog）の root の骨が実際に動くか、ジョグの速さ | 確かめた（2026-11-19、エディタの Python で root の骨の位置を読んだ）：歩き・ジョグとも root の骨が動く。歩き 3.0 m/s、ジョグ 6.0 m/s（`BS_Idle_Walk_Run` の標本の速さと同じ）。`MM_Idle` は動かない。DD-03 §6.5 |
| モンタージュの root motion がミラーで反転しないこと（G-8） | 左利きの選手でモンタージュを再生し、移動の向きを見る（案 A を選ぶ場合だけ） |
| `BlueprintType` のない列挙のプロパティが、Chooser の列の束縛の選択に出るか（G-7） | 表を作るときに見る（`BlueprintType` を付ければ確かめなくてよい） |
| Chooser の表を Python（エディタのスクリプト）で作る・直す方法 | `ColumnsStructs`・`ResultsStructs` は C++ からは書ける（UPROPERTY、エディタのデータ）。Python で `FInstancedStruct` を扱えるかは未確認 |
| Game Animation Sample 5.8 の構成の詳細 | サンプルを入れて中身を見る（必要になったら） |
| `Montage_Play` の直後の最初の Tick で、位置が 1 フレーム分進むか | モンタージュを止めてステッパの位置を置く形（DD-03 §6.1・§6.6）にしたので、インパクトの時刻には効かない。T-STRK-004 は、インパクトの時刻 T と打点の時刻の差で測る（DD-03 §12.2） |

## 8. 出典

**エンジンのソース（UE 5.8.4、`C:\Program Files\Epic Games\UE_5.8\Engine\` から）**

- Chooser：`Plugins/Chooser/Chooser.uplugin`、`Plugins/Chooser/Source/Chooser/Chooser.Build.cs`、`Public/Chooser.h`・`ChooserSignature.h`・`IObjectChooser.h`・`IHasContext.h`・`ChooserFunctionLibrary.h`・`ChooserPropertyAccess.h`・`EnumColumn.h`・`FloatRangeColumn.h`・`ObjectChooser_Asset.h`・`IChooserColumn.h`、`Private/Chooser.cpp`・`ChooserFunctionLibrary.cpp`・`ObjectChooser_Asset.cpp`・`FloatRangeColumn.cpp`・`RandomizeColumn.cpp`、`Source/ChooserEditor/Private/SPropertyAccessChainWidget.cpp`
- Motion Warping：`Plugins/Animation/MotionWarping/MotionWarping.uplugin`、`Source/MotionWarping/Public/MotionWarpingComponent.h`・`RootMotionModifier.h`・`RootMotionModifier_SkewWarp.h`・`AnimNotifyState_MotionWarping.h`、`Private/MotionWarpingComponent.cpp`・`RootMotionModifier.cpp`・`RootMotionModifier_SkewWarp.cpp`・`MotionWarpingCharacterAdapter.cpp`
- エンジン本体：`Source/Runtime/Engine/Classes/Animation/AnimInstance.h`・`AnimMontage.h`・`AnimSequence.h`・`AnimSequenceBase.h`・`AnimationAsset.h`・`AnimNotifies/AnimNotify.h`・`AnimNotifies/AnimNotifyState.h`、`Source/Runtime/Engine/Public/Animation/AnimNotifyQueue.h`・`AnimTypes.h`・`TrajectoryTypes.h`、`Source/Runtime/Engine/Private/Animation/AnimInstance.cpp`・`AnimMontage.cpp`・`AnimSequence.cpp`、`Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp`、`Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h`
- エディタ：`Source/Editor/AnimationBlueprintLibrary/Public/AnimPose.h`、`Private/AnimPose.cpp`、`AnimationBlueprintLibrary.build.cs`
- PoseSearch：`Plugins/Animation/PoseSearch/PoseSearch.uplugin`、`Source/Runtime/PoseSearch.Build.cs`、`Source/Runtime/Public/PoseSearch/PoseSearchSchema.h`・`PoseSearchDatabase.h`・`AnimNode_MotionMatching.h`・`AnimNode_PoseSearchHistoryCollector.h`・`PoseSearchTrajectoryLibrary.h`・`PoseSearchTrajectoryPredictor.h`・`Chooser/PoseSearchChooserColumn.h`、`Source/Runtime/Private/PoseSearchTrajectoryLibrary.cpp`・`PoseSearchAssetSampler.cpp`、`Plugins/Experimental/Animation/MotionTrajectory/MotionTrajectory.uplugin`・`Source/MotionTrajectory/Public/CharacterTrajectoryComponent.h`、`Plugins/Animation/AnimationWarping/AnimationWarping.uplugin`

**このリポジトリ**：`Source/TennisSim/Public/Reach/TennisReach.h`、`Source/Tennis/Private/Player/TennisCharacter.cpp`、`Source/Tennis/Public/Player/TennisRacketComponent.h`、`Content/Tennis/Characters/Animations/Locomotion/Mannequin/`

**Web**

- Epic Developer Community, *Motion Matching in Unreal Engine* — https://dev.epicgames.com/documentation/en-US/unreal-engine/motion-matching-in-unreal-engine
- Epic Developer Community, *Game Animation Sample Project in Unreal Engine* — https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine
- Unreal Engine Tech Blog, *Download the latest Game Animation Sample project, now updated for UE 5.8*（2026-11-12 は取得できず。検索の要約だけ） — https://www.unrealengine.com/tech-blog/download-the-latest-game-animation-sample-project-now-updated-for-ue-5-8
