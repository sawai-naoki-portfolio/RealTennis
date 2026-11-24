# UML-02：クラス図

| 項目 | 内容 |
|---|---|
| 文書 ID | UML-02 |
| 版 | 1.0（2026-11-04） |
| 表記 | Mermaid の classDiagram。`*--` = コンポジション（所有）、`o--` = 集約（参照）、`-->` = 関連、`..>` = 依存（使う）、`<|--` = 継承。`<<struct>>` は USTRUCT または素の C++ 構造体、`<<namespace>>` は関数の集まり |
| 関連 | [モジュール設計](../02_Architecture/02_Module_Package_Design.md)、[データモデル](../05_Data/01_Data_Model.md) |

> 図には主要なメンバーだけを書く。完全な定義はヘッダー（`Source/`）が正。

---

## 1. シミュレーション層：ボール（TennisSim）

```mermaid
classDiagram
    class FBallState {
        <<struct>>
        +FVector Pos
        +FVector Vel
        +FVector Spin
        +double Time
        +int32 NumBounces
        +int32 LastHitter
        +int32 Seq
        +bool bNetTouched
        +bool bInPlay
    }
    class FBallParams {
        <<struct>>
        +double Mass
        +double Radius
        +double Cd0
        +double CdSpin
        +double InertiaAlpha
        +double SpinDecayPerMeter
        +double RestitutionHard
        +CrossSectionArea() double
        +MomentOfInertia() double
    }
    class FSurfaceParams {
        <<struct>>
        +double RestitutionRef
        +double RestitutionSpeedSlope
        +double Friction
        +double TangentialRestitution
        +double TempCoeff
        +double ContactPatchBase
        +double ContactPatchPerVn
        +RestitutionAt(double Vn, double TempC) double
        +ContactPatch(double Vn) double
    }
    class FAirParams {
        <<struct>>
        +double Rho
        +FVector WindMean
        +double GustFactor
        +double GustPhase1
        +double GustPhase2
        +WindAt(double Time) FVector
    }
    class FMatchConditions {
        <<struct>>
        +double TemperatureC
        +double AltitudeM
        +FVector WindMean
        +double GustFactor
        +bool bRoofClosed
        +EffectiveTempC() double
        +ComputeAir(int32 PointSeed) FAirParams
        +AirDensity(double AltitudeM, double TempC)$ double
    }
    class TennisCourtGeometry {
        <<namespace>>
        +NetHeightAt(double Y)$ double
        +HasNetAt(double Y)$ bool
        +HasNetBodyAt(double Y)$ bool
    }
    class FBallSimContext {
        <<struct>>
        +FBallParams Ball
        +FSurfaceParams Surface
        +FAirParams Air
        +double TempC
    }
    class TennisBallPhysics {
        <<namespace>>
        +Step(FBallState S, double Dt, FBallSimContext Ctx, TArray~FBallEvent~ Out)$ void
        +AdvanceTo(FBallState S, double Target, FBallSimContext Ctx, TArray~FBallEvent~ Out)$ bool
        +ComputeAccel(FVector V, FVector W, FBallParams B, FAirParams A, double T)$ FVector
        +IntegrateFlight(FBallState S, double Dt, FBallSimContext Ctx)$ void
        +ResolveGroundBounce(FBallState S, FBallParams B, FSurfaceParams Sf, double TempC)$ void
    }
    class FBallEvent {
        <<struct>>
        +EBallEventType Type
        +double Time
        +FVector Pos
        +FVector Vel
        +FVector Spin
        +double NormalSpeed
        +double ContactPatch
        +double NetClearance
        +ENetPart NetPart
    }
    class FTrajectory {
        <<struct>>
        +int32 Seq
        +TArray~FBallState~ States
        +TArray~FBallEvent~ Events
        +FirstBounce() FBallEvent
    }
    class FTennisBallPredictor {
        +Predict(FBallState Start, FBallSimContext Ctx, FPredictOptions Opt, FTrajectory Out)$ void
        +GetStateAt(FTrajectory T, double Time, FBallSimContext Ctx, FBallState Out)$ bool
    }
    FBallSimContext *-- FBallParams
    FBallSimContext *-- FSurfaceParams
    FBallSimContext *-- FAirParams
    FMatchConditions ..> FAirParams : creates
    TennisBallPhysics ..> FBallState : updates
    TennisBallPhysics ..> FBallSimContext : reads
    TennisBallPhysics ..> TennisCourtGeometry : net
    TennisBallPhysics ..> FBallEvent : emits
    FTennisBallPredictor ..> TennisBallPhysics : same Step
    FTennisBallPredictor ..> FTrajectory : fills
    FTrajectory *-- FBallState
    FTrajectory *-- FBallEvent
```

- 規格値（コートの寸法・ネットの高さ・ポストの位置）は `TennisConstants.h` の `TennisConst`。図の型は値を持たず、そこを参照する
- ネット・ポスト・シングルススティックとの衝突は `TennisBallPhysics` の中（Private）で解く。`EBallEventType` は Bounce・NetCross・NetTouch・PostHit
- 後で足すもの：恒久施設（`FBallSimContext::Fixtures`、W-5.3。W-1.4 の残り）と選手の体（`Bodies`・`BodyHit`、W-5.3）、コートの区域の判定（`IsInside`・`ServiceBox`、M5 のルールの判定）

## 2. シミュレーション層：ショット（TennisSim）

