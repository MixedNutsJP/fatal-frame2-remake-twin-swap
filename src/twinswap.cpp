// FATAL FRAME II: Crimson Butterfly REMAKE — TwinSwap
// Created by MixedNuts - https://github.com/MixedNutsJP/fatal-frame2-remake-twin-swap
// Licensed under the MIT License. See LICENSE for details.
//
// 操作キャラ（澪）と同行キャラ（繭）の見た目を、澪・繭・紗重・八重から ini で選ぶ。
// MixedNuts Mod Loader のプラグインとして MixedNuts\Mods\twinswap\ に置く
// （2.0.0 から。1.x は xinput1_4.dll のプロキシで単独で動いていた）。
//
// 仕組み:
//   キャラの枠（衣装ごとに高精細・軽量の 2 つ）は、2 つの kidsobjdb（0x2082ad97 /
//   0x97485e9b）の中でモデル定義を参照している。この参照を書き換えると、モデルと
//   それにぶら下がる専用設定（材質・骨・揺れもの）が丸ごと一緒に移る。
//
//   澪のモデルを同行キャラ（繭）に付けると、顔・歯・目まわりが描画されない。描画は
//   grp のグループ単位で表示が決まり、澪のモデルは顔を専用グループ 5526a88f に置いて
//   いるが、繭のキャラはこのグループを表示しないため。繭のモデルと同じく、顔の LOD
//   エントリを常時表示のグループ 0 の範囲へ移し（g1m）、グループ 0 を広げて顔の
//   グループを空にする（grp）。どちらもサイズは変わらない。
//
//   改変したファイルは Mod 専用の fdata にまとめ、root.rdb / root.rdx をそれを指す
//   ように書き換える。この 3 つはローダーのファイル改変として返し、キャッシュと
//   差し替えはローダーが行う。元にするのは現在の root.rdb / root.rdx（Yumia ツールで
//   入れた Mod や、先に読み込まれた Mod の改変を含む）なので、それらはそのまま残る。
//   ゲームのファイルは一切変更しない。
//
// ログは英語で書く（利用者が自分で状況を判断できるように）。コメントは日本語。

#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

#include <mixednuts/plugin.h>
#include <mixednuts/bytes.hpp>
#include <mixednuts/file.hpp>
#include <mixednuts/ini.hpp>
#include <mixednuts/log.hpp>

#include "inflate.hpp"

