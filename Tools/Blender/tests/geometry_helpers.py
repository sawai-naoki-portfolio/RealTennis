"""テストの補助：メッシュの体積とバウンディングボックス（純粋な関数）。"""


def signed_volume(verts, faces):
    """閉じた面の体積（面が外を向いていれば正）。多角形は扇形の三角形に分ける。"""
    total = 0.0
    for face in faces:
        a = verts[face[0]]
        for b, c in zip((verts[i] for i in face[1:-1]), (verts[i] for i in face[2:])):
            total += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0])
                      + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0
    return total


def bounds(verts):
    """(最小の角, 最大の角)。"""
    return [min(v[i] for v in verts) for i in range(3)], [max(v[i] for v in verts) for i in range(3)]
