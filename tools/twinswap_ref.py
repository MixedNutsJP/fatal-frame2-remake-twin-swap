# 姉妹スワップの参照実装。DLL と同じ手順で root.rdb / root.rdx / fdata を作る。
# DLL の出力とバイト単位で突き合わせるのと、DLL を作る前のゲーム内確認に使う。
#
#   python twinswap_ref.py build <出力フォルダ> <main: mio|mayu|sae|yae> <sub: 同左>
#       fdata_package の現在の root.rdb / root.rdx（= 他の Mod を含む）を元に、出力フォルダへ 3 ファイルを作る
#   python twinswap_ref.py install <main> <sub>
#       検証用。バックアップ（スカート丈 Mod だけの状態）を元に作り、ゲームのフォルダへ直接書き込む
#   python twinswap_ref.py restore
#       バックアップに戻す
import sys, os, struct, zlib, shutil

GAME = r'D:\SteamLibrary\steamapps\common\FatalFrameII\fdata_package'
BACKUP = r'D:\develop\FATAL FRAME2_mods\analysis_datas\twin_swap_poc\backup_before_poc'
FDATA_HASH = 0xfffe7510

DBS = (0x2082ad97, 0x97485e9b)   # キャラの枠を定義する DB。枠 → モデル定義の参照を書き換える

# 衣装メニューの順に 1 対 1。各衣装は (高精細, 軽量) の 2 つのモデル定義を持つ。
# 澪の 8 着目（SILENT HILL f のネイビーセーラー）は相手がいないので入れ替えない
MIO_DEFS = [(0x511668a8, 0x36546a72), (0xd946a887, 0xbe84aa51), (0x6176e866, 0x46b4ea30),
            (0xe9a72845, 0xcee52a0f), (0x71d76824, 0x571569ee), (0xfa07a803, 0xdf45a9cd),
            (0x8237e7e2, 0x6775e9ac)]
MAYU_DEFS = [(0x49118300, 0x0b93ba76), (0xc6e93f01, 0x896b7677), (0x44c0fb02, 0x07433278),
             (0xc298b703, 0x851aee79), (0x40707304, 0x02f2aa7a), (0xbe482f05, 0x80ca667b),
             (0x3c1feb06, 0xfea2227c)]

# 澪のモデル（g1m, grp）。同行キャラ（繭）に付けると顔が描画されないので直す
MIO_MODELS = [(0xcade596e, 0xd1cdeaec), (0xb50f91e4, 0xbbff2362), (0xd7774eef, 0xde66e06d),
              (0xc1a88765, 0xc89818e3), (0xe4104470, 0xeaffd5ee), (0xce417ce6, 0xd5310e64),
              (0xf0a939f1, 0xf798cb6f), (0xdada7267, 0xe1ca03e5), (0xfd422f72, 0x0431c0f0),
              (0xe77367e8, 0xee62f966), (0x09db24f3, 0x10cab671), (0xf40c5d69, 0xfafbeee7),
              (0x16741a74, 0x1d63abf2), (0x00a552ea, 0x0794e468)]
FACE_GROUP = 0x5526A88F


# ---- rdb / rdx / fdata -------------------------------------------------------

def rdb_entries(rdb):
    """{ハッシュ: (エントリ先頭, エントリ長)}"""
    out, o = {}, 0x20
    while o < len(rdb):
        assert rdb[o:o + 4] == b'IDRK', hex(o)
        size = struct.unpack_from('<Q', rdb, o + 8)[0]
        out[struct.unpack_from('<I', rdb, o + 0x24)[0]] = (o, size)
        o += (size + 3) & ~3
    return out


def rdb_location(rdb, ent):
    """エントリの末尾から (fdata 番号, fdata 内の位置)"""
    o, size = ent
    ssz = struct.unpack_from('<Q', rdb, o + 0x10)[0]
    f = o + size - ssz
    if ssz == 13:
        off, _, idx = struct.unpack_from('<IIH', rdb, f + 2)
    elif ssz == 0x11:
        extra, off, _, idx = struct.unpack_from('<IIIH', rdb, f + 2)
        off += (extra & 0xFF) << 32
    else:
        raise ValueError('footer size %d' % ssz)
    return idx, off