namespace {

using mixednuts::Log;
using mixednuts::Rd;
using mixednuts::Utf8;
using mixednuts::Wr;
using mixednuts::file::ReadAt;

constexpr char     kVersion[]  = "2.4.0";
constexpr char     kCacheTag[] = "twinswap-v17";   // 生成ロジックを変えたら上げる
constexpr uint32_t kFdataHash  = 0xFFFE7510;

const wchar_t kRdb[] = L"fdata_package\\root.rdb";
const wchar_t kRdx[] = L"fdata_package\\root.rdx";

// 見た目の選択肢。ini の値もこの名前
enum Look { kMio, kMayu, kSae, kYae, kChitose };
const wchar_t* const kLookNames[] = {L"mio", L"mayu", L"sae", L"yae", L"chitose"};
const char* const    kLookNamesA[] = {"mio", "mayu", "sae", "yae", "chitose"};

bool g_enabled = true;
Look g_main = kMayu;   // 操作キャラ（本編の澪）の見た目
Look g_sub  = kMio;    // 同行キャラ（本編の繭）の見た目
bool g_rope = true;    // 紗重・八重の赤い縄を表示するか

// 澪の夏のカーディガン（2 着目）の目隠し
enum Blindfold { kBfDefault, kBfShow, kBfHide };
const char* const kBlindfoldNames[] = {"default", "show", "hide"};
Blindfold g_blindfold = kBfDefault;

// ---- ファイル入出力 -----------------------------------------------------
//
// 生成はローダーから呼ばれ、ファイルはローダーの入出力を通して読み書きする。
// fdata は大きいので丸ごとは読まず、現在の実体のパスを聞いて部分的に読む
// （先に読み込まれた Mod が足した fdata も同じように読める）。

const MixedNutsApi*     g_api = nullptr;
const MixedNutsPatchIo* g_io  = nullptr;   // 生成中だけ有効
std::wstring            g_modDir;

HANDLE OpenRead(const std::wstring& path)
{
    return mixednuts::file::OpenRead(path, g_api->CreateFileOriginal);
}

// ---- 対応表 -------------------------------------------------------------

const uint32_t kDbs[] = {0x2082AD97, 0x97485E9B};   // キャラの枠を定義する DB

// 衣装メニューの順に 1 対 1。{高精細, 軽量} のモデル定義。
// 澪の 8 着目（SILENT HILL f のネイビーセーラー）は相手がいないので入れ替えない
const uint32_t kMioDefs[7][2] = {
    {0x511668A8, 0x36546A72}, {0xD946A887, 0xBE84AA51}, {0x6176E866, 0x46B4EA30},
    {0xE9A72845, 0xCEE52A0F}, {0x71D76824, 0x571569EE}, {0xFA07A803, 0xDF45A9CD},
    {0x8237E7E2, 0x6775E9AC}};
const uint32_t kMayuDefs[7][2] = {
    {0x49118300, 0x0B93BA76}, {0xC6E93F01, 0x896B7677}, {0x44C0FB02, 0x07433278},
    {0xC298B703, 0x851AEE79}, {0x40707304, 0x02F2AA7A}, {0xBE482F05, 0x80CA667B},
    {0x3C1FEB06, 0xFEA2227C}};

// 紗重・八重は白い着物（生前の姿）の 1 着だけで、高精細・軽量の区別も無い。どの衣装を
// 選んでもこの定義を指す。腰に縄を巻くだけの方が紗重、縄が長く垂れている方が八重
// （2.1.0 では逆にしていた。ゲーム内で見比べた利用者の指摘で 2.2.0 で直した）。
// どちらも顔は常時表示のグループ 0 にあり、顔の修正は要らない
constexpr uint32_t kSaeDef = 0x47095B30;   // g1m 0x9649abe6
constexpr uint32_t kYaeDef = 0xAA5CC277;   // g1m 0xe92e0aff

// 立花千歳。顔は常時表示のグループ 0 にあり、ほかのグループ（7f621c22 / abdec7fe）は目まわりの
// 差分で、双子のキャラが表示する 7f621c22 で足りるので、グループの修正は要らない。
// 千歳は背が低く、腰の骨の高さが 74.16（双子は 87.25）。双子のモーションは腰を 87.25 に置くので、
// そのままでは体が 13.09 持ち上がって足が浮く。骨格を書き換えて合わせる（FitChitose）
constexpr float kChitoseDrop = 87.25f - 74.16f;

// 千歳の骨格は双子のモーションに合わせて書き換えるので、本来の千歳（別の枠から同じモデルを
// 使う）に影響しないよう、書き換えたモデルは別のファイルに置く。置き場所は、見た目として
// 使われていない方の双子の初期衣装（高精細・軽量）。双子の枠はすべてこの Mod が書き換えるので、
// そのモデル定義を指す枠は千歳の見た目のものだけになる。
// モデル定義が参照するファイルのうち、g1m / grp / mtl / oid / ktid の 5 つを千歳のもので上書きする
// （oid は g1m のエントリの付属データが、ktid はモデル定義が間接的に参照している）
struct ModelFiles { uint32_t def, g1m, grp, mtl, oid, ktid; };
const ModelFiles kChitoseFiles[2] = {
    {0x8D206A12, 0x91C71644, 0x98B6A7C2, 0x92022E82, 0x83C14FBD, 0xF4A640B7},
    {0xBC9476DC, 0x171B23BA, 0x1E0AB538, 0x17563BF8, 0x09155D33, 0x19D3E201}};
const ModelFiles kMayuFiles[2] = {
    {0x49118300, 0x7DCC4616, 0x84BBD794, 0x7E075E54, 0x6FC67F8F, 0x89470B25},
    {0x0B93BA76, 0xD9C21C60, 0xE0B1ADDE, 0xD9FD349E, 0xCBBC55D9, 0xAC0BFE1B}};
const ModelFiles kMioFiles[2] = {
    {0x511668A8, 0xCADE596E, 0xD1CDEAEC, 0xCB1971AC, 0xBCD892E7, 0xDE7762CD},
    {0x36546A72, 0xB50F91E4, 0xBBFF2362, 0xB54AAA22, 0xA709CB5D, 0x3A6D3917}};

const ModelFiles* ChitoseHome()
{
    return (g_main == kMayu || g_sub == kMayu) ? kMioFiles : kMayuFiles;
}

uint32_t DefFor(Look look, int costume, int detail)
{
    switch (look)
    {
    case kMio:  return kMioDefs[costume][detail];
    case kMayu: return kMayuDefs[costume][detail];
    case kSae:  return kSaeDef;
    case kYae:  return kYaeDef;
    default:    return ChitoseHome()[detail].def;
    }
}

// 上の澪のモデル定義が使う {g1m, grp}
const uint32_t kMioModels[14][2] = {
    {0xCADE596E, 0xD1CDEAEC}, {0xB50F91E4, 0xBBFF2362}, {0xD7774EEF, 0xDE66E06D},
    {0xC1A88765, 0xC89818E3}, {0xE4104470, 0xEAFFD5EE}, {0xCE417CE6, 0xD5310E64},
    {0xF0A939F1, 0xF798CB6F}, {0xDADA7267, 0xE1CA03E5}, {0xFD422F72, 0x0431C0F0},
    {0xE77367E8, 0xEE62F966}, {0x09DB24F3, 0x10CAB671}, {0xF40C5D69, 0xFAFBEEE7},
    {0x16741A74, 0x1D63ABF2}, {0x00A552EA, 0x0794E468}};

constexpr uint32_t kFaceGroup = 0x5526A88F;

// 澪の夏のカーディガン（2 着目）の高精細モデルには、白い目隠し（グループ 7eb9f3ba、部品
// @1EED9A49）が入っている。普段は表示されないが、姉妹を入れ替えていると、取り憑かれて
// 敵として現れる場面などで表示される。
//   show: 目隠しを常時表示のグループ 0 へ移す（顔や縄と同じ方法）
//   hide: 目隠しの部品のインデックス数を 0 にして、描画されないようにする。
//         表示プリセットの 4 番目（この衣装だけ目隠しのグループを含む）からグループ名を
//         外す方法も試したが、上の場面では消えなかった（別の経路で表示されている）
constexpr uint32_t kBlindfoldG1m   = 0xD7774EEF;
constexpr uint32_t kBlindfoldGroup = 0x7EB9F3BA;

// 紗重・八重のモデルの {g1m, grp}。縄（部品 @1EED9A49）がグループ 768a168d と 6ad387ac にあり、
// 双子のキャラはこの 2 つを表示しないので、顔と同じ方法で常時表示のグループ 0 へ移す。
// Rope=0 なら移さない（双子のキャラでは縄が表示されないまま）
const uint32_t kSaeModel[2] = {0x9649ABE6, 0x9D393D64};
const uint32_t kYaeModel[2] = {0xE92E0AFF, 0xF01D9C7D};
const uint32_t kRopeGroups[] = {0x768A168D, 0x6AD387AC};

// ---- rdb / rdx / fdata --------------------------------------------------

struct RdbEntry { size_t off, size; };

// rdb のエントリ: 0x30 バイトの見出し（+0x08 長さ, +0x10 末尾の長さ, +0x18 ファイルサイズ,
// +0x24 ハッシュ, +0x2C フラグ）、付属データ、末尾（fdata の番号と位置）
bool IndexRdb(const std::vector<uint8_t>& rdb, std::unordered_map<uint32_t, RdbEntry>& out)
{
    size_t o = 0x20;
    while (o + 0x30 <= rdb.size())
    {
        if (memcmp(&rdb[o], "IDRK", 4) != 0) return false;
        const uint64_t size = Rd<uint64_t>(&rdb[o + 8]);
        if (size < 0x30 || o + size > rdb.size()) return false;
        out[Rd<uint32_t>(&rdb[o + 0x24])] = {o, static_cast<size_t>(size)};
        o += static_cast<size_t>((size + 3) & ~3ull);
    }
    return o == rdb.size();
}

bool Location(const std::vector<uint8_t>& rdb, const RdbEntry& e, int16_t& idx, uint64_t& off)
{
    const uint64_t ssz = Rd<uint64_t>(&rdb[e.off + 0x10]);
    const uint8_t* f = &rdb[e.off + e.size - ssz];
    if (ssz == 13)
    {
        off = Rd<uint32_t>(f + 2);
        idx = Rd<int16_t>(f + 10);
    }
    else if (ssz == 0x11)
    {
        off = Rd<uint32_t>(f + 6) + (static_cast<uint64_t>(Rd<uint32_t>(f + 2) & 0xFF) << 32);
        idx = Rd<int16_t>(f + 14);
    }
    else return false;
    return true;
}

struct Source {
    std::vector<uint8_t> rdb, rdx;
    std::unordered_map<uint32_t, RdbEntry> ents;
    std::unordered_map<int16_t, uint32_t> fdatas;   // rdx: 番号 → fdata のハッシュ
};

struct File {
    uint32_t hash = 0;
    uint32_t type = 0, tkid = 0;
    std::vector<uint8_t> extra;    // fdata 側のエントリの付属データ（そのまま引き継ぐ）
    std::vector<uint8_t> data;
};

bool ReadEntry(const Source& src, uint32_t hash, File& out)
{
    auto it = src.ents.find(hash);
    if (it == src.ents.end()) { Log("[NG] 0x%08X is not in root.rdb", hash); return false; }
    int16_t idx = 0;
    uint64_t off = 0;
    if (!Location(src.rdb, it->second, idx, off)) { Log("[NG] Unknown rdb entry for 0x%08X", hash); return false; }
    auto fd = src.fdatas.find(idx);
    if (fd == src.fdatas.end()) { Log("[NG] fdata #%d of 0x%08X is not in root.rdx", idx, hash); return false; }

    wchar_t name[32];
    swprintf_s(name, L"0x%08x.fdata", fd->second);
    const std::wstring rel = std::wstring(L"fdata_package\\") + name;
    const wchar_t* path = g_io->Path(g_io->self, rel.c_str());
    HANDLE h = path ? OpenRead(path) : INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE) { Log("[NG] Cannot open %s", Utf8(name).c_str()); return false; }

