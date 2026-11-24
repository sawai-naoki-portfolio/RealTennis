"""RealTennis のプロジェクト専用の Unreal MCP ツールセット。Content/Python/init_unreal.py から register() を呼ぶ。"""


def register() -> bool:
    """ツールセットを MCP のレジストリに登録する。レジストリがなければ False（エディタは止めない）。"""
    from toolset_registry.registration import Registration

    from .tools import TennisTools

    return Registration([TennisTools]).register()
