#!/usr/bin/env python3
"""ballsim_reference.py — RES-02（ボール物理の調査報告）の数値実験を再現する参照スクリプト。

既存設計 UE5_Tennis_Technical_Design.md §3 のモデル（RK4・240 Hz・抗力・マグヌス・撃力バウンド）を
Python で再実装したもの。標準ライブラリのみで動く。

  py -I ballsim_reference.py   … 報告書の表の数値を出力する

報告書に書いた数値との照合は tests/test_ballsim_reference.py（CI で実行）。

注意：UE 側の実装の「正解」ではない。C++ 実装の自動テストの期待値を作るための参照用。
"""
from __future__ import annotations

import math
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

# ---- 定数（既存設計 §3.1） ----
M = 0.0577       # kg
R = 0.0335       # m
ALPHA = 0.55     # I = α m r²
G = 9.81
A = math.pi * R * R
DT = 1.0 / 240.0


# ---- ベクトル ----
def add(a, b): return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
def mul(a, s): return [a[0] * s, a[1] * s, a[2] * s]
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
def norm(a): return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


class Air:
    def __init__(self, cd=0.55, rho=1.21):
        self.cd, self.rho = cd, rho

    @property
    def kd(self):
        return 0.5 * self.rho * self.cd * A / M


def accel(v, w, air: Air):
    acc = [0.0, 0.0, -G]
    s = norm(v)
    if s < 1e-3:
        return acc
    acc = add(acc, mul(v, -air.kd * s))                         # 抗力
    wm = norm(w)
    if wm > 1e-2:
        S = R * wm / s
        cl = 1.0 / (2.0 + 1.0 / S)                                # C_L = 1/(2+1/S)
        km = 0.5 * air.rho * cl * A / M
        d = cross(w, v)
        dn = norm(d)
        if dn > 0:
            acc = add(acc, mul(d, km * s * s / dn))               # マグヌス
    return acc


def rk4(p, v, w, dt, air):
    a1 = accel(v, w, air); v1 = add(v, mul(a1, dt / 2))
    a2 = accel(v1, w, air); v2 = add(v, mul(a2, dt / 2))
    a3 = accel(v2, w, air); v3 = add(v, mul(a3, dt))
    a4 = accel(v3, w, air)
    p = add(p, mul(add(add(v, mul(v1, 2)), add(mul(v2, 2), v3)), dt / 6))
    v = add(v, mul(add(add(a1, mul(a2, 2)), add(mul(a3, 2), a4)), dt / 6))
    return p, v


def bounce(v, w, e, mu, et=0.0):
    """撃力のバウンド（既存設計 §3.5）＋接線方向の反発 e_t（RES-02 §3.4 の提案）。"""
    n = [0.0, 0.0, 1.0]
    I = ALPHA * M * R * R
    rc = [0.0, 0.0, -R]
    jn = -(1 + e) * M * v[2]
    vc = add(v, cross(w, rc))
    vct = [vc[0], vc[1], 0.0]
    jt = mul(vct, -(1 + et) * M * ALPHA / (1 + ALPHA))
    jmax = mu * jn
    slid = False
    if norm(jt) > jmax:
        jt = mul(jt, jmax / norm(jt))
        slid = True
    v = add(v, mul(add(mul(n, jn), jt), 1 / M))
    w = add(w, mul(cross(rc, jt), 1 / I))
    return v, w, slid


def topspin(rpm, sign=1.0):
    """+X に進むボールのトップスピン（Up × (+X) = +Y）。sign=-1 でバックスピン。"""
    return [0.0, sign * rpm * 2 * math.pi / 60, 0.0]


def flight(p, v, w, e, mu, air, et=0.0, tmax=4.0, bounces=1):
    t = 0.0
    info = {"netZ": None, "apex": p[2], "bounces": [], "hitnet": False}
    while t < tmax:
        pp = p[:]
        p, v = rk4(p, v, w, DT, air)
        t += DT
        info["apex"] = max(info["apex"], p[2])
        if pp[0] < 0 <= p[0]:
            f = -pp[0] / (p[0] - pp[0])
            z = pp[2] + (p[2] - pp[2]) * f
            info["netZ"] = z - R - 0.914
            if z + R < 0.914:
                info["hitnet"] = True
                return info
        if p[2] < R and v[2] < 0:
            vin = v[:]
            v, w, slid = bounce(v, w, e, mu, et)
            p[2] = R
            info["bounces"].append(dict(t=t, x=p[0], vin=vin, vout=v[:], slid=slid))
            if len(info["bounces"]) >= bounces:
                return info
    return info


