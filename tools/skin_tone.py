# 肌の色味を別のテクスチャから移す処理の参照実装（DLL と同じ整数演算で、出力はバイト単位で一致する）。
#
#   recolor(target_g1t, source_g1t) -> bytes
#
# target の絵柄を保ったまま、source の「なだらかな色の分布」を移す。顔・手のテクスチャは、
# 千歳・紗重・八重で配置（UV）が同じなので、場所ごとの色の比を掛ければ、肌の色だけが移る。
#   1. 両方の最上位ミップ（BC1）を RGB に展開する
#   2. 64 ピクセル角のます目ごとに色の合計を取り、[1,2,1] のぼかしを縦横に 2 回かける
#   3. ます目ごとに source / target の比（12 ビット固定小数、上限 4 倍）を作る
#   4. 比をピクセルへ双線形で広げて target に掛ける
#   5. ミップを 2x2 の平均で作り直し、すべて BC1 に圧縮して、target の g1t の画素部分へ書き戻す
# g1t のヘッダーとファイルの長さは変わらない。
import struct
import numpy as np

CELL = 64
PASSES = 2
BIAS = 4            # 暗い所で比が暴れないように、分子と分母に足す量（ピクセル値の単位）
RATIO_MAX = 4 << 12


def g1t_info(g1t):
    """(幅, 高さ, ミップ数, 画素データの位置)。BC1 のテクスチャ 1 枚だけの g1t に限る"""
    assert g1t[:4] == b'GT1G'
    table = struct.unpack_from('<I', g1t, 0xC)[0]
    assert struct.unpack_from('<I', g1t, 0x10)[0] == 1, 'one texture expected'
    th = table + struct.unpack_from('<I', g1t, table)[0]
    mips, fmt, dims = g1t[th] >> 4, g1t[th + 1], g1t[th + 2]
    assert fmt == 0x59, 'BC1 expected'
    w, h = 1 << (dims & 15), 1 << (dims >> 4)
    size = sum(max(1, (w >> m) // 4) * max(1, (h >> m) // 4) * 8 for m in range(mips))
    data = len(g1t) - size
    assert data >= th + 8 and w % CELL == 0 and h % CELL == 0 and (w >> (mips - 1)) >= 4 and (h >> (mips - 1)) >= 4
    return w, h, mips, data


def expand565(c):
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1)


def bc1_decode(data, w, h):
    n = (w // 4) * (h // 4)
    blk = np.frombuffer(data, dtype='<u2', count=n * 4).reshape(n, 4).astype(np.int64)
    c0, c1 = blk[:, 0], blk[:, 1]
    bits = blk[:, 2] | (blk[:, 3] << 16)
    e0, e1 = expand565(c0), expand565(c1)
    four = (c0 > c1)[:, None]
    p2 = np.where(four, (2 * e0 + e1 + 1) // 3, (e0 + e1) // 2)
    p3 = np.where(four, (e0 + 2 * e1 + 1) // 3, 0)
    pal = np.stack([e0, e1, p2, p3], 1)                       # n, 4, 3
    idx = (bits[:, None] >> (2 * np.arange(16))) & 3          # n, 16
    px = np.take_along_axis(pal, idx[:, :, None], 1)          # n, 16, 3
    return px.reshape(h // 4, w // 4, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(h, w, 3)


def bc1_encode(img):
    h, w, _ = img.shape
    blk = img.reshape(h // 4, 4, w // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3).astype(np.int64)
    mn, mx = blk.min(1), blk.max(1)
    inset = (mx - mn) >> 4
    mn, mx = mn + inset, mx - inset

    def pack(v):
        return (((v[:, 0] * 31 + 127) // 255) << 11) | (((v[:, 1] * 63 + 127) // 255) << 5) | ((v[:, 2] * 31 + 127) // 255)

    c0, c1 = pack(mx), pack(mn)
    e0, e1 = expand565(c0), expand565(c1)
    pal = np.stack([e0, e1, (2 * e0 + e1 + 1) // 3, (e0 + 2 * e1 + 1) // 3], 1)   # n, 4, 3
    d = ((blk[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)                    # n, 16, 4
    idx = d.argmin(-1)
    idx[c0 == c1] = 0
    bits = (idx << (2 * np.arange(16))).sum(1)
    out = np.stack([c0, c1, bits & 0xFFFF, bits >> 16], 1).astype('<u2')
    return out.tobytes()


def blur121(a, axis):
    lo = np.concatenate([np.take(a, [0], axis), np.take(a, range(a.shape[axis] - 1), axis)], axis)
    hi = np.concatenate([np.take(a, range(1, a.shape[axis]), axis), np.take(a, [-1], axis)], axis)
    return lo + 2 * a + hi


def low(img):
    h, w, _ = img.shape
    s = img.astype(np.int64).reshape(h // CELL, CELL, w // CELL, CELL, 3).sum((1, 3))
    for _ in range(PASSES):
        s = blur121(blur121(s, 1), 0)
    return s


def transfer(target, source):
    """target（h, w, 3）に source の色味を移した画像"""
    h, w, _ = target.shape
    k = BIAS * CELL * CELL * 16 ** PASSES
    ratio = np.minimum(RATIO_MAX, ((low(source) + k) << 12) // (low(target) + k))   # gh, gw, 3

    def axis(n, g):
        t = 2 * np.arange(n) + 1 - CELL
        g0 = np.where(t < 0, 0, t // (2 * CELL))
        wt = np.where(t < 0, 0, t % (2 * CELL))
        return g0, np.minimum(g0 + 1, g - 1), wt

    y0, y1, wy = axis(h, h // CELL)
    x0, x1, wx = axis(w, w // CELL)
    full = 2 * CELL
    wy, wx = wy[:, None, None], wx[None, :, None]
    r = (ratio[y0][:, x0] * (full - wx) * (full - wy) + ratio[y0][:, x1] * wx * (full - wy)
         + ratio[y1][:, x0] * (full - wx) * wy + ratio[y1][:, x1] * wx * wy) >> 14
    return np.minimum(255, (target.astype(np.int64) * r + 2048) >> 12)


def recolor(target_g1t, source_g1t):
    w, h, mips, data = g1t_info(target_g1t)
    assert g1t_info(source_g1t)[:3] == (w, h, mips), 'the two textures differ in size'
    top = (w // 4) * (h // 4) * 8
    sdata = g1t_info(source_g1t)[3]
    img = transfer(bc1_decode(target_g1t[data:data + top], w, h),
                   bc1_decode(source_g1t[sdata:sdata + top], w, h))
    out = bytearray(target_g1t[:data])
    for m in range(mips):
        out += bc1_encode(img)
        hh, ww, _ = img.shape
        img = (img.reshape(hh // 2, 2, ww // 2, 2, 3).sum((1, 3)) + 2) >> 2
    assert len(out) == len(target_g1t)
    return bytes(out)