def rdx_files(rdx):
    return {struct.unpack_from('<h', rdx, i)[0]: struct.unpack_from('<I', rdx, i + 4)[0]
            for i in range(0, len(rdx), 8)}


def read_entry(folder, rdb, rdx, h):
    """ファイル本体と、fdata 側のエントリの付属データ（種別, tkid, 付属バイト列）"""
    idx, off = rdb_location(rdb, rdb_entries(rdb)[h])
    with open(os.path.join(folder, '0x%08x.fdata' % rdx_files(rdx)[idx]), 'rb') as f:
        f.seek(off)
        head = f.read(0x30)
        assert head[:8] == b'IDRK0000'
        esize, csize, usize = struct.unpack_from('<3Q', head, 8)
        etype, fh, tkid, flags = struct.unpack_from('<4I', head, 0x20)
        assert fh == h
        extra = f.read(esize - csize - 0x30)
        if csize == usize:
            return f.read(usize), (etype, tkid, extra)
        out = bytearray()
        while len(out) < usize:
            if flags & 0x100000:
                zsize = struct.unpack('<I', f.read(4))[0]
            else:
                zsize = struct.unpack('<HQ', f.read(10))[0]
            out += zlib.decompress(f.read(zsize))
        assert len(out) == usize
        return bytes(out), (etype, tkid, extra)


# ---- 改変 ------------------------------------------------------------------

def find_unique(data, v):
    b = struct.pack('<I', v)
    assert data.count(b) == 1, 'ref 0x%08x found %d times' % (v, data.count(b))
    return data.find(b)


# 紗重・八重は 1 着だけ（高精細・軽量の区別も無い）。どの衣装でもこの定義を指す
SAE_DEF = 0x47095b30   # g1m 0x9649abe6。腰に縄を巻くだけの方（2.1.0 では八重と取り違えていた）
YAE_DEF = 0xaa5cc277   # g1m 0xe92e0aff。縄が長く垂れている方
# 立花千歳。顔はグループ 0 にあり、グループの修正は不要。腰の骨は 74.16、双子は 87.25。
# 双子のモーションでは体が 13.09 持ち上がって足が浮くので、骨格を書き換えて合わせる（fit_chitose）
CHITOSE_DROP = struct.unpack('<f', struct.pack('<f', struct.unpack('<f', struct.pack('<f', 87.25))[0]
                                                 - struct.unpack('<f', struct.pack('<f', 74.16))[0]))[0]
# 書き換えた千歳のモデルは、見た目として使われていない方の双子の初期衣装（高精細・軽量）に置く。
# 並びは (モデル定義, g1m, grp, mtl, oid, ktid)
CHITOSE_FILES = [(0x8d206a12, 0x91c71644, 0x98b6a7c2, 0x92022e82, 0x83c14fbd, 0xf4a640b7),
                 (0xbc9476dc, 0x171b23ba, 0x1e0ab538, 0x17563bf8, 0x09155d33, 0x19d3e201)]
MAYU_FILES = [(0x49118300, 0x7dcc4616, 0x84bbd794, 0x7e075e54, 0x6fc67f8f, 0x89470b25),
              (0x0b93ba76, 0xd9c21c60, 0xe0b1adde, 0xd9fd349e, 0xcbbc55d9, 0xac0bfe1b)]
MIO_FILES = [(0x511668a8, 0xcade596e, 0xd1cdeaec, 0xcb1971ac, 0xbcd892e7, 0xde7762cd),
             (0x36546a72, 0xb50f91e4, 0xbbff2362, 0xb54aaa22, 0xa709cb5d, 0x3a6d3917)]


def chitose_home(main, sub):
    return MIO_FILES if 'mayu' in (main, sub) else MAYU_FILES