```mermaid
classDiagram
    class ETennisShotType {
        <<enumeration>>
        Flat
        Topspin
        HeavyTopspin
        Slice
        Lob
        SliceLob
        Drop
        Volley
        HalfVolley
        Smash
        ServeFlat
        ServeSlice
        ServeKick
        ServeUnderarm
    }
    class FShotIntent {
        <<struct>>
        +ETennisShotType Type
        +FVector2D AimTarget
        +TOptional~double~ ChargeStartTime
        +TOptional~double~ ReleaseTime
        +bool bAutoReleased
        +double PowerMod
        +double ControlMod
        +EInputDevice Device
        +int32 PlayerId
    }
    class FShotQualityInputs {
        <<struct>>
        +double TimingError
        +double ImpactDistance
        +double RunSpeedRatio
        +double StretchRatio
        +double IncomingSpeed
        +double IncomingSpin
        +double ContactHeight
        +double Stamina
        +double PressureFactor
    }
    class FShotQualityResult {
        <<struct>>
        +double Quality
        +ETimingGrade Grade
        +double SigmaDepth
        +double SigmaSide
        +double AimPull
        +TArray~EMissCause~ Causes
    }
    class FTennisShotQuality {
        +Evaluate(FShotQualityInputs In, FShotTypeParams T, FPlayerShotSkill Skill)$ FShotQualityResult
    }
    class FTennisErrorModel {
        +ApplyError(FVector2D Target, FShotQualityResult Q, FRandomStream Rng, FVector2D Dir)$ FVector2D
        +MakeSeed(int32 MatchSeed, int32 RallySeq, int32 ShotIndex)$ int32
    }
    class FShotRequest {
        <<struct>>
        +FVector Origin
        +FVector Target
        +double Time
        +ESolverConstraint Constraint
        +double ConstraintValue
        +double Speed
        +double MinSpeed
        +double MaxSpeed
        +double InitialElevationDeg
        +double Rpm
        +double SpinAxisDeg
    }
    class FShotTypeParams {
        <<struct>>
        +double MinSpeed
        +double MaxSpeed
        +double BaseRpm
        +double SpinAxisDeg
        +ESolverConstraint Constraint
        +double ConstraintAtCharge0
        +double ConstraintAtCharge1
        +double InitialElevationDeg
        +double BaseSigma
        +double PaceReuseFactor
    }
    class TennisShotRequestMapping {
        <<namespace>>
        +Make(FShotTypeParams Params, FVector Origin, FVector Target, double Time, double Charge, ETennisHandedness Handedness, double SideInput)$ FShotRequest
        +ChargeAtImpact(FShotIntent Intent, double ChargeTime)$ double
    }
    class FShotSolveResult {
        <<struct>>
        +bool bSolved
        +FBallState Launch
        +FVector PredictedLanding
        +double ResidualRange
        +double ResidualConstraint
        +double PredictedNetClearance
        +double PredictedFlightTime
        +int32 Iterations
        +EShotFallback Fallback
    }
    class FTennisShotSolver {
        -FBallSimContext Context
        +Solve(FShotRequest Request) FShotSolveResult
        +SimulateToFirstBounce(FBallState Launch) FFlight
        +MakeLaunch(FShotRequest Request, double Azimuth, double Speed, double Elevation)$ FBallState
    }
    class FFlight {
        <<struct>>
        +bool bLanded
        +FVector LandPos
        +bool bReachedNet
        +double NetClearance
        +double Apex
        +double FlightTime
    }
    class ETennisHandedness {
        <<enumeration>>
        Right
        Left
    }
    class TennisShotSpin {
        <<namespace>>
        +AxisDeg(double RightHandedAxisDeg, ETennisHandedness Handedness, double SideInput)$ double
    }
    class TennisShotInitialGuess {
        <<namespace>>
        +Estimate(FShotRequest Request, double Azimuth, double ConstraintValue, FBallSimContext Ctx)$ FGuess
    }
    class UPrimaryDataAsset
    class UShotTypeData {
        +ETennisShotType Type
        +FShotTypeParams Params
    }
    UPrimaryDataAsset <|-- UShotTypeData
    UShotTypeData *-- FShotTypeParams
    UShotTypeData ..> ETennisShotType
    FShotIntent ..> ETennisShotType
    FTennisShotQuality ..> FShotQualityInputs : reads
    FTennisShotQuality ..> FShotQualityResult : returns
    FTennisErrorModel ..> FShotQualityResult : uses sigma
    TennisShotRequestMapping ..> FShotTypeParams : reads
    TennisShotRequestMapping ..> FShotRequest : makes
    TennisShotRequestMapping ..> TennisShotSpin
    TennisShotSpin ..> ETennisHandedness
    TennisShotSpin ..> FShotRequest : SpinAxisDeg
    FTennisShotSolver ..> FShotRequest : input
    FTennisShotSolver ..> FShotSolveResult : output
    FTennisShotSolver ..> FFlight
    FTennisShotSolver ..> TennisShotInitialGuess : initial value
    TennisShotInitialGuess ..> TennisBallPhysics : ComputeAccel
    FTennisShotSolver ..> TennisBallPhysics : same Step
    class TennisBallPhysics {
        <<namespace>>
    }
```

- `UShotTypeData` は `DA_Shot_*`（球種ごとに 1 つ）。`FFlight` は `FTennisShotSolver::FFlight`。`TennisShotInitialGuess` は Private（抗力とマグヌスを入れた放物線の近似。DD-02 §7.3）
- 後で足すもの：`FShotIntent` の `AimTarget`・`PowerMod`・`ControlMod`（M4。W-4.2）と `Device`・`PlayerId`（今あるのは `Type`・`ChargeStartTime`・`ReleaseTime`・`bAutoReleased`）。M4 で `FShotTypeParams` の `BaseSigma`・`PaceReuseFactor`、`FShotQualityInputs`・`FShotQualityResult`・`FTennisShotQuality`・`FTennisErrorModel`（W-4.1）

## 3. ゲームプレイ層：ボールとコート（Tennis）

```mermaid
classDiagram
    class AActor
    class UActorComponent
    class UPrimaryDataAsset
    class ATennisCourt {
        +double UnrealUnitsPerMeter$
        +TObjectPtr~UCourtSurfaceData~ Surface
        +TArray~UTennisFixtureComponent~ Fixtures
        +LocalMetersToWorld(FVector P) FVector
        +WorldToLocalMeters(FVector P) FVector
        +LocalMetersToWorldVector(FTransform Court, FVector V)$ FVector
        +WorldToLocalMetersVector(FTransform Court, FVector V)$ FVector
        +BuildSimContext(FMatchConditions C, UBallSpecData B) FBallSimContext
    }
    class UTennisFixtureComponent {
        +EFixtureType Type
        +FFixtureShape ToShape() FFixtureShape
    }
    class ATennisBall {
        #UStaticMeshComponent Mesh
        +UNiagaraComponent Trail
        #UTennisBallSimComponent Sim
        +GetSim() UTennisBallSimComponent
        +ApplySquash(double Vn) void
    }
    class UTennisBallSimComponent {
        -FBallState State
        -FBallState PrevState
        -FFixedStepClock Clock
        -int64 StepCount
        -FBallSimContext Context
        -FTrajectory Prediction
        +FOnTennisBallEvent OnBallEvent
        +Launch(FBallState LaunchState) void
        +GetState() FBallState
        +GetContext() FBallSimContext
        +GetPrediction() FTrajectory
        +GetStateAt(double Time, FBallState Out) bool
        +GetStepCount() int64
        +GetCourt() ATennisCourt
        +TickComponent(float Dt) void
        -StepOnce() void
        -Broadcast(TArray~FBallEvent~ E) void
        -DrawDebug() void
    }
    class TennisBallDebugDraw {
        <<namespace>>
        +TrajectoryPolyline(FTrajectory T, FTransform Court, int32 Stride)$ TArray~FVector~
        +DrawPrediction(UWorld W, ATennisCourt C, FTrajectory T)$ void
        +DrawActualPath(UWorld W, ATennisCourt C, TArray~FVector~ Path)$ void
    }
    class UCourtSurfaceData {
        +FText DisplayName
        +double TargetCpr
        +FSurfaceParams Params
        +TObjectPtr~UMetaSoundSource~ BounceSound
        +TObjectPtr~UNiagaraSystem~ BounceFX
        +bool bLeavesMarks
        +double SlideFactor
    }
    class UBallSpecData {
        +FText DisplayName
        +FBallParams Params
    }
    class UVenueData {
        +FText Name
        +TObjectPtr~UCourtSurfaceData~ Surface
        +TObjectPtr~UBallSpecData~ Ball
        +FMatchConditions Conditions
        +TSoftObjectPtr~UWorld~ Level
    }
    AActor <|-- ATennisCourt
    AActor <|-- ATennisBall
    UActorComponent <|-- UTennisBallSimComponent
    UActorComponent <|-- UTennisFixtureComponent
    UPrimaryDataAsset <|-- UCourtSurfaceData
    UPrimaryDataAsset <|-- UBallSpecData
    UPrimaryDataAsset <|-- UVenueData
    ATennisBall *-- UTennisBallSimComponent
    ATennisCourt *-- UTennisFixtureComponent
    ATennisCourt o-- UCourtSurfaceData
    UTennisBallSimComponent o-- ATennisCourt : context
    UTennisBallSimComponent ..> TennisBallDebugDraw : debug
    UVenueData o-- UCourtSurfaceData
    UVenueData o-- UBallSpecData
```