def solve_elev(v0, rpm, origin, target_x, e, mu, air, top=True):
    """着地点が target_x になる仰角を二分法で求める（方位は +X 固定）。"""
    lo, hi = -15.0, 30.0
    w = topspin(rpm, 1.0 if top else -1.0)
    for _ in range(60):
        mid = (lo + hi) / 2
        th = math.radians(mid)
        info = flight(origin[:], [v0 * math.cos(th), 0, v0 * math.sin(th)], w[:], e, mu, air)
        if info["hitnet"] or not info["bounces"] or info["bounces"][0]["x"] < target_x:
            lo = mid
        else:
            hi = mid
    el = (lo + hi) / 2
    th = math.radians(el)
    return el, flight(origin[:], [v0 * math.cos(th), 0, v0 * math.sin(th)], w[:], e, mu, air, bounces=2), w


def time_to_x(v0, el, w, origin, x_end, e, mu, air):
    th = math.radians(el)
    p, v, ww = origin[:], [v0 * math.cos(th), 0, v0 * math.sin(th)], w[:]
    t = 0.0
    while t < 3.0:
        p, v = rk4(p, v, ww, DT, air)
        t += DT
        if p[2] < R and v[2] < 0:
            v, ww, _ = bounce(v, ww, e, mu)
            p[2] = R
        if p[0] >= x_end:
            break
    return t, p[2]


def drop_test(e, air, h=2.54):
    dt = 1 / 2400
    p, v, w = [0, 0, h + R], [0, 0, 0], [0, 0, 0]
    while p[2] >= R:
        p, v = rk4(p, v, w, dt, air)
    vin = v[2]
    p, v, top = [0, 0, R], [0, 0, -vin * e], R
    while v[2] > 0:
        p, v = rk4(p, v, w, dt, air)
        top = max(top, p[2])
    return abs(vin), 100 * (top - R)


def cpr_test(e, mu, et=0.0):
    """仮想 ITF CPR 試験：30 m/s・入射角 16°・回転なし。CPR = 100(1-μ) + 150(0.81-e)。"""
    v = [30 * math.cos(math.radians(16)), 0, -30 * math.sin(math.radians(16))]
    vo, _, slid = bounce(v, [0, 0, 0], e, mu, et)
    mu_app = (v[0] - vo[0]) / (-v[2] + vo[2])
    e_app = vo[2] / -v[2]
    return mu_app, e_app, 100 * (1 - mu_app) + 150 * (0.81 - e_app), slid


def category(c):
    return 1 if c < 30 else 2 if c < 35 else 3 if c < 40 else 4 if c < 45 else 5


SURFACES = [
    ("EXISTING Hard", 0.75, 0.60, 0.0), ("EXISTING Clay", 0.72, 0.80, 0.0),
    ("EXISTING Grass", 0.70, 0.45, 0.0), ("EXISTING Carpet", 0.73, 0.50, 0.0),
    ("PROPOSED Clay", 0.84, 0.90, 0.03), ("PROPOSED Hard-Medium", 0.80, 0.63, 0.0),
    ("PROPOSED Hard-MedFast", 0.79, 0.60, 0.0), ("PROPOSED Grass", 0.72, 0.62, 0.0),
    ("PROPOSED Indoor-Fast", 0.77, 0.55, 0.0),
]

BENCH = [  # 名前, 初速 m/s, rpm, トップスピンか, 打点, 目標の着地 x
    ("rally_fh", 34.7, 2300, True, [-11.9, 0, 0.95], 8.885),
    ("heavy_fh", 33.3, 3200, True, [-11.9, 0, 0.95], 8.885),
    ("flat_fh", 41.7, 800, True, [-11.0, 0, 1.0], 9.5),
    ("bh_slice", 27.8, 1500, False, [-11.9, 0, 0.8], 8.885),
    ("serve_flat", 51.9, 1200, True, [-11.985, 0.3, 2.9], 6.2),
    ("serve_kick", 40.7, 3200, True, [-11.985, 0.3, 2.75], 5.6),
]
HARD_E, HARD_MU = 0.80, 0.63