    bool ok = false;
    uint8_t head[0x30];
    do
    {
        if (!ReadAt(h, off, head, sizeof(head)) || memcmp(head, "IDRK0000", 8) != 0) break;
        const uint64_t esize = Rd<uint64_t>(head + 8), csize = Rd<uint64_t>(head + 0x10),
                       usize = Rd<uint64_t>(head + 0x18);
        const uint32_t flags = Rd<uint32_t>(head + 0x2C);
        if (Rd<uint32_t>(head + 0x24) != hash || esize < 0x30 + csize || usize > (1u << 30) ||
            csize > (1u << 30))
            break;
        out.hash = hash;
        out.type = Rd<uint32_t>(head + 0x20);
        out.tkid = Rd<uint32_t>(head + 0x28);
        out.extra.resize(static_cast<size_t>(esize - csize - 0x30));
        if (!out.extra.empty() && !ReadAt(h, off + 0x30, out.extra.data(), out.extra.size())) break;
        const uint64_t body = off + 0x30 + out.extra.size();

        out.data.clear();
        if (csize == usize)
        {
            out.data.resize(static_cast<size_t>(usize));
            ok = ReadAt(h, body, out.data.data(), out.data.size());
            break;
        }
        // 圧縮: チャンクごとに {u16 長さ, u64 ?} か {u32 長さ}（flags & 0x100000）＋ zlib
        std::vector<uint8_t> z(static_cast<size_t>(csize));
        if (!ReadAt(h, body, z.data(), z.size())) break;
        out.data.reserve(static_cast<size_t>(usize));
        size_t p = 0;
        bool good = true;
        while (out.data.size() < usize)
        {
            size_t zsize = 0;
            if (flags & 0x100000)
            {
                if (p + 4 > z.size()) { good = false; break; }
                zsize = Rd<uint32_t>(&z[p]);
                p += 4;
            }
            else
            {
                if (p + 10 > z.size()) { good = false; break; }
                zsize = Rd<uint16_t>(&z[p]);
                p += 10;
            }
            if (p + zsize > z.size() ||
                !inflate::Zlib(&z[p], zsize, out.data, static_cast<size_t>(usize)))
            {
                good = false;
                break;
            }
            p += zsize;
        }
        ok = good && out.data.size() == usize;
    } while (false);
    CloseHandle(h);
    if (!ok) Log("[NG] Cannot read 0x%08X from %s", hash, Utf8(name).c_str());
    return ok;
}