- `UTennisBallSimComponent` に後で足すもの：トス（`Toss`、サーブ W-3.7）と選手の体（`SetBodies`、W-5.3）
- 今の `ATennisCourt` は座標の変換（コート局所の m ↔ ワールドの cm）だけを持つ。面のデータ（`Surface`）・恒久施設（`Fixtures`・`UTennisFixtureComponent`、W-5.3）・物理の条件を作る `BuildSimContext` は後で足す（今の `UTennisBallSimComponent` の条件は既定値）。寸法は `TennisConst`、幾何は `TennisCourtGeometry`（§1）
- 後で足すもの：`ATennisBall` の `Trail`・`ApplySquash`（W-7.6）、`UCourtSurfaceData` の `BounceSound`〜`SlideFactor`（M7）、`UVenueData`（会場のデータ。W-9.3）

## 4. ゲームプレイ層：選手・入力・アニメーション（Tennis）

```mermaid
classDiagram
    class ACharacter
    class APlayerController
    class UCharacterMovementComponent
    class UAnimInstance
    class UAnimNotify
    class ATennisCharacter {
        +float CapsuleRadius$
        +float CapsuleHalfHeight$
        +FName RacketHandSocket$
        +int32 PlayerIndex
        +int32 CourtSide
        +ETennisHandedness Handedness
        +EBackhandStyle Backhand
        +TObjectPtr~UPlayerProfileData~ Profile
        +GetTennisMovement() UTennisMovementComponent
        +GetReach() UTennisReachComponent
        +GetRacket() UTennisRacketComponent
        +GetFollowers() TArray~USkeletalMeshComponent~
        +GetGrooms() TArray~UGroomComponent~
        +GetAppearance() UTennisPlayerAppearance
        +ApplyAppearance(UTennisPlayerAppearance A) bool
        +SubmitShotIntent(FShotIntent I) void
        +CancelShotIntent() void
        +GetShotIntent() FShotIntent
        +GetPlannedHit() FHitCandidate
        +FollowBall(UTennisBallSimComponent B) void
        +OnHitCandidateFound FSimpleMulticastDelegate
        +OnStrokeEnded FSimpleMulticastDelegate
        +OnStrokeImpact FSimpleMulticastDelegate
        +GetSweetSpotWorld() FVector
        +GetBodyCapsule() FPlayerCapsule
    }
    class UTennisMovementComponent {
        +FMovementParams Params
        #TObjectPtr~ATennisCourt~ Court
        -FMovementStepper Stepper
        -FMovementModifiers Modifiers
        -double SideSign
        +double AssistWeight
        +StartBoost() void
        +AdvanceWorldVelocity(FTransform Court, FVector Location, FVector Velocity, FVector Input, double Side, FMovementStepper S, FMovementParams P, double FrameTime, double TimeScale, FMovementModifiers Mods)$ FVector
        +GetCourt() ATennisCourt
        +GetSideSign() double
        +GetMoveState() FMovementState
        +PredictWorldPath(TConstArrayView~double~ Times) TArray~FVector~
        #CalcVelocity(float Dt, float Friction, bool bFluid, float Braking) void
        +TriggerSplitStep(double OpponentImpactTime) void
        +SetAutoMoveTarget(FVector2D CourtTarget, double TimeLeft, double SettleTime) void
        +ClearAutoMoveTarget() void
        +IsAutoMoving() bool
        +SetFollowThrough(bool bOn) void
        +GetModifiers() FMovementModifiers
        +GetMaxSpeed() float
    }
    class TennisMovement {
        <<namespace>>
        +Step(FMovementState S, FVector2D Input, double SideSign, FMovementModifiers M, FMovementParams P, double Dt)$ FMovementState
        +AdvanceFrame(FMovementStepper S, FVector2D Position, FVector2D Input, double SideSign, FMovementModifiers M, FMovementParams P, double FrameTime, double TimeScale)$ FVector2D
        +AutoMoveInput(FVector2D Position, FAutoMoveTarget A, double SideSign, FMovementParams P)$ FVector2D
        +ClampToArea(FVector2D Pos, FVector2D Vel, double SideSign)$ void
        +IsBackpedal(FVector2D Dir, double SideSign)$ bool
        +MakeParams(double Speed, double Acceleration)$ FMovementParams
        +StartBoost(FMovementState S, FMovementParams P)$ void
        +PredictPath(FMovementState S, FVector2D Position, TOptional~FAutoMoveTarget~ A, FVector2D Input, double SideSign, FMovementModifiers M, FMovementParams P, TConstArrayView~double~ Times)$ TArray~FVector2D~
    }
    class UTennisReachComponent {
        +FReachParams Params
        +OnCandidateFound FOnHitCandidateFound
        -TWeakObjectPtr~UTennisBallSimComponent~ Ball
        +SetBall(UTennisBallSimComponent B) void
        +StartSearching() void
        +StopSearching() void
        +IsSearching() bool
        +FindBest(FHitCandidate Out) bool
        +MakeQuery(FReachQuery Out) bool
    }
    class TennisReach {
        <<namespace>>
        +TimeToReach(FReachQuery Q, FVector2D Target, FMovementParams M)$ double
        +FindCandidates(FTrajectory T, FReachQuery Q, FMovementParams M, FReachParams P, TArray~FHitCandidate~ Out)$ void
        +RefineForStroke(FTrajectory T, FHitCandidate C, FVector SweetSpotOffset, FVector2D AimPoint, FReachQuery Q, FMovementParams M, FReachParams P, FStrokeSyncParams S)$ FHitCandidate
    }
    class FReachQuery {
        <<struct>>
        +FVector2D Position
        +FVector2D Velocity
        +double BoostTimeLeft
        +double SideSign
        +ETennisHandedness Handedness
        +EBackhandStyle Backhand
        +double Now
        +double ReactionTime
        +double PrepTime
    }
    class FHitCandidate {
        <<struct>>
        +double Time
        +FVector BallPos
        +FVector BallVel
        +FVector2D BodyTarget
        +EStrokeSide Side
        +EHitKind Kind
        +bool bStretch
        +double TimeToReach
        +double Score
    }
    class TennisStrokeTiming {
        <<namespace>>
        +WrapAngle(double Angle)$ double
        +NeutralAim(double SideSign, FStrokeSyncParams P)$ FVector2D
        +PlanStart(double ImpactTime, double Available, FStrokeSyncParams P)$ FStrokeStartPlan
        +CorrectPlayRate(double Rate, double Position, double ImpactTime, double Available, double Elapsed, FStrokeSyncParams P)$ double
        +FacingYaw(FVector2D HitPoint, FVector2D AimPoint, double SideSign, FStrokeSyncParams P)$ double
        +BodyTarget(FVector2D BallPos, double YawRad, FVector2D SweetSpotOffset)$ FVector2D
        +ReturnYaw(double Yaw, double TargetYaw, double RateRad, double Dt)$ double
    }
    class FStrokeSyncParams {
        +double MinPlayRate
        +double MaxPlayRate
        +double ReplanInterval
        +double MaxRateChangePerSecond
        +double HitRefineWindow
        +double MaxFacingDeg
        +double NeutralAimDepth
        +double ImpactReach
        +double SwingLead
        +double LowerBodyBlendTime
        +double ReturnYawRateDeg
    }
    class TennisStrokeImpact {
        <<namespace>>
        +HandedOffset(FVector SweetSpotOffset, ETennisHandedness Hand)$ FVector
        +BodyAt(FVector2D FrameStartPos, FVector2D Velocity, double FrameStartTime, double T)$ FVector2D
        +SweetSpotAt(FVector2D BodyPos, double YawRad, FVector Offset)$ FVector
        +IsWithinReach(double Distance, FStrokeSyncParams P)$ bool
    }
    TennisStrokeImpact ..> FStrokeSyncParams
    class FStrokeImpact {
        <<struct>>
        +double Time
        +FVector SweetSpot
    }
    UTennisStrokeComponent ..> FStrokeImpact : OnImpact
    UTennisStrokeComponent ..> TennisStrokeImpact : T のスイートスポット
    ATennisCharacter ..> TennisStrokeImpact : 判定
    ATennisCharacter ..> FTennisShotSolver : M3 の打球
    class FStrokeStartPlan {
        +double Delay
        +double StartPosition
        +double PlayRate
    }
    TennisStrokeTiming ..> FStrokeSyncParams
    TennisStrokeTiming ..> FStrokeStartPlan
    UTennisStrokeComponent ..> TennisStrokeTiming
    class TennisStrokeStep {
        <<namespace>>
        +Begin(FStrokeStepper S, FStrokeClip C, double Available, double StartYaw, double TargetYaw, FStrokeSyncParams P)$ bool
        +Advance(FStrokeStepper S, int32 NumSteps, double AvailableAtEnd, double NetYaw, FStrokeSyncParams P)$ FStrokeStepEvents
        +Cancel(FStrokeStepper S)$ bool
        +LowerBodyLocomotionAlpha(FStrokeStepper S, FStrokeSyncParams P)$ double
    }
    class FStrokeStepper {
        <<struct>>
        +EStrokeState State
        +FStrokeClip Clip
        +double Delay
        +double StartPosition
        +double Position
        +double PlayRate
        +int32 StepsSinceReplan
        +double StartYaw
        +double TargetYaw
        +double Yaw
    }
    class FStrokeClip {
        <<struct>>
        +double ImpactTime
        +double RecoverTime
    }
    class FStrokeStepEvents {
        <<struct>>
        +bool bImpact
        +double ImpactLead
        +bool bEnded
    }
    TennisStrokeStep ..> FStrokeStepper
    TennisStrokeStep ..> FStrokeStepEvents
    TennisStrokeStep ..> TennisStrokeTiming
    FStrokeStepper *-- FStrokeClip
    UTennisStrokeComponent ..> TennisStrokeStep
    class UTennisStrokeComponent {
        +FStrokeSyncParams Sync
        -TObjectPtr~UStrokeData~ Stroke
        -FStrokeStepper Stepper
        -FHitCandidate Hit
        -int64 LastStepCount
        -FVector ImpactOffset
        +FOnStrokeImpact OnImpact
        +OnFinished FSimpleMulticastDelegate
        +PlayStroke(FHitCandidate C, ETennisShotType Type) bool
        +CancelStroke() bool
        +IsStroking() bool
        +GetStepper() FStrokeStepper
        +GetPlannedHit() FHitCandidate
        +GetStroke() UStrokeData
        -ApplyToBody() void
    }
    class UTennisStaminaComponent {
        +double Stamina
        +Consume(EStaminaUse U, double Amount) void
        +RecoverBetweenPoints(double Seconds) void
        +GetSpeedFactor() double
        +GetSigmaFactor() double
    }
    class UTennisPlayerAttributesComponent {
        +FPlayerAttributes Attr
        +GetShotSkill(ETennisShotType T) FPlayerShotSkill
        +GetMoveParams() FMoveParams
    }
    class UTennisRacketComponent {
        +FName GripSocket$
        +FName SweetSpotSocket$
        +SetRacketMesh(UStaticMesh M) bool
        +GetSweetSpotWorld() FVector
    }
    class UTennisPlayerAppearance {
        +TSoftObjectPtr~USkeletalMesh~ Body
        +TArray~TSoftObjectPtr_USkeletalMesh~ Followers
        +TArray~TSoftObjectPtr_UGroomBindingAsset~ GroomBindings
        +TArray~FTennisStrokeFit~ StrokeFits
        +FTennisReachBody ReachBody
        +FindStrokeFit(UStrokeData S) FTennisStrokeFit
    }
    class UTennisPlayerSettings {
        +TSoftObjectPtr~UTennisPlayerAppearance~ Appearance
        +TSoftClassPtr~UAnimInstance~ AnimClass
        +TSoftObjectPtr~UStaticMesh~ RacketMesh
        +TSoftObjectPtr~UChooserTable~ StrokeChooser
        +TArray~TSoftObjectPtr_UShotTypeData~ ShotTypes
        +FindShotType(ETennisShotType Type) UShotTypeData
    }
    class UTennisAnimInstance {
        +FTransformTrajectory Trajectory
        +float LowerBodyLocomotionAlpha
        +NativeUpdateAnimation(float DeltaSeconds) void
    }
    class UAnimNotify_TennisImpact {
        +Notify(USkeletalMeshComponent M, UAnimSequenceBase A, FAnimNotifyEventReference R) void
    }
    class FStrokeSelectRow {
        <<struct>>
        +EStrokeSide Side
        +TArray~EBackhandStyle~ BackhandStyles
        +TArray~EStrokeHeightBand~ Heights
        +TArray~EStrokeReach~ Reaches
        +TArray~EStrokeFamily~ Families
        +TArray~EStrokeStance~ Stances
        +bool bHasHitHeightMin
        +double HitHeightMin
        +bool bHasHitHeightMax
        +double HitHeightMax
    }
    class FTennisStrokeChooserInput {
        <<struct>>
        +EStrokeSide Side
        +EBackhandStyle BackhandStyle
        +EStrokeHeightBand Height
        +double HitHeight
        +EStrokeReach Reach
        +EStrokeFamily Family
        +EStrokeStance Stance
        +float TimeBudget
    }
    class TennisStrokeSelect {
        <<namespace>>
        +HeightBandOf(double Z)$ EStrokeHeightBand
        +FamilyOf(EHitKind K, ETennisShotType T, double TimeBudget, double BlockTimeBudget)$ EStrokeFamily
        +ReachOf(bool bStretch)$ EStrokeReach
    }
    class UAnimNotify_TennisRecoverStart
    class UTennisStrokeMetaBaker {
        <<TennisEditor>>
        +Bake(UStrokeData S, FString OutError)$ bool
        +BakeStrokeFit(UStrokeData S, USkeletalMesh Body, UStaticMesh R, FTransform OutSweetSpot, FString OutError)$ bool
        +BakeAppearance(UTennisPlayerAppearance A, TArray~UStrokeData~ Strokes, UChooserTable Table, UStaticMesh R, FString OutError)$ bool
    }
    class UTennisStrokeChooserBuilder {
        <<TennisEditor>>
        +Rebuild()$ int32
    }
    class UTennisLocomotionDatabaseBuilder {
        <<TennisEditor>>
        +CreateSchema(FString PackageName, USkeleton Skeleton)$ UPoseSearchSchema
        +CreateDatabase(FString PackageName, UPoseSearchSchema Schema)$ UPoseSearchDatabase
        +SetAnimations(UPoseSearchDatabase Database, TArray~UAnimSequence~ Sequences)$ int32
    }
    class UStrokeData {
        +TObjectPtr~UAnimMontage~ Montage
        +TArray~FStrokeSelectRow~ SelectRows
        +double ImpactTime
        +double WindupTime
        +double ReleaseLeadTime
        +double ChargeTime
        +double FollowThroughLock
    }
    class ATennisPlayerController {
        -UTennisShotInputComponent ShotInput
        +GetShotInput() UTennisShotInputComponent
        +StickToWorldDirection(FVector2D Stick, double ForwardYawDegrees)$ FVector
        +NetwardYawDegrees(double CourtYawDegrees, double Side)$ double
        #OnPossess(APawn P) void
        #OnUnPossess() void
        -OnMove(FInputActionValue V) void
        +ApplyInputPhase(ETennisInputPhase P) bool
    }
    class UTennisShotInputComponent {
        -FTennisShotInputMachine Machine
        -TArray~FShotButtonTypes~ ButtonTypes
        +BindInput(UEnhancedInputComponent Input) void
        +OnShotPressed(int32 Button) void
        +OnShotReleased(int32 Button) void
        +OnTechnique(bool bHeld) void
        +Cancel() void
        +BindCharacter(ATennisCharacter C) void
        +GetState() EShotInputState
        -Publish(bool bChanged, TCHAR What) void
        -OnCandidateFound() void
        -OnStrokeEnded() void
        -OnStrokeImpact() void
        +AimFromStick(FVector2D S, int32 Side) FVector2D
    }
    class FTennisShotInputMachine {
        +double DefaultBufferSeconds$
        -double BufferSeconds
        -EShotInputState State
        -FShotIntent Intent
        -int32 HeldButton
        -FShotButtonTypes HeldTypes
        -bool bTechniqueHeld
        +Press(int32 Button, FShotButtonTypes T, double Now) bool
        +Release(int32 Button, double Now) bool
        +SetTechnique(bool bHeld) bool
        +CandidateFound(double Now) bool
        +Impact() bool
        +StrokeEnded() bool
        +Update(double Now) bool
        +Cancel() bool
        +GetState() EShotInputState
        +GetIntent() FShotIntent
    }
    class EShotInputState {
        <<enumeration>>
        Idle
        Buffered
        Charging
        Released
        Committed
    }
    class FShotButtonTypes {
        <<struct>>
        +ETennisShotType Plain
        +ETennisShotType WithTechnique
    }
    class UTennisInputSettings {
        +TSoftObjectPtr~UInputMappingContext~ RallyContext
        +FTennisPhaseContext GlobalContext
        +TMap PhaseContexts
        +int32 RallyContextPriority
        +TSoftObjectPtr~UInputAction~ MoveAction
        +TArray~FTennisShotButton~ RallyShotButtons
        +TSoftObjectPtr~UInputAction~ TechniqueAction
        +double InputBufferSeconds
    }
    class FTennisShotButton {
        <<struct>>
        +TSoftObjectPtr~UInputAction~ Action
        +ETennisShotType Type
        +ETennisShotType TechniqueType
    }
    class UTennisSwipeRecognizer {
        +Begin(FVector2D P, double T) void
        +Add(FVector2D P, double T) void
        +End(double T) FSwipeResult
    }
    class UStaticMeshComponent
    class UDeveloperSettings
    class UTennisBallSimComponent
    class ATennisCourt
    ACharacter <|-- ATennisCharacter
    UCharacterMovementComponent <|-- UTennisMovementComponent
    APlayerController <|-- ATennisPlayerController
    UStaticMeshComponent <|-- UTennisRacketComponent
    UDeveloperSettings <|-- UTennisPlayerSettings
    UPrimaryDataAsset <|-- UTennisPlayerAppearance
    UTennisPlayerSettings ..> UTennisPlayerAppearance : default
    UDeveloperSettings <|-- UTennisInputSettings
    UAnimInstance <|-- UTennisAnimInstance
    UAnimNotify <|-- UAnimNotify_TennisImpact
    UAnimNotify <|-- UAnimNotify_TennisRecoverStart
    UTennisStrokeMetaBaker ..> UStrokeData : Bake
    UStrokeData *-- FStrokeSelectRow
    UTennisStrokeChooserBuilder ..> UStrokeData : reads SelectRows
    UTennisStrokeComponent ..> FTennisStrokeChooserInput : PlayStroke
    UTennisStrokeComponent ..> TennisStrokeSelect
    ATennisCharacter *-- UTennisMovementComponent
    UTennisMovementComponent ..> TennisMovement : CalcVelocity
    UTennisReachComponent ..> TennisReach : FindCandidates
    UTennisReachComponent o-- UTennisBallSimComponent : prediction
    UTennisMovementComponent o-- ATennisCourt : court
    ATennisCharacter ..> UTennisPlayerSettings : appearance
    ATennisCharacter ..> UTennisPlayerAppearance : body and followers
    TennisReach ..> FHitCandidate
    TennisReach ..> TennisStrokeTiming : RefineForStroke
    TennisReach ..> TennisStrokeImpact : HandedOffset
    TennisReach ..> FStrokeSyncParams
    ATennisCharacter *-- UTennisReachComponent
    ATennisCharacter *-- UTennisStrokeComponent
    ATennisCharacter *-- UTennisStaminaComponent
    ATennisCharacter *-- UTennisPlayerAttributesComponent
    ATennisCharacter *-- UTennisRacketComponent
    ATennisPlayerController *-- UTennisShotInputComponent
    UTennisShotInputComponent *-- FTennisShotInputMachine
    UTennisShotInputComponent ..> UTennisInputSettings : BindInput
    UTennisInputSettings *-- FTennisShotButton
    FTennisShotInputMachine ..> EShotInputState
    FTennisShotInputMachine ..> FShotButtonTypes
    ATennisPlayerController ..> ATennisCharacter : AddMovementInput
    UTennisShotInputComponent o-- UTennisSwipeRecognizer
    UTennisShotInputComponent ..> ATennisCharacter : SubmitShotIntent
    UTennisStrokeComponent ..> UTennisReachComponent
    UTennisStrokeComponent o-- UStrokeData
    UTennisStrokeComponent ..> FTennisShotQuality
    UTennisStrokeComponent ..> FTennisShotSolver
    class FTennisShotQuality
    class FTennisShotSolver
```