def compute():
    """報告書の表に載せた数値をすべて計算して辞書で返す。"""
    r = {}
    base = Air()
    a = accel([30, 0, 0], [0, 250, 0], base)
    r["magnus_az"] = a[2] + G                       # マグヌスの Z 成分（重力を除く）
    r["drag_ax"] = a[0]
    r["kd"] = base.kd
    r["terminal"] = math.sqrt(G / base.kd)
    for e in (0.73, 0.75, 0.76, 0.77, 0.78):
        r[f"drop_v_{e}"], r[f"drop_h_{e}"] = drop_test(e, base)
    for name, e, mu, et in SURFACES:
        m, ee, c, slid = cpr_test(e, mu, et)
        r[f"cpr_{name}"] = (round(m, 3), round(c, 1), category(c), "slide" if slid else "grip")
    for name, v0, rpm, top, org, tx in BENCH:
        el, info, w = solve_elev(v0, rpm, org, tx, HARD_E, HARD_MU, base, top)
        b0 = info["bounces"][0]
        t_ret, h_ret = time_to_x(v0, el, w, org, 12.885, HARD_E, HARD_MU, base)
        r[f"bench_{name}"] = dict(elev=el, net=info["netZ"], apex=info["apex"], t1=b0["t"],
                                  vin=norm(b0["vin"]), vout=norm(b0["vout"]), slid=b0["slid"],
                                  t_ret=t_ret, h_ret=h_ret)
    for cd in (0.50, 0.55, 0.60):                   # C_d の感度（1 m 後ろ → 2.5 m 内側）
        el, info, _ = solve_elev(34.7, 2300, [-13.0, 0, 0.95], 9.4, HARD_E, HARD_MU, Air(cd=cd))
        r[f"cd_{cd}"] = (info["netZ"], info["bounces"][0]["t"])
    el, info, w = solve_elev(34.7, 2300, [-13.0, 0, 0.95], 9.4, HARD_E, HARD_MU, base)
    th = math.radians(el)
    for rho in (1.21, 1.058, 0.957):                # 標高（同じ打ち出し）
        inf = flight([-13.0, 0, 0.95], [34.7 * math.cos(th), 0, 34.7 * math.sin(th)], w[:],
                     HARD_E, HARD_MU, Air(rho=rho))
        r[f"rho_{rho}"] = (inf["bounces"][0]["x"] - 9.4, inf["bounces"][0]["t"])
    return r


def report(r):
    print(f"Magnus @30 m/s, 250 rad/s: {-r['magnus_az']:.2f} m/s^2 (down), drag {r['drag_ax']:.2f} m/s^2")
    print(f"k_d = {r['kd']:.5f} /m, terminal = {r['terminal']:.2f} m/s")
    for e in (0.73, 0.75, 0.76, 0.77, 0.78):
        print(f"drop e={e}: impact {r[f'drop_v_{e}']:.2f} m/s, rebound {r[f'drop_h_{e}']:.1f} cm")
    for name, *_ in SURFACES:
        m, c, cat, mode = r[f"cpr_{name}"]
        print(f"CPR {name:22s}: mu_app={m:.3f} CPR={c:5.1f} Cat{cat} {mode}")
    for name, *_ in BENCH:
        b = r[f"bench_{name}"]
        print(f"{name:10s}: elev {b['elev']:6.2f} deg, net {b['net']:.2f} m, apex {b['apex']:.2f} m, "
              f"bounce {b['t1']:.3f} s, v {b['vin']*3.6:.0f}->{b['vout']*3.6:.0f} km/h "
              f"({'slide' if b['slid'] else 'grip'}), returner {b['t_ret']:.3f} s @ {b['h_ret']:.2f} m")
    for cd in (0.50, 0.55, 0.60):
        print(f"Cd={cd}: net {r[f'cd_{cd}'][0]:.2f} m, flight {r[f'cd_{cd}'][1]:.3f} s")
    for rho in (1.21, 1.058, 0.957):
        print(f"rho={rho}: landing {r[f'rho_{rho}'][0]:+.2f} m, flight {r[f'rho_{rho}'][1]:.3f} s")


if __name__ == "__main__":
    report(compute())