// ---- 改変 ---------------------------------------------------------------

bool FindUnique(const std::vector<uint8_t>& d, uint32_t v, size_t& pos)
{
    int n = 0;
    for (size_t i = 0; i + 4 <= d.size(); ++i)
        if (Rd<uint32_t>(&d[i]) == v) { pos = i; ++n; }
    return n == 1;
}

// 澪の枠は Main の見た目、繭の枠は Sub の見た目のモデル定義を指すようにする
bool AssignRefs(std::vector<uint8_t>& db, uint32_t hash)
{
    size_t mioPos[7][2], mayuPos[7][2];
    for (int i = 0; i < 7; ++i)
        for (int k = 0; k < 2; ++k)
            if (!FindUnique(db, kMioDefs[i][k], mioPos[i][k]) ||
                !FindUnique(db, kMayuDefs[i][k], mayuPos[i][k]))
            {
                Log("[NG] DB 0x%08X: costume %d is not found exactly once", hash, i + 1);
                return false;
            }
    for (int i = 0; i < 7; ++i)
        for (int k = 0; k < 2; ++k)
        {
            Wr<uint32_t>(&db[mioPos[i][k]], DefFor(g_main, i, k));
            Wr<uint32_t>(&db[mayuPos[i][k]], DefFor(g_sub, i, k));
        }
    return true;
}

// g1m の G1MG チャンクの中から、指定の種類のセクションの先頭を探す
bool G1mgSection(const std::vector<uint8_t>& d, uint32_t type, size_t& out)
{
    if (d.size() < 0x10) return false;
    size_t o = Rd<uint32_t>(&d[0xC]);
    for (;;)
    {
        if (o + 0x30 > d.size()) return false;
        if (memcmp(&d[o], "GM1G", 4) == 0) break;   // "G1MG" が逆順で入っている
        const uint32_t len = Rd<uint32_t>(&d[o + 8]);
        if (len == 0) return false;
        o += len;
    }
    size_t p = o + 0x30;
    const uint32_t n = Rd<uint32_t>(&d[o + 0x2C]);
    for (uint32_t i = 0; i < n; ++i)
    {
        if (p + 12 > d.size()) return false;
        const uint32_t t = Rd<uint32_t>(&d[p]), len = Rd<uint32_t>(&d[p + 4]);
        if (t == type) { out = p; return true; }
        if (len == 0) return false;
        p += len;
    }
    return false;
}

// 指定した名前のグループの LOD エントリを、常時表示のグループ 0 の直後へ移す。
// grp はグループ 0 をその分広げ、移したグループは名前を残したまま空にする（名前から
// 番号を引く処理が、見つからない名前でどう振る舞うか分からないので、名前は消さない）。
// どちらもサイズは変わらない
bool MergeGroups(File& g1m, File& grp, const uint32_t* names, size_t count)
{
    auto& gd = grp.data;
    if (gd.size() < 64 || gd.size() % 32) return false;
    const size_t groups = gd.size() / 32;

    std::vector<size_t> first(groups), nent(groups);
    std::vector<bool> move(groups, false);
    size_t total = 0, moving = 0;
    for (size_t i = 0; i < groups; ++i)
    {
        const uint8_t* r = &gd[i * 32];
        first[i] = total;
        total += nent[i] = Rd<uint32_t>(r + 0x14);
        for (size_t k = 0; k < count; ++k)
            if (i > 0 && Rd<uint32_t>(r) == names[k]) move[i] = true;
        if (!move[i]) continue;
        // 差分（ID 付き）の数の欄が使われているグループは扱わない
        if (Rd<uint32_t>(r + 0x0C) || Rd<uint32_t>(r + 0x10) || Rd<uint32_t>(r + 0x18) ||
            Rd<uint32_t>(r + 0x1C))
        {
            Log("[NG] grp 0x%08X: group %zu has fields this mod does not handle", grp.hash, i);
            return false;
        }
        ++moving;
    }
    if (!moving) { Log("[NG] grp 0x%08X: the groups to move are not found", grp.hash); return false; }

    auto& d = g1m.data;
    size_t sec = 0;
    if (!G1mgSection(d, 0x10009, sec)) { Log("[NG] g1m 0x%08X: no LOD section", g1m.hash); return false; }
    std::vector<std::pair<size_t, size_t>> ents;   // {位置, 長さ}
    size_t q = sec + 0x30;
    while (q + 28 <= d.size() && d[q] == '@')
    {
        const size_t len = 28 + 4 * static_cast<size_t>(Rd<uint32_t>(&d[q + 24]));
        if (q + len > d.size()) return false;
        ents.push_back({q, len});
        q += len;
    }
    if (ents.size() < total)
    {
        Log("[NG] g1m 0x%08X: %zu LOD entries, grp expects %zu", g1m.hash, ents.size(), total);
        return false;
    }
    // 新しい並び: グループ 0、移すグループ（元の順）、残りのグループ、grp に属さない末尾
    std::vector<size_t> order;
    auto span = [&](size_t g) { for (size_t e = 0; e < nent[g]; ++e) order.push_back(first[g] + e); };
    span(0);
    for (size_t g = 1; g < groups; ++g) if (move[g]) span(g);
    for (size_t g = 1; g < groups; ++g) if (!move[g]) span(g);
    for (size_t i = total; i < ents.size(); ++i) order.push_back(i);

    std::vector<uint8_t> blob;
    for (size_t i : order) blob.insert(blob.end(), d.begin() + ents[i].first,
                                       d.begin() + ents[i].first + ents[i].second);
    memcpy(&d[ents[0].first], blob.data(), blob.size());   // エントリは連続しているので長さは同じ

    for (size_t g = 1; g < groups; ++g)
    {
        if (!move[g]) continue;
        uint8_t* r = &gd[g * 32];
        Wr<uint32_t>(&gd[0x08], Rd<uint32_t>(&gd[0x08]) + Rd<uint32_t>(r + 0x08));
        Wr<uint32_t>(&gd[0x14], Rd<uint32_t>(&gd[0x14]) + Rd<uint32_t>(r + 0x14));
        Wr<uint32_t>(r + 0x08, 0);
        Wr<uint32_t>(r + 0x14, 0);
    }
    return true;
}