- 今の流れ：`ATennisCharacter` が `UTennisReachComponent` に探させ（`StartSearching`）、見つかったら `OnHitCandidateFound` で入力に知らせてから、`UTennisStrokeComponent::PlayStroke` で振り始める（選び直した打点に入る位置へ自動移動。選べなければ終える）。インパクトでは、ストロークが T のスイートスポットを渡し（`FStrokeImpact`）、選手が判定して打ち（M3：球種のデータ `UTennisPlayerSettings::ShotTypes` で、ニュートラルの狙いへ。DD-03 §6.4）、最後に `OnStrokeImpact` で入力に知らせ、RecoverStart で `OnStrokeEnded` を知らせる（ST-05）。離した意図は記録するだけ（振っていなければすぐ終える）。取り消すと、インパクトの前ならストロークも止める（DD-03 §2.3・§6.6）
- ロコモーション（DD-03 §6.5）：`UTennisAnimInstance`（`ABP_TennisPlayer` の親）が、毎フレーム、軌跡（過去の履歴・今・`UTennisMovementComponent::PredictWorldPath`）と下半身の重み（ストロークのステッパから）を作る。取り消した後は、`UTennisStrokeComponent` が体の向きをネットへ戻す
- 選手の側（−1／+1）は、今は `UTennisMovementComponent::GetSideSign`（プレイの開始時の位置で決める）。ラケットは `UTennisRacketComponent`（手のソケット `RacketHandSocket` に付ける。DD-03 §7）
- 後で足すもの：`ATennisCharacter` の `PlayerIndex`・`CourtSide`・`Handedness`・`Backhand`・`Profile`・`GetSweetSpotWorld`・`GetBodyCapsule`、`UTennisMovementComponent` の `AssistWeight`（W-4.3）・`TriggerSplitStep`・`GetMaxSpeed`、`AimFromStick`（W-4.2）。`UTennisAnimInstance` のスプリットステップの合図（そのクリップができてから）と手の IK の目標（腕の微調整：M9）、`UTennisPlayerAttributesComponent`（W-6.1）、`UTennisStaminaComponent`（W-6.7）、`UTennisSwipeRecognizer`（W-10.4）