def fit_chitose(g1m, helper=68, spare=75, helper_id=107, spare_id=114):
    """千歳のモデルの骨格（G1MS）を、双子のモーションで足が浮かないように書き換える。
    腰（1 番）の初期位置を上げ、体は 2 番の子（X が上向き）を下げる。腰の直下の補助の骨は、
    68 番を下げ役にする（75 番に 68 番の値を写して ID の表で入れ替え、69 番以降の腰の子を 68 番の子に）"""
    g1m = bytearray(g1m)
    o = struct.unpack_from('<I', g1m, 0xC)[0]
    while g1m[o:o + 4][::-1] != b'G1MS':
        o += struct.unpack_from('<I', g1m, o + 8)[0]
    joints = o + struct.unpack_from('<I', g1m, o + 12)[0]
    jc = struct.unpack_from('<H', g1m, o + 20)[0]
    table = o + 28

    def parent(j):
        return struct.unpack_from('<i', g1m, joints + 48 * j + 12)[0]

    assert parent(2) == 1 and parent(helper) == 1 and parent(spare) == 1
    assert struct.unpack_from('<H', g1m, table + 2 * helper_id)[0] == helper
    assert struct.unpack_from('<H', g1m, table + 2 * spare_id)[0] == spare
    assert all(parent(j) != 1 for j in range(3, helper))
    g1m[joints + 48 * spare:joints + 48 * spare + 48] = g1m[joints + 48 * helper:joints + 48 * helper + 48]
    struct.pack_into('<H', g1m, table + 2 * helper_id, spare)
    struct.pack_into('<H', g1m, table + 2 * spare_id, helper)
    y = struct.unpack_from('<f', g1m, joints + 48 + 36)[0]
    struct.pack_into('<f', g1m, joints + 48 + 36, y + CHITOSE_DROP)
    struct.pack_into('<3f', g1m, joints + 48 * helper + 32, 0, -CHITOSE_DROP, 0)
    for j in range(3, jc):
        if parent(j) == 2:
            x = struct.unpack_from('<f', g1m, joints + 48 * j + 32)[0]
            struct.pack_into('<f', g1m, joints + 48 * j + 32, x - CHITOSE_DROP)
        elif parent(j) == 1 and j > helper:
            struct.pack_into('<i', g1m, joints + 48 * j + 12, helper)
    return bytes(g1m)


def def_for(look, i, k, home):
    return {'mio': MIO_DEFS[i][k], 'mayu': MAYU_DEFS[i][k], 'sae': SAE_DEF, 'yae': YAE_DEF,
            'chitose': home[k][0]}[look]


def assign_refs(db, main, sub):
    """澪の枠は main の見た目、繭の枠は sub の見た目のモデル定義を指すようにする"""
    db = bytearray(db)
    home = chitose_home(main, sub)
    pos = {v: find_unique(db, v) for pair in MIO_DEFS + MAYU_DEFS for v in pair}
    for i, (mio, mayu) in enumerate(zip(MIO_DEFS, MAYU_DEFS)):
        for k in range(2):
            struct.pack_into('<I', db, pos[mio[k]], def_for(main, i, k, home))
            struct.pack_into('<I', db, pos[mayu[k]], def_for(sub, i, k, home))
    return bytes(db)


def g1mg_section(data, typ):
    o = struct.unpack_from('<I', data, 0xC)[0]
    while data[o:o + 4][::-1] != b'G1MG':
        o += struct.unpack_from('<I', data, o + 8)[0]
    p = o + 0x30
    for _ in range(struct.unpack_from('<I', data, o + 0x2C)[0]):
        t, ssz = struct.unpack_from('<2I', data, p)
        if t == typ:
            return p
        p += ssz
    raise ValueError('no section %x' % typ)


def merge_groups(g1m, grp, names):
    """指定した名前のグループの LOD エントリを、常時表示のグループ 0 の直後へ移す。
    grp ではグループ 0 をその分広げ、移したグループは名前を残したまま空（0 項目・0 エントリ）にする。
    どちらもサイズは変わらない。"""
    rows = [list(struct.unpack_from('<8I', grp, i)) for i in range(0, len(grp), 32)]
    assert len(grp) % 32 == 0
    move = [i for i, r in enumerate(rows) if r[0] in names]
    assert move and 0 not in move, 'groups not found'
    for i in move:
        assert rows[i][3] == rows[i][4] == rows[i][6] == rows[i][7] == 0, 'unexpected grp fields'
    g1m = bytearray(g1m)
    p = g1mg_section(g1m, 0x10009)
    q, ents = p + 0x30, []
    while g1m[q:q + 1] == b'@':
        n = struct.unpack_from('<I', g1m, q + 24)[0]
        ents.append((q, 28 + 4 * n))
        q += 28 + 4 * n
    first, pos = [], 0
    for r in rows:
        first.append(pos)
        pos += r[5]
    total = pos
    assert len(ents) >= total, 'LOD entries %d < grp %d' % (len(ents), total)
    span = lambda i: list(range(first[i], first[i] + rows[i][5]))
    moved = [e for i in move for e in span(i)]
    order = span(0) + moved + [e for i in range(1, len(rows)) if i not in move for e in span(i)] + list(range(total, len(ents)))
    raw = [bytes(g1m[a:a + n]) for a, n in ents]
    start = ents[0][0]
    blob = b''.join(raw[i] for i in order)
    g1m[start:start + len(blob)] = blob
    for i in move:
        rows[0][2] += rows[i][2]
        rows[0][5] += rows[i][5]
        rows[i][2] = rows[i][5] = 0
    return bytes(g1m), b''.join(struct.pack('<8I', *r) for r in rows)


