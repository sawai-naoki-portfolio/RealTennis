"""MetaHuman の選手（W-3.10。ART-03 §6、ART-01 A-CHR-010・011）の値と場所、ツールの入力の検証（unreal に依存しない。CI で単体テストする）。

顔は Core Data のプリセットを写したもの（利用者の判断、2026-11-14：男性 M01 は Kelvin、女性 F01 は Celeste）。体は Parametric Body の
測定値をリグと組み立ての前に当てる。髪はプリセットのまま、または Core Data の髪の Wardrobe Item に替える。服は外して組み立てる
（テニスウェアは Blender で組み立てた体の殻として作り、WEAR_FOLDER の SKM_Wear_<key> として取り込む：ART-03 §6 の手順 5）。
組み立てた共通のアセットは全員で 1 つの COMMON_FOLDER に置く。
"""
from __future__ import annotations

import re
from dataclasses import dataclass

METAHUMAN_FOLDER = "/Game/Tennis/Characters/Players/MetaHuman"  # 編集用の MHC_Player_* と、組み立てた <MHC_Player_*>/ を置く
COMMON_FOLDER = f"{METAHUMAN_FOLDER}/Common"  # 組み立ての共通のアセット（骨格・コントロールリグ・マテリアル。全員で 1 つ）
SKELETON = f"{COMMON_FOLDER}/Female/Medium/NormalWeight/Body/metahuman_base_skel"  # 組み立てが作る体の骨格（全員で 1 つ）
WEAR_FOLDER = f"{METAHUMAN_FOLDER}/Wear"  # テニスウェア（A-CHR-040〜043）。選手ごとに 1 つの SKM_Wear_<key>
PRESET_FOLDER = "/MetaHumanCharacter/Optional/Presets"  # Core Data のプリセット（MetaHuman Character）
HAIR_FOLDER = "/MetaHumanCharacter/Optional/Grooms/Bindings/Hair"  # Core Data の髪の Wardrobe Item

HAIR_SLOT = "Hair"
OUTFIT_SLOT = "Outfits"

# Parametric Body の測定値の名前と範囲（UE 5.8 の get_body_constraints。2026-11-18 に確かめた）。ここにない名前は当てない
BODY_RANGES: dict[str, tuple[float, float]] = {
    "Height": (139.3, 227.0),                # cm
    "Masculine/Feminine": (-2.0, 2.0),       # 負が男性的
    "Muscularity": (-2.3, 2.3),
    "Fat": (-2.8, 2.8),
}

_KEY = re.compile(r"^[MF][0-9]{2}$")
_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")


class MetaHumanPlayerError(ValueError):
    """選手の値・ツールの入力が決まりに合わない。"""


@dataclass(frozen=True)
class MetaHumanPlayer:
    key: str                               # "M01"（男性 M、女性 F と 2 桁）
    preset: str                            # 顔の元の Core Data のプリセット（PRESET_FOLDER の MetaHuman Character）
    body: tuple[tuple[str, float], ...]    # 当てる測定値（名前, 値）。BODY_RANGES の名前と範囲
    hair: str | None = None                # 替える髪の Wardrobe Item（HAIR_FOLDER）。None はプリセットのまま

    @property
    def character(self) -> str:
        return f"MHC_Player_{self.key}"

    @property
    def character_path(self) -> str:
        return f"{METAHUMAN_FOLDER}/{self.character}"

    @property
    def build_folder(self) -> str:
        """組み立てた体・顔・髪などが入るフォルダ（組み立ては build_path の下に、キャラクターの名前のフォルダを作る）。"""
        return f"{METAHUMAN_FOLDER}/{self.character}"

    @property
    def body_mesh(self) -> str:
        """組み立てた体のメッシュの名前（Blender で服を作る元にもする：player_base）。"""
        return f"SKM_{self.character}_BodyMesh"

    @property
    def body_mesh_path(self) -> str:
        return f"{self.build_folder}/Body/{self.body_mesh}"

    @property
    def face_mesh_path(self) -> str:
        return f"{self.build_folder}/Face/SKM_{self.character}_FaceMesh"

    @property
    def wear(self) -> str:
        """テニスウェアのメッシュの名前（Blender の build_metahuman_wear.py が書き出す FBX の名前と同じ）。"""
        return f"SKM_Wear_{self.key}"

    @property
    def wear_path(self) -> str:
        return f"{WEAR_FOLDER}/{self.wear}"

    @property
    def appearance(self) -> str:
        return f"DA_Appearance_Player_{self.key}"

    @property
    def appearance_path(self) -> str:
        return f"{METAHUMAN_FOLDER}/{self.appearance}"

    @property
    def preset_path(self) -> str:
        return f"{PRESET_FOLDER}/{self.preset}"

    @property
    def hair_path(self) -> str | None:
        return f"{HAIR_FOLDER}/{self.hair}" if self.hair else None