## 5. ゲームプレイ層：ルール・試合・統計（Tennis ＋ TennisSim）

```mermaid
classDiagram
    class AGameModeBase
    class AGameStateBase
    class ATennisGameMode {
        +UTennisMatchFlowComponent Flow
        +RequestHit(FHitRequest R) void
        +ConfirmHit(FHitRequest R) FBallLaunchEvent
    }
    class UTennisMatchFlowComponent {
        -ETennisMatchPhase Phase
        -FTennisRuleJudge Judge
        -FMatchRules Rules
        -double PhaseTimer
        +StartMatch(FMatchSetup S) void
        +OnBallEvent(FBallEvent E) void
        +OnHit(FHitInfo H) void
        -BeginPoint() void
        -EndPoint(FPointResult R) void
        -AdvancePhase(ETennisMatchPhase Next) void
    }
    class FTennisRuleJudge {
        -FPointContext Ctx
        -EPointPhase PointPhase
        -int32 LastHitter
        -int32 BouncesSinceHit
        -bool bFirstBounceGood
        -bool bNetTouchedThisFlight
        -bool bCrossedNetThisFlight
        -FPointResult Result
        +BeginPoint(FPointContext C) void
        +OnHit(FHitInfo H) ERuleVerdict
        +OnBallEvent(FBallEvent E) ERuleVerdict
        +OnPlayerEvent(FPlayerRuleEvent E) ERuleVerdict
        +GetResult() FPointResult
    }
    class FPointContext {
        <<struct>>
        +int32 Server
        +EServeSide Side
        +bool bSecondServe
        +bool bNoLet
        +FCourtGeometry Court
    }
    class FPointResult {
        <<struct>>
        +int32 Winner
        +ETennisPointReason Reason
        +FName RuleRef
        +int32 RallyLength
        +int32 LastHitter
        +bool bWinnerShot
    }
    class FTennisScore {
        <<struct>>
        +int32 Points[2]
        +int32 Games[2]
        +int32 Sets[2]
        +TArray~FIntPoint~ SetHistory
        +ETieBreakKind TieBreak
        +int32 Server
        +int32 TieBreakFirstServer
        +int32 EndsSwapped
    }
    class FMatchRules {
        <<struct>>
        +int32 SetsToWin
        +int32 GamesPerSet
        +bool bNoAd
        +bool bNoLet
        +int32 TieBreakAt
        +int32 TieBreakTo
        +EFinalSetMode FinalSet
        +bool bShortSet
        +ETBEndChange TBEndChange
        +EBallChange BallChange
    }
    class FTennisScoreRules {
        +AwardPoint(FTennisScore S, int32 W, FMatchRules R)$ FScoreDelta
        +GetServeInfo(FTennisScore S, FMatchRules R)$ FServeInfo
        +ShouldChangeEnds(FTennisScore B, FTennisScore A, FMatchRules R)$ bool
        +GetSituation(FTennisScore S, FMatchRules R)$ EScoreSituation
        +GetCallText(FTennisScore S, ECallLanguage L)$ FText
    }
    class ATennisGameState {
        +FTennisScore Score
        +ETennisMatchPhase Phase
        +int32 RallySeq
        +int32 MatchSeed
        +UTennisMatchStatsComponent Stats
        +MulticastLaunch(FBallLaunchEvent E) void
        +MulticastBounce(FBallBounceNetEvent E) void
        +OnRep_Score() void
    }
    class UTennisMatchStatsComponent {
        +FPlayerMatchStats Players[2]
        +RecordShot(FShotRecord R) void
        +RecordPoint(FPointResult R) void
        +ClassifyError(FShotRecord R) EErrorKind
    }
    AGameModeBase <|-- ATennisGameMode
    AGameStateBase <|-- ATennisGameState
    ATennisGameMode *-- UTennisMatchFlowComponent
    UTennisMatchFlowComponent *-- FTennisRuleJudge
    UTennisMatchFlowComponent *-- FMatchRules
    UTennisMatchFlowComponent ..> FTennisScoreRules
    FTennisRuleJudge ..> FPointContext
    FTennisRuleJudge ..> FPointResult
    FTennisScoreRules ..> FTennisScore
    ATennisGameState *-- FTennisScore
    ATennisGameState *-- UTennisMatchStatsComponent
    UTennisMatchFlowComponent ..> ATennisGameState : writes
```