def hide_groups(g1m, grp, names):
    """指定した名前のグループに属する部品（サブメッシュ）のインデックス数を 0 にして、
    どの表示設定でも描画されないようにする。g1m のサイズは変わらない。grp は変えない。"""
    rows = [struct.unpack_from('<8I', grp, i) for i in range(0, len(grp), 32)]
    g1m = bytearray(g1m)
    p = g1mg_section(g1m, 0x10009)
    q, ents = p + 0x30, []
    while g1m[q:q + 1] == b'@':
        n = struct.unpack_from('<I', g1m, q + 24)[0]
        ents.append(struct.unpack_from('<%dI' % n, g1m, q + 28))
        q += 28 + 4 * n
    hide, keep, pos = set(), set(), 0
    for r in rows:
        for e in ents[pos:pos + r[5]]:
            (hide if r[0] in names else keep).update(e)
        pos += r[5]
    for e in ents[pos:]:
        keep.update(e)
    assert hide and not (hide & keep), 'groups not found or parts shared'
    sub = g1mg_section(g1m, 0x10008)
    for s in hide:
        struct.pack_into('<I', g1m, sub + 12 + 56 * s + 52, 0)   # +52 = インデックス数
    return bytes(g1m)


def face_fix(g1m, grp):
    """澪のモデルの顔（グループ 5526a88f）を常時表示にする"""
    return merge_groups(g1m, grp, (FACE_GROUP,))


# 澪の夏のカーディガン（2 着目）の高精細モデルの目隠し
BLINDFOLD_G1M = 0xd7774eef
BLINDFOLD_GROUP = 0x7EB9F3BA


# 紗重・八重のモデル（g1m, grp）。縄（部品 @1EED9A49）がグループ 768a168d と 6ad387ac にあり、
# 双子のキャラはこの 2 つを表示しないので、常時表示にする
SAE_YAE_MODELS = {'sae': (0x9649abe6, 0x9d393d64), 'yae': (0xe92e0aff, 0xf01d9c7d)}
ROPE_GROUPS = (0x768A168D, 0x6AD387AC)


# ---- 組み立て ----------------------------------------------------------------