# A-CHR-010・011（ART-01 §3）：男性は身長 185 cm・筋肉質、女性は身長 175 cm・筋肉質。筋肉の値は調整（PIE で見て決め直す）
PLAYERS: tuple[MetaHumanPlayer, ...] = (
    MetaHumanPlayer("M01", "Kelvin", (("Height", 185.0), ("Muscularity", 1.2))),
    MetaHumanPlayer("F01", "Celeste", (("Height", 175.0), ("Muscularity", 1.0)), "WI_Hair_S_LowPonytail"),
)


def validate(player: MetaHumanPlayer) -> None:
    """選手の値が決まりに合うか。合わなければ MetaHumanPlayerError。"""
    if not _KEY.match(player.key):
        raise MetaHumanPlayerError(f"key must be M or F and two digits: {player.key!r}")
    if not _NAME.match(player.preset):
        raise MetaHumanPlayerError(f"preset must be an asset name: {player.preset!r}")
    if player.hair is not None and not _NAME.match(player.hair):
        raise MetaHumanPlayerError(f"hair must be an asset name: {player.hair!r}")
    names = [name for name, _ in player.body]
    if len(names) != len(set(names)):
        raise MetaHumanPlayerError(f"{player.key}: a body measurement is given twice: {names}")
    for name, value in player.body:
        if name not in BODY_RANGES:
            raise MetaHumanPlayerError(f"{player.key}: unknown body measurement {name!r} (known: {sorted(BODY_RANGES)})")
        low, high = BODY_RANGES[name]
        if not (isinstance(value, (int, float)) and low <= value <= high):
            raise MetaHumanPlayerError(f"{player.key}: {name} {value!r} is outside {low}..{high}")


def check_built_paths(player: MetaHumanPlayer, body_path: str, skeleton_path: str) -> None:
    """組み立ての後に書き換える体のメッシュと骨格が、決まったフォルダ（その選手の組み立てのフォルダ・共通のフォルダ）の中か。
    外（プラグインの中・ほかのフォルダ）なら MetaHumanPlayerError（決まったフォルダの外を書き換えない）。"""
    if not body_path.startswith(player.build_folder + "/"):
        raise MetaHumanPlayerError(f"{player.key}: the body {body_path!r} is not in {player.build_folder}")
    if not skeleton_path.startswith(COMMON_FOLDER + "/"):
        raise MetaHumanPlayerError(f"{player.key}: the skeleton {skeleton_path!r} is not in {COMMON_FOLDER}")


def find_player(key: str) -> MetaHumanPlayer:
    """ツールの入力（選手のキー）から選手を引く。一覧にないキーは MetaHumanPlayerError（パスを組み立てる前に止める）。"""
    if not isinstance(key, str) or not _KEY.match(key):
        raise MetaHumanPlayerError(f"player key must be like 'M01' or 'F01': {key!r}")
    for player in PLAYERS:
        if player.key == key:
            validate(player)
            return player
    raise MetaHumanPlayerError(f"unknown player {key!r} (known: {[p.key for p in PLAYERS]})")