- 今の `ATennisGameMode` は、既定のポーン（`ATennisCharacter`）とコントローラー（`ATennisPlayerController`）を決めるだけ。図のほか（試合の流れ・ルール・スコア）は M5（W-5.1〜W-5.8）、統計は W-7.9 で足す

## 6. ゲームプレイ層：AI（Tennis）

```mermaid
classDiagram
    class AAIController
    class ATennisAIController {
        +UStateTreeAIComponent Brain
        +TObjectPtr~UAIDifficultyData~ Difficulty
        +TObjectPtr~UAIPlayStyleData~ Style
        +FTennisOpponentModel Opponent
        +FRandomStream Rng
    }
    class UStateTreeAIComponent
    class FTennisRallyContext {
        <<struct>>
        +FTrajectory Perceived
        +double ReactionReadyTime
        +double PredictionNoise
        +FVector OpponentPos
        +double OpponentBalance
        +EScoreSituation Situation
    }
    class FSTE_TennisRallyContext {
        <<StateTree evaluator>>
        +Tick() void
    }
    class FSTT_TennisServe {
        <<StateTree task>>
        +EnterState() EStateTreeRunStatus
        +Tick() EStateTreeRunStatus
    }
    class FSTT_TennisRecover {
        <<StateTree task>>
        +ComputeRecoverPoint() FVector
    }
    class FSTT_TennisIntercept {
        <<StateTree task>>
        +ChooseHitPoint() FHitCandidate
    }
    class FSTT_TennisChooseShot {
        <<StateTree task>>
        +ScoreCandidates() TArray~FShotOption~
        +PickSoftmax(double Temperature) FShotOption
    }
    class FTennisShotTable {
        +Lookup(ETennisShotType T, double ContactZ, double Depth, double Speed) FShotEstimate
        +Build(FBallSimContext Ctx)$ FTennisShotTable
    }
    class FTennisOpponentModel {
        +RecordShot(FVector2D Land, ETennisShotType T) void
        +PredictBias() FVector2D
    }
    class UAIDifficultyData {
        +double ReactionTime
        +double SpeedScale
        +double PredictionNoise0
        +double TimingSigma
        +double AimMargin
        +double RiskAppetite
        +double MeanCharge
        +double SoftmaxTemperature
    }
    class UAIPlayStyleData {
        +double NetApproachBias
        +double DropShotBias
        +double FlatBias
        +double SpinBias
        +double ServeRiskBias
        +TMap~ETennisShotType,double~ ShotWeights
    }
    AAIController <|-- ATennisAIController
    ATennisAIController *-- UStateTreeAIComponent
    ATennisAIController o-- UAIDifficultyData
    ATennisAIController o-- UAIPlayStyleData
    ATennisAIController *-- FTennisOpponentModel
    FSTE_TennisRallyContext ..> FTennisRallyContext : fills
    FSTT_TennisIntercept ..> FTennisRallyContext
    FSTT_TennisChooseShot ..> FTennisShotTable
    FSTT_TennisChooseShot ..> FShotIntent : produces
    FSTT_TennisServe ..> FShotIntent : produces
    class FShotIntent
```