// 読み出して直したモデルを files に足す。直せなければ足さずに警告だけ出す
bool AddMerged(const Source& src, const uint32_t model[2], const uint32_t* names, size_t count,
               std::vector<File>& files, bool& merged)
{
    File g1m, grp;
    merged = false;
    if (!ReadEntry(src, model[0], g1m) || !ReadEntry(src, model[1], grp)) return false;
    if (!MergeGroups(g1m, grp, names, count)) return true;
    files.push_back(std::move(g1m));
    files.push_back(std::move(grp));
    merged = true;
    return true;
}

// 指定した名前のグループに属する部品（サブメッシュ）のインデックス数を 0 にして、
// どの表示設定でも描画されないようにする。grp は読むだけで、g1m のサイズは変わらない。
// 部品がほかのグループからも使われている場合は、巻き添えを避けて何もしない
bool HideGroups(File& g1m, const File& grp, const uint32_t* names, size_t count)
{
    const auto& gd = grp.data;
    if (gd.size() < 32 || gd.size() % 32) return false;
    const size_t groups = gd.size() / 32;

    auto& d = g1m.data;
    size_t lod = 0, sub = 0;
    if (!G1mgSection(d, 0x10009, lod) || !G1mgSection(d, 0x10008, sub))
    {
        Log("[NG] g1m 0x%08X: no LOD or submesh section", g1m.hash);
        return false;
    }
    const uint32_t nsub = Rd<uint32_t>(&d[sub + 8]);
    if (sub + 12 + 56ull * nsub > d.size()) return false;

    // LOD エントリを順に読み、grp の範囲から、隠す部品とそれ以外の部品を分ける
    std::vector<bool> hide(nsub, false), keep(nsub, false);
    size_t q = lod + 0x30, group = 0, left = Rd<uint32_t>(&gd[0x14]);
    bool any = false;
    while (q + 28 <= d.size() && d[q] == '@')
    {
        const uint32_t n = Rd<uint32_t>(&d[q + 24]);
        if (q + 28 + 4ull * n > d.size()) return false;
        while (group < groups && left == 0)
            left = ++group < groups ? Rd<uint32_t>(&gd[group * 32 + 0x14]) : 0;
        bool target = false;
        if (group < groups)
        {
            for (size_t k = 0; k < count; ++k)
                if (Rd<uint32_t>(&gd[group * 32]) == names[k]) target = true;
            --left;
        }
        for (uint32_t i = 0; i < n; ++i)
        {
            const uint32_t s = Rd<uint32_t>(&d[q + 28 + 4 * i]);
            if (s >= nsub) return false;
            (target ? hide : keep)[s] = true;
        }
        any = any || target;
        q += 28 + 4ull * n;
    }
    if (!any) { Log("[NG] grp 0x%08X: the groups to hide are not found", grp.hash); return false; }
    for (uint32_t s = 0; s < nsub; ++s)
        if (hide[s] && keep[s])
        {
            Log("[NG] g1m 0x%08X: part %u is shared with other groups", g1m.hash, s);
            return false;
        }
    for (uint32_t s = 0; s < nsub; ++s)
        if (hide[s]) Wr<uint32_t>(&d[sub + 12 + 56 * s + 52], 0);   // +52 = インデックス数
    return true;
}