def build(folder, rdb, rdx, main, sub, rope=True, blindfold='default'):
    """(新しい rdb, 新しい rdx, fdata) を返す"""
    files = []
    for h in DBS:
        data, meta = read_entry(folder, rdb, rdx, h)
        files.append((h, assign_refs(data, main, sub), meta))
    for g1m_h, grp_h in MIO_MODELS:
        names = []
        if sub == 'mio':
            names.append(FACE_GROUP)
        if blindfold == 'show' and g1m_h == BLINDFOLD_G1M:
            names.append(BLINDFOLD_GROUP)
        hide = blindfold == 'hide' and g1m_h == BLINDFOLD_G1M
        if not names and not hide:
            continue
        g1m, g1m_meta = read_entry(folder, rdb, rdx, g1m_h)
        grp, grp_meta = read_entry(folder, rdb, rdx, grp_h)
        if names:
            g1m, grp = merge_groups(g1m, grp, names)
        if hide:   # 目隠しの部品を描画されないようにする（顔を移した後の grp で範囲を引く）
            g1m = hide_groups(g1m, grp, (BLINDFOLD_GROUP,))
        files.append((g1m_h, g1m, g1m_meta))
        if names:
            files.append((grp_h, grp, grp_meta))
    if 'chitose' in (main, sub):
        # 骨格を双子のモーションに合わせたモデルを、使っていない双子の初期衣装に置く。
        # 中身は千歳、エントリの付属データは置き場所のものを使う
        for src, dst in zip(CHITOSE_FILES, chitose_home(main, sub)):
            for n in range(1, 6):
                data = read_entry(folder, rdb, rdx, src[n])[0]
                if n == 1:
                    data = fit_chitose(data)
                files.append((dst[n], data, read_entry(folder, rdb, rdx, dst[n])[1]))
    for look in ('sae', 'yae'):
        if rope and look in (main, sub):
            g1m_h, grp_h = SAE_YAE_MODELS[look]
            g1m, g1m_meta = read_entry(folder, rdb, rdx, g1m_h)
            grp, grp_meta = read_entry(folder, rdb, rdx, grp_h)
            g1m, grp = merge_groups(g1m, grp, ROPE_GROUPS)
            files += [(g1m_h, g1m, g1m_meta), (grp_h, grp, grp_meta)]

    marker = max(rdx_files(rdx)) + 1
    assert FDATA_HASH not in rdx_files(rdx).values()
    new_rdx = rdx + struct.pack('<2hI', marker, -1, FDATA_HASH)

    fdata = bytearray(b'PDRK0000' + struct.pack('<2I', 0x10, 0))
    where = {}
    for h, data, (etype, tkid, extra) in files:
        off = len(fdata)
        esize = 0x30 + len(extra) + len(data)
        fdata += b'IDRK0000' + struct.pack('<3Q', esize, len(data), len(data))
        fdata += struct.pack('<4I', etype, h, tkid, 0) + extra + data
        fdata += b'\0' * (-len(fdata) % 0x10)
        where[h] = (off, esize, len(data))
    struct.pack_into('<I', fdata, 0xC, len(fdata))

    new_rdb = bytearray(rdb)
    ents = rdb_entries(rdb)
    for h, (off, esize, usize) in where.items():
        o, size = ents[h]
        ssz = struct.unpack_from('<Q', new_rdb, o + 0x10)[0]
        struct.pack_into('<Q', new_rdb, o + 0x18, usize)
        struct.pack_into('<I', new_rdb, o + 0x2C, 0x20000)   # 非圧縮
        f = o + size - ssz
        if ssz == 13:
            struct.pack_into('<IIH', new_rdb, f + 2, off, esize, marker)
        else:
            struct.pack_into('<IIIH', new_rdb, f + 2, 0, off, esize, marker)
    return bytes(new_rdb), new_rdx, bytes(fdata)


def main():
    cmd = sys.argv[1]
    if cmd == 'build':
        out, m, s = sys.argv[2:5]
        rdb = open(os.path.join(GAME, 'root.rdb'), 'rb').read()
        rdx = open(os.path.join(GAME, 'root.rdx'), 'rb').read()
        r, x, f = build(GAME, rdb, rdx, m, s)
        os.makedirs(out, exist_ok=True)
        for name, b in (('root.rdb', r), ('root.rdx', x), ('0x%08x.fdata' % FDATA_HASH, f)):
            open(os.path.join(out, name), 'wb').write(b)
        print('built', out, len(f), 'bytes of fdata')
    elif cmd == 'install':
        m, s = sys.argv[2:4]
        rdb = open(os.path.join(BACKUP, 'root.rdb'), 'rb').read()
        rdx = open(os.path.join(BACKUP, 'root.rdx'), 'rb').read()
        r, x, f = build(GAME, rdb, rdx, m, s)
        for old in ('0xffff0010.fdata', '0xffff0010.yumiamod.json'):
            if os.path.exists(os.path.join(GAME, old)):
                os.remove(os.path.join(GAME, old))
        for name, b in (('0x%08x.fdata' % FDATA_HASH, f), ('root.rdx', x), ('root.rdb', r)):
            open(os.path.join(GAME, name), 'wb').write(b)
        print('installed main=%s sub=%s' % (m, s))
    elif cmd == 'restore':
        for name in ('root.rdb', 'root.rdx'):
            shutil.copy2(os.path.join(BACKUP, name), os.path.join(GAME, name))
        for old in ('0x%08x.fdata' % FDATA_HASH, '0xffff0010.fdata', '0xffff0010.yumiamod.json'):
            if os.path.exists(os.path.join(GAME, old)):
                os.remove(os.path.join(GAME, old))
        print('restored')


if __name__ == '__main__':
    main()