## 7. プレゼンテーション層（Tennis）

```mermaid
classDiagram
    class UWorldSubsystem
    class UGameInstanceSubsystem
    class APlayerCameraManager
    class UTennisEventSubsystem {
        +FOnBallLaunched OnBallLaunched
        +FOnBallBounce OnBallBounce
        +FOnBallNetTouch OnBallNetTouch
        +FOnImpact OnImpact
        +FOnPointStarted OnPointStarted
        +FOnPointEnded OnPointEnded
        +FOnScoreChanged OnScoreChanged
        +FOnMatchPhaseChanged OnMatchPhaseChanged
        +FOnChangeEnds OnChangeEnds
        +Broadcast(FBallEvent E) void
    }
    class ATennisPlayerCameraManager {
        -TObjectPtr~UTennisCameraModeBase~ Mode
        -TWeakObjectPtr~ATennisBall~ Ball
        +GetMode() UTennisCameraModeBase
        #UpdateViewTarget(FTViewTarget T, float Dt) void
        -MakeContext(FTennisCameraContext Out) bool
    }
    class FTennisCameraContext {
        <<struct>>
        +FTransform CourtTransform
        +FVector Player
        +FVector Ball
        +double Side
        +double ViewAspect
    }
    class UTennisCameraDirector {
        -ETennisCameraMode Mode
        -TMap~ETennisCameraMode,UTennisCameraModeBase~ Modes
        +SetMode(ETennisCameraMode M, float Blend) void
        +OnChangeEnds() void
    }
    class UTennisCameraModeBase {
        <<abstract>>
        +Evaluate(FTennisCameraContext C, float Dt) FMinimalViewInfo
        +Reset() void
    }
    class UTennisCameraMode_Broadcast {
        +FTennisBroadcastCameraParams Params
        -FTennisCameraSpring Location
        -FTennisCameraSpring LookAt
        -double LastSide
        +Evaluate(FTennisCameraContext C, float Dt) FMinimalViewInfo
        +Reset() void
    }
    class FTennisBroadcastCameraParams {
        <<struct>>
        +double BehindBaseline
        +double Height
        +double FollowY
        +double BallWeight
        +double FovDeg
        +double MaxFovDeg
        +double EdgeMargin
        +double LocationOmega
        +double LookAtOmega
    }
    class FTennisCameraSpring {
        <<struct>>
        +FVector Value
        +FVector Velocity
        +Reset(FVector Target) void
        +Step(FVector Target, double Omega, double Dt) void
    }
    class TennisBroadcastCamera {
        <<namespace>>
        +Desired(FTennisBroadcastCameraParams P, double Side, FVector Player, FVector Ball, FVector OutLocation, FVector OutLookAt)$ void
        +OpponentBaselineEnds(double Side, FVector OutA, FVector OutB)$ void
        +ProjectToScreen(FVector Location, FRotator Rotation, double FovDeg, double RefAspect, double Aspect, FVector Point)$ TOptional~FVector2D~
        +ConstrainedRotation(FTennisBroadcastCameraParams P, double Side, double Aspect, FVector Location, FVector LookAt, FVector Player, double OutFovDeg)$ FRotator
    }
    class UTennisCameraMode_Low
    class UTennisCameraMode_BehindPlayer
    class UTennisCameraMode_Replay
    class UTennisPresentationSubsystem {
        +UTennisUmpireComponent Umpire
        +UTennisCrowdComponent Crowd
        +UTennisAudioDirector Audio
    }
    class UTennisUmpireComponent {
        +CallScore(FTennisScore S) void
        +CallVerdict(ERuleVerdict V) void
    }
    class UTennisCrowdComponent {
        +double Excitement
        +OnRallyShot(int32 RallyLength) void
        +OnPointEnded(FPointResult R, EScoreSituation S) void
    }
    class UTennisAudioDirector {
        +PlayHit(FImpactEvent E) void
        +PlayBounce(FBallEvent E) void
        +SetMixState(EMixState S) void
    }
    class UTennisReplaySubsystem {
        -FPointRecording Current
        -TArray~FPointRecording~ Highlights
        +BeginRecording() void
        +EndRecording(FPointResult R) void
        +Play(FPointRecording R, float Speed) void
    }
    class UTennisHapticsService {
        +PlayImpact(double Quality, double Speed) void
    }
    UWorldSubsystem <|-- UTennisEventSubsystem
    UWorldSubsystem <|-- UTennisPresentationSubsystem
    UWorldSubsystem <|-- UTennisReplaySubsystem
    UGameInstanceSubsystem <|-- UTennisHapticsService
    APlayerCameraManager <|-- ATennisPlayerCameraManager
    ATennisPlayerCameraManager *-- UTennisCameraModeBase : Mode
    ATennisPlayerCameraManager ..> FTennisCameraContext : makes
    UTennisCameraModeBase ..> FTennisCameraContext : reads
    ATennisPlayerCameraManager *-- UTennisCameraDirector
    UTennisCameraDirector *-- UTennisCameraModeBase
    UTennisCameraModeBase <|-- UTennisCameraMode_Broadcast
    UTennisCameraMode_Broadcast *-- FTennisBroadcastCameraParams
    UTennisCameraMode_Broadcast *-- FTennisCameraSpring
    UTennisCameraMode_Broadcast ..> TennisBroadcastCamera
    UTennisCameraModeBase <|-- UTennisCameraMode_Low
    UTennisCameraModeBase <|-- UTennisCameraMode_BehindPlayer
    UTennisCameraModeBase <|-- UTennisCameraMode_Replay
    UTennisPresentationSubsystem *-- UTennisUmpireComponent
    UTennisPresentationSubsystem *-- UTennisCrowdComponent
    UTennisPresentationSubsystem *-- UTennisAudioDirector
    UTennisPresentationSubsystem ..> UTennisEventSubsystem : subscribes
    UTennisReplaySubsystem ..> UTennisEventSubsystem : subscribes
    UTennisCameraDirector ..> UTennisEventSubsystem : subscribes
```