// 千歳のモデルの骨格（G1MS）を、双子のモーションで足が浮かないように書き換える。
// 骨 1 本は 48 バイト: 拡大 3f, 親 i32, 回転 4f, 位置 4f（親からの相対値）。+28 から骨 ID の表
// （ID → 骨の番号）。モーションは骨を ID で指す。
// 腰の骨（1 番、回転なし）の子は、体の親（2 番）と、補助の骨（68〜107 番）。モーションが位置を
// 与えるのは 1 番、2 番と、補助の骨の一部（68〜74 番 = ID 107〜113）で、ほかは回転だけ。
// 位置を与えられる骨は、初期値を下げても上書きされる。そこで:
// - 腰の初期位置を kChitoseDrop だけ上げる（モーションが置く高さになる。以下はその分を下で戻す）
// - 体: 2 番はそのままにして、2 番の子を下げる。2 番は回転 (0.5, 0.5, 0.5, 0.5) を持ち、
//   2 番から見た X が上向きにあたる
// - 補助の骨: 親を下げるしかないので、68 番を下げ役にする。75 番（ID 114。モーションが何も
//   与えず、頂点も付いていない）に 68 番の値を写して ID の表で 2 本を入れ替え、68 番を腰から
//   (0, -kChitoseDrop, 0) に置き、69 番以降の腰の子を 68 番の子にする
// こうすると、2 番と 68 番以外の骨の初期の位置（スキニングと布の基準）は変わらない。
// 体だけ下げると、手をつなぐときに袖が伸びて体が浮いた（手の目標の骨が取り残される）。
// 親の番号が子より大きくなる付け替えは、布（裾と垂れた髪）が消えた。骨を足すのは起動時に止まった
bool FitChitose(File& g1m)
{
    constexpr uint16_t kHelper = 68, kSpare = 75, kHelperId = 107, kSpareId = 114;
    auto& d = g1m.data;
    if (d.size() < 0x18) return false;
    size_t o = Rd<uint32_t>(&d[0xC]);
    for (;;)
    {
        if (o + 0x20 > d.size()) return false;
        if (memcmp(&d[o], "SM1G", 4) == 0) break;   // "G1MS" が逆順で入っている
        const uint32_t len = Rd<uint32_t>(&d[o + 8]);
        if (len == 0) return false;
        o += len;
    }
    const uint32_t jo = Rd<uint32_t>(&d[o + 12]);
    const uint16_t jc = Rd<uint16_t>(&d[o + 20]), ids = Rd<uint16_t>(&d[o + 22]);
    if (jc <= kSpare || ids <= kSpareId || 28ull + 2 * ids > jo || o + jo + 48ull * jc > d.size())
        return false;
    uint8_t* joints = &d[o + jo];
    uint8_t* table = &d[o + 28];
    auto parent = [&](uint16_t j) { return Rd<int32_t>(joints + 48 * j + 12); };
    if (parent(2) != 1 || parent(kHelper) != 1 || parent(kSpare) != 1 ||
        Rd<uint16_t>(table + 2 * kHelperId) != kHelper || Rd<uint16_t>(table + 2 * kSpareId) != kSpare)
        return false;
    for (int k = 0; k < 4; ++k)
        if (fabsf(Rd<float>(joints + 48 * 2 + 16 + 4 * k) - 0.5f) > 0.001f) return false;
    for (uint16_t j = 3; j < kHelper; ++j)
        if (parent(j) == 1) return false;   // 68 番より前に、腰の子は 2 番しかいないこと

    memcpy(joints + 48 * kSpare, joints + 48 * kHelper, 48);
    Wr<uint16_t>(table + 2 * kHelperId, kSpare);
    Wr<uint16_t>(table + 2 * kSpareId, kHelper);
    Wr<float>(joints + 48 + 36, Rd<float>(joints + 48 + 36) + kChitoseDrop);
    Wr<float>(joints + 48 * kHelper + 32, 0.0f);
    Wr<float>(joints + 48 * kHelper + 36, -kChitoseDrop);
    Wr<float>(joints + 48 * kHelper + 40, 0.0f);
    for (uint16_t j = 3; j < jc; ++j)
    {
        uint8_t* joint = joints + 48 * j;
        if (parent(j) == 2) Wr<float>(joint + 32, Rd<float>(joint + 32) - kChitoseDrop);
        else if (parent(j) == 1 && j > kHelper) Wr<int32_t>(joint + 12, kHelper);
    }
    return true;
}

// ---- 生成 ---------------------------------------------------------------

// 現在の内容を丸ごと読む（先に読み込まれた Mod の改変を含む）
bool ReadCurrent(const wchar_t* rel, std::vector<uint8_t>& out)
{
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (!g_io->Read(g_io->self, rel, &data, &size)) return false;
    out.assign(data, data + size);
    return true;
}