- 中継カメラの基本（W-3.8）では、`ATennisPlayerCameraManager` が `UTennisCameraMode_Broadcast` を 1 つだけ（`Mode`）持ち、操作する選手・コート・ボールから `FTennisCameraContext`（コート局所の m）を作って評価する。選手が替わったら `Reset` する。選手かコートがなければ、エンジンの既定の視点にする（警告は `LogTennisCamera`）
- 中継カメラの計算（目標の位置と注視点、ばね、画面への投影、相手のベースラインと選手を画面に入れる向きと FOV の制約）は `TennisBroadcastCamera`（DD-07 §1.1）。演出の値なので、フレームの DeltaTime を使う
- 後で足すもの：モードの切り替え（`UTennisCameraDirector`。`Mode` はここへ移す）とエンドチェンジの反転は W-5.10、`UTennisCameraMode_Low`・`_BehindPlayer` は W-7.1、`_Replay` は W-7.2。`UTennisEventSubsystem` と演出・リプレイ・振動の型も、まだない

## 8. ゲームインスタンス・セーブ・プラットフォーム（Tennis）

```mermaid
classDiagram
    class UGameInstance
    class UGameInstanceSubsystem
    class USaveGame
    class UGameUserSettings
    class UTennisGameInstance {
        +GetMatchSetup() UTennisMatchSetupSubsystem
    }
    class UTennisMatchSetupSubsystem {
        +FMatchSetup Setup
        +SetPlayers(UPlayerProfileData A, UPlayerProfileData B) void
        +SetVenue(UVenueData V) void
        +SetRules(UMatchRulesData R) void
        +SetOpponentKind(EOpponentKind K) void
    }
    class UTennisSaveSubsystem {
        +LoadProfile() UTennisProfileSave
        +SaveProfile() bool
        +SaveMatchSnapshot(FMatchSnapshot S) bool
        +SyncCloud() void
    }
    class UTennisProfileSave {
        +int32 SaveVersion
        +FCareerStats Stats
        +TArray~FPointRecording~ SavedReplays
        +FTouchLayout TouchLayout
        +FMatchSnapshot Suspended
        +Migrate(int32 From) void
    }
    class UTennisUserSettings {
        +ETennisAssistPreset Assist
        +ETouchScheme TouchScheme
        +ESpeedUnit SpeedUnit
        +FText Language
        +float MasterVolume
        +bool bReduceFlashes
    }
    class UTennisPlatformSubsystem {
        +OnEnterBackground() void
        +OnEnterForeground() void
        +GetInputKind() ECommonInputType
    }
    UGameInstance <|-- UTennisGameInstance
    UGameInstanceSubsystem <|-- UTennisMatchSetupSubsystem
    UGameInstanceSubsystem <|-- UTennisSaveSubsystem
    UGameInstanceSubsystem <|-- UTennisPlatformSubsystem
    USaveGame <|-- UTennisProfileSave
    UGameUserSettings <|-- UTennisUserSettings
    UTennisSaveSubsystem ..> UTennisProfileSave
    UTennisPlatformSubsystem ..> UTennisSaveSubsystem : autosave
```

## 9. 開発用（Tennis）

```mermaid
classDiagram
    class AActor
    class UDeveloperSettings
    class ATennisShotTester {
        #TObjectPtr~ATennisBall~ Ball
        #TArray~UShotTypeData~ ShotTypes
        #FVector HitPoint
        #double Charge
        +ShootAt(FVector TargetLocal) bool
        +ShootWith(UShotTypeData Type, FVector TargetLocal) bool
        +NextShotType() void
        +Shoot(UTennisBallSimComponent Sim, FShotTypeParams P, FVector HitPoint, FVector Target, double Charge)$ FShotSolveResult
        +GroundPointOnRay(FVector Start, FVector Direction)$ TOptional~FVector~
    }
    class UTennisDebugSettings {
        -int32 DebugTrajectory
        -float TimeScale
        +GetTrajectoryMode() int32
        +GetTimeScale() float
        +RegisterConsoleCommands()$ void
        +UnregisterConsoleCommands()$ void
    }
    class ATennisBall
    class UShotTypeData
    class UTennisBallSimComponent
    class FTennisShotSolver
    class TennisShotRequestMapping {
        <<namespace>>
    }
    AActor <|-- ATennisShotTester
    UDeveloperSettings <|-- UTennisDebugSettings
    ATennisShotTester o-- ATennisBall
    ATennisShotTester o-- UShotTypeData
    ATennisShotTester ..> TennisShotRequestMapping : Make
    ATennisShotTester ..> FTennisShotSolver : Solve
    ATennisShotTester ..> UTennisBallSimComponent : Launch
    UTennisBallSimComponent ..> UTennisDebugSettings : time scale, trajectory
```

- `ATennisShotTester` は打球ツール（FR-DBG-003）。PIE でクリックしたコートの地点へ、選んだ球種で打つ。テストマップに置き、MCP からは `ShootWith` を呼ぶ（Tools/Unreal の `shoot_ball`）
- `UTennisDebugSettings` は Project Settings の Tennis Debug。コンソールコマンド `tennis.Debug.Trajectory`・`tennis.TimeScale` は、このプロパティを直接読み書きする（ARCH-04 §3.1）