bool Generate()
{
    Source src;
    if (!ReadCurrent(kRdb, src.rdb) || !ReadCurrent(kRdx, src.rdx) || src.rdx.size() % 8)
    {
        Log("[NG] Cannot read root.rdb / root.rdx");
        return false;
    }
    if (!IndexRdb(src.rdb, src.ents)) { Log("[NG] root.rdb has an unknown layout"); return false; }
    int16_t marker = 0;
    for (size_t i = 0; i < src.rdx.size(); i += 8)
    {
        const int16_t m = Rd<int16_t>(&src.rdx[i]);
        const uint32_t h = Rd<uint32_t>(&src.rdx[i + 4]);
        if (h == kFdataHash) { Log("[NG] root.rdx already lists this mod's fdata"); return false; }
        src.fdatas[m] = h;
        if (m > marker) marker = m;
    }
    ++marker;

    std::vector<File> files;
    for (uint32_t h : kDbs)
    {
        File f;
        if (!ReadEntry(src, h, f) || !AssignRefs(f.data, h)) return false;
        files.push_back(std::move(f));
    }
    int fixed = 0;
    // 澪のモデル: 同行キャラに付けるなら顔を、Blindfold=show なら夏のカーディガンの目隠しを、
    // 常時表示のグループ 0 へ移す。Blindfold=hide なら目隠しの部品を描画されないようにする
    for (auto& m : kMioModels)
    {
        uint32_t names[2];
        size_t n = 0;
        const bool blindfoldModel = m[0] == kBlindfoldG1m;
        if (g_sub == kMio) names[n++] = kFaceGroup;
        if (g_blindfold == kBfShow && blindfoldModel) names[n++] = kBlindfoldGroup;
        const bool hide = g_blindfold == kBfHide && blindfoldModel;
        if (!n && !hide) continue;

        File g1m, grp;
        if (!ReadEntry(src, m[0], g1m) || !ReadEntry(src, m[1], grp)) return false;
        const bool merged = n && MergeGroups(g1m, grp, names, n);
        if (n && !merged)
            Log("[NG] Could not fix model 0x%08X; Mio's face (as the companion) or her"
                " blindfold may not be shown as set in that costume", m[0]);
        // 顔を移した後の grp で、目隠しのグループの範囲を引く
        const bool hidden = hide && HideGroups(g1m, grp, &kBlindfoldGroup, 1);
        if (hide && !hidden) Log("[NG] Could not hide the blindfold of model 0x%08X", m[0]);
        if (!merged && !hidden) continue;
        files.push_back(std::move(g1m));
        if (merged) files.push_back(std::move(grp));
        ++fixed;
    }
    // 千歳: 骨格を双子のモーションに合わせたモデルを、使っていない双子の初期衣装に置く
    for (int k = 0; k < 2 && (g_main == kChitose || g_sub == kChitose); ++k)
    {
        const ModelFiles& from = kChitoseFiles[k];
        const ModelFiles& to = ChitoseHome()[k];
        const uint32_t pairs[5][2] = {
            {from.g1m, to.g1m}, {from.grp, to.grp}, {from.mtl, to.mtl}, {from.oid, to.oid},
            {from.ktid, to.ktid}};
        for (const auto& pair : pairs)
        {
            // 中身は千歳、エントリの付属データは置き場所のものを使う
            File chitose, home;
            if (!ReadEntry(src, pair[0], chitose) || !ReadEntry(src, pair[1], home)) return false;
            if (pair[0] == from.g1m && !FitChitose(chitose))
            {
                Log("[NG] g1m 0x%08X: could not fit Chitose's skeleton", pair[0]);
                return false;
            }
            home.data = std::move(chitose.data);
            files.push_back(std::move(home));
        }
        ++fixed;
    }
    // 紗重・八重は縄のグループを常時表示にする（双子のキャラはこの 2 つを表示しない）
    for (Look look : {kSae, kYae})
    {
        if (!g_rope || (g_main != look && g_sub != look)) continue;
        bool merged = false;
        if (!AddMerged(src, look == kSae ? kSaeModel : kYaeModel, kRopeGroups,
                       sizeof(kRopeGroups) / sizeof(kRopeGroups[0]), files, merged))
            return false;
        if (merged) ++fixed;
        else Log("[NG] Could not show the rope of %s; she will appear without it", kLookNamesA[look]);
    }

    // fdata: "PDRK0000", u32 0x10, u32 全体サイズ、その後に 16 バイト境界でエントリ
    std::vector<uint8_t> fdata(0x10);
    memcpy(fdata.data(), "PDRK0000", 8);
    Wr<uint32_t>(&fdata[8], 0x10);
    struct Where { uint32_t off, esize, usize; };
    std::unordered_map<uint32_t, Where> where;
    for (auto& f : files)
    {
        const size_t off = fdata.size();
        const size_t esize = 0x30 + f.extra.size() + f.data.size();
        fdata.resize(off + 0x30);
        uint8_t* e = &fdata[off];
        memcpy(e, "IDRK0000", 8);
        Wr<uint64_t>(e + 0x08, esize);
        Wr<uint64_t>(e + 0x10, f.data.size());
        Wr<uint64_t>(e + 0x18, f.data.size());
        Wr<uint32_t>(e + 0x20, f.type);
        Wr<uint32_t>(e + 0x24, f.hash);
        Wr<uint32_t>(e + 0x28, f.tkid);
        Wr<uint32_t>(e + 0x2C, 0);
        fdata.insert(fdata.end(), f.extra.begin(), f.extra.end());
        fdata.insert(fdata.end(), f.data.begin(), f.data.end());
        fdata.resize((fdata.size() + 15) & ~static_cast<size_t>(15));
        where[f.hash] = {static_cast<uint32_t>(off), static_cast<uint32_t>(esize),
                         static_cast<uint32_t>(f.data.size())};
    }
    Wr<uint32_t>(&fdata[0xC], static_cast<uint32_t>(fdata.size()));

    // rdb: エントリの長さは変えず、サイズ・フラグ・末尾（位置と fdata 番号）だけ書き換える
    std::vector<uint8_t> rdb = src.rdb;
    for (auto& [h, w] : where)
    {
        const RdbEntry& e = src.ents[h];
        const uint64_t ssz = Rd<uint64_t>(&rdb[e.off + 0x10]);
        Wr<uint64_t>(&rdb[e.off + 0x18], w.usize);
        Wr<uint32_t>(&rdb[e.off + 0x2C], 0x20000);   // 非圧縮（Yumia ツールと同じ値）
        uint8_t* f = &rdb[e.off + e.size - ssz];
        if (ssz == 13)
        {
            Wr<uint32_t>(f + 2, w.off);
            Wr<uint32_t>(f + 6, w.esize);
            Wr<int16_t>(f + 10, marker);
        }
        else
        {
            Wr<uint32_t>(f + 2, 0);
            Wr<uint32_t>(f + 6, w.off);
            Wr<uint32_t>(f + 10, w.esize);
            Wr<int16_t>(f + 14, marker);
        }
    }

    // rdx: 末尾に {番号, -1, fdata のハッシュ} を足す
    std::vector<uint8_t> rdx = src.rdx;
    rdx.resize(rdx.size() + 8);
    Wr<int16_t>(&rdx[rdx.size() - 8], marker);
    Wr<int16_t>(&rdx[rdx.size() - 6], -1);
    Wr<uint32_t>(&rdx[rdx.size() - 4], kFdataHash);

    wchar_t name[48];
    swprintf_s(name, L"fdata_package\\0x%08x.fdata", kFdataHash);
    if (!g_io->Write(g_io->self, name, fdata.data(), fdata.size()) ||
        !g_io->Write(g_io->self, kRdx, rdx.data(), rdx.size()) ||
        !g_io->Write(g_io->self, kRdb, rdb.data(), rdb.size()))
    {
        Log("[NG] Cannot hand the swap data to the loader");
        return false;
    }
    Log("[OK] Generated the swap data (%zu files, %d models fixed, %zu bytes)",
        files.size(), fixed, fdata.size());
    return true;
}

// ゲームが root.rdb / root.rdx を初めて開いたときにローダーから呼ばれる。
// キャッシュが有効な間は呼ばれない。
int GenerateSwap(void*, const MixedNutsPatchIo* io, char* note, size_t cap)
{
    const DWORD t0 = GetTickCount();
    g_io = io;
    const bool ok = Generate();
    g_io = nullptr;
    if (!ok)
    {
        sprintf_s(note, cap, "could not build the swap data (see twinswap.log)");
        return 0;
    }
    Log("     (took %lu ms)", GetTickCount() - t0);
    sprintf_s(note, cap, "Main=%s Sub=%s Rope=%d Blindfold=%s", kLookNamesA[g_main],
              kLookNamesA[g_sub], g_rope ? 1 : 0, kBlindfoldNames[g_blindfold]);
    return 1;
}

// ---- 設定 ---------------------------------------------------------------

// 項目が無ければ既定値（入れ替え）を使う。値がどの名前でもなければ、打ち間違いで
// 意図しない入れ替えが起きないよう、そのキャラ本来の見た目のままにする
Look ReadLook(const std::wstring& ini, const wchar_t* key, Look def, Look own)
{
    const std::wstring s = mixednuts::ini::String(ini, L"Swap", key, kLookNames[def]);
    for (int i = kMio; i <= kChitose; ++i)
        if (_wcsicmp(s.c_str(), kLookNames[i]) == 0) return static_cast<Look>(i);
    Log("[NG] [Swap] %s=%s is not mio, mayu, sae, yae or chitose; keeping the original look (%s)",
        Utf8(key).c_str(), Utf8(s).c_str(), kLookNamesA[own]);
    return own;
}

void LoadConfig()
{
    namespace ini = mixednuts::ini;
    const std::wstring file = g_modDir + L"twinswap.ini";
    g_enabled = ini::Bool(file, L"General", L"Enabled", true);
    mixednuts::log::Open(g_modDir, L"twinswap.log", ini::Bool(file, L"General", L"Log", true));
    g_main = ReadLook(file, L"Main", kMayu, kMio);
    g_sub  = ReadLook(file, L"Sub", kMio, kMayu);
    g_rope = ini::Bool(file, L"Swap", L"Rope", true);


    const std::wstring bf = ini::String(file, L"Swap", L"Blindfold", L"default");
    if (_wcsicmp(bf.c_str(), L"show") == 0) g_blindfold = kBfShow;
    else if (_wcsicmp(bf.c_str(), L"hide") == 0) g_blindfold = kBfHide;
    else if (_wcsicmp(bf.c_str(), L"default") != 0)
        Log("[NG] [Swap] Blindfold=%s is not default, show or hide; using default",
            Utf8(bf).c_str());
}

} // namespace

// MixedNuts Mod Loader から、ゲームのコードが動く前に呼ばれる。
// 入れ替えるならファイル改変を登録する（生成はゲームが root.rdb / rdx を開いたとき）。
MIXEDNUTS_PLUGIN_EXPORT int WINAPI MixedNutsPluginInit(const MixedNutsApi* api)
{
    if (!api || api->version < MIXEDNUTS_API_VERSION) return 0;
    g_api    = api;
    g_modDir = api->pluginDir;
    LoadConfig();
    Log("TwinSwap %s  Main=%s Sub=%s Rope=%d Blindfold=%s", kVersion, kLookNamesA[g_main],
        kLookNamesA[g_sub], g_rope ? 1 : 0, kBlindfoldNames[g_blindfold]);

    // 見た目が元のままで、目隠しの設定も既定なら何もしない
    if (!g_enabled || (g_main == kMio && g_sub == kMayu && g_blindfold == kBfDefault))
    {
        Log("[OK] Nothing to do (disabled, or Main=mio / Sub=mayu with Blindfold=default)");
        return 1;
    }

    // tag には結果に影響する設定も入れる。変わればローダーが作り直す
    static char tag[128];
    sprintf_s(tag, "%s main=%s sub=%s rope=%d blindfold=%s", kCacheTag, kLookNamesA[g_main],
              kLookNamesA[g_sub], g_rope ? 1 : 0, kBlindfoldNames[g_blindfold]);
    static const wchar_t* const targets[] = { kRdb, kRdx, nullptr };
    const MixedNutsPatch patch{ targets, tag, &GenerateSwap, nullptr };
    if (!api->RegisterPatch(api, &patch))
    {
        Log("[NG] Could not register with the loader; the game runs unmodified");
        return 0;
    }
    Log("[OK] Registered with the loader");
    return 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(hModule);
    return TRUE;
}
