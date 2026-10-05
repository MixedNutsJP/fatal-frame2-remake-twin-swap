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
#include <climits>
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
constexpr char     kCacheTag[] = "twinswap-v23";   // 生成ロジックを変えたら上げる
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
bool g_chitoseHuman = false;   // 千歳の肌を人間の色にするか
bool g_saeYaeGhost  = false;   // 紗重・八重の肌を幽霊の色にするか

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

// 紗重・八重は白い着物（生前の姿）の 1 着だけで、高精細・軽量の区別も無い。腰に縄を巻くだけの
// 方が紗重、縄が長く垂れている方が八重（2.1.0 では逆にしていた。ゲーム内で見比べた利用者の
// 指摘で 2.2.0 で直した）。どちらも顔は常時表示のグループ 0 にあり、顔の修正は要らない

// 立花千歳。顔は常時表示のグループ 0 にあり、ほかのグループ（7f621c22 / abdec7fe）は目まわりの
// 差分で、双子のキャラが表示する 7f621c22 で足りるので、グループの修正は要らない。
// 千歳は背が低く、腰の骨の高さが 74.16（双子は 87.25）。双子のモーションは腰を 87.25 に置くので、
// そのままでは体が 13.09 持ち上がって足が浮く。骨格を書き換えて合わせる（FitChitose）
constexpr float kChitoseDrop = 87.25f - 74.16f;

// 紗重・八重・千歳は、モデルの写しを別のファイルに置いて使う。本来の紗重・八重・千歳（別の枠
// から同じモデルを使う）に影響させないため。置き場所は、見た目として使われていない双子の
// 初期衣装（高精細・軽量）。双子の枠はすべてこの Mod が書き換えるので、そのモデル定義を指す枠は
// 写しを使う見た目のものだけになる。
// モデル定義が参照するファイルのうち、g1m / grp / mtl / oid / ktid を写し元のもので上書きする
// （oid は g1m のエントリの付属データが、ktid はモデル定義が間接的に参照している）。
// db はモデルごとの kidsobjdb（補助の骨を動かす計算の定義が 1 個入っている）。紗重・八重は
// 双子と中身が違うので、これも写す。千歳は双子のもので確認が取れているので写さない（0）。
// 2.3.0 までは、紗重・八重は本来のモデル定義をそのまま使い、縄を表示するために本来のモデルを
// 書き換えていた（イベントに登場する紗重・八重の縄も常に表示されていた）。
// 紗重・八重は、着物の袖が立ち止まっていてもなびき続ける。2.3.0 までの置き方でも写しでも同じで、
// 原因は分かっていない（置き場所のモデル定義から、骨に揺れを与える設定を外しても、キャラごとの
// 識別子を写し元のものにしても変わらなかった）
struct ModelFiles { uint32_t g1m, grp, mtl, oid, ktid, db; };

// 写しを使う見た目（紗重・八重・千歳）1 人ぶん。files は高精細・軽量（紗重・八重は 1 体だけなので同じものを並べる）。
// faceObj / handObj は、それぞれの ktid の中で顔と手足のテクスチャを指すオブジェクト。
// faceG1t / handG1t は、そのテクスチャの実ファイル
struct Extra {
    ModelFiles files[2];
    uint32_t   faceObj[2], handObj[2];
    uint32_t   faceG1t, handG1t;
};
const Extra kSaeExtra = {
    {{0x9649ABE6, 0x9D393D64, 0x9684C424, 0x8843E55F, 0x80765F55, 0xEA30890C},
     {0x9649ABE6, 0x9D393D64, 0x9684C424, 0x8843E55F, 0x80765F55, 0xEA30890C}},
    {0xAF727D4F, 0xAF727D4F}, {0x5C5C8C6F, 0x5C5C8C6F}, 0x742128BD, 0x1C0FB7DD};
const Extra kYaeExtra = {
    {{0xE92E0AFF, 0xF01D9C7D, 0xE969233D, 0xDB284478, 0x8A1DE35C, 0xF46E1125},
     {0xE92E0AFF, 0xF01D9C7D, 0xE969233D, 0xDB284478, 0x8A1DE35C, 0xF46E1125}},
    {0x82465651, 0x82465651}, {0x2F306571, 0x2F306571}, 0x60B77D78, 0x08A60C98};
const Extra kChitoseExtra = {
    {{0x91C71644, 0x98B6A7C2, 0x92022E82, 0x83C14FBD, 0xF4A640B7, 0},
     {0x171B23BA, 0x1E0AB538, 0x17563BF8, 0x09155D33, 0x19D3E201, 0}},
    {0xD90F8A1F, 0xDDAF9A77}, {0x8708793F, 0x396AD757}, 0xFF963D2B, 0xE1683C4B};

// 置き場所。defs / files は初期衣装の高精細・軽量。slotObj / slotG1t は、色を変えた肌のテクスチャ
// （顔、手足）を置く枠。この双子の初期衣装の ktid だけが使っているテクスチャから 2 つ選んだ
struct Home {
    uint32_t   defs[2];
    ModelFiles files[2];
    uint32_t   slotObj[2], slotG1t[2];
};
const Home kMayuHome = {
    {0x49118300, 0x0B93BA76},
    {{0x7DCC4616, 0x84BBD794, 0x7E075E54, 0x6FC67F8F, 0x89470B25, 0x88BA7F80},
     {0xD9C21C60, 0xE0B1ADDE, 0xD9FD349E, 0xCBBC55D9, 0xAC0BFE1B, 0x17810486}},
    {0x2ABDFA33, 0xCD7BE040}, {0x0D81DFD3, 0xDF669026}};
const Home kMioHome = {
    {0x511668A8, 0x36546A72},
    {{0xCADE596E, 0xD1CDEAEC, 0xCB1971AC, 0xBCD892E7, 0xDE7762CD, 0x95EF3E94},
     {0xB50F91E4, 0xBBFF2362, 0xB54AAA22, 0xA709CB5D, 0x3A6D3917, 0xF56100CE}},
    {0xF8760DA1, 0x3DA012EE}, {0xDAB28B97, 0x761D04E4}};

const Extra& ExtraOf(Look look)
{
    return look == kSae ? kSaeExtra : look == kYae ? kYaeExtra : kChitoseExtra;
}

// 肌の色を変えるか。千歳は人間の肌色に、紗重・八重は幽霊の肌色にできる
bool SkinChanged(Look look) { return look == kChitose ? g_chitoseHuman : g_saeYaeGhost; }

// 写しを使う見た目か
bool UsesCopy(Look look) { return look == kSae || look == kYae || look == kChitose; }

// 写しの置き場所。空いている双子を繭、澪の順に、Main、Sub の順で割り当てる。
// 写しを使う見た目が 2 種類なら双子は 2 人とも空いていて、1 種類なら少なくとも 1 人は空いている
const Home& HomeFor(Look look)
{
    const Home* free[2] = {};
    int n = 0;
    if (g_main != kMayu && g_sub != kMayu) free[n++] = &kMayuHome;
    if (g_main != kMio && g_sub != kMio) free[n++] = &kMioHome;
    return *free[(look == g_main || !UsesCopy(g_main)) ? 0 : 1];
}

uint32_t DefFor(Look look, int costume, int detail)
{
    if (UsesCopy(look)) return HomeFor(look).defs[detail];
    return look == kMio ? kMioDefs[costume][detail] : kMayuDefs[costume][detail];
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

// 紗重・八重の縄（部品 @1EED9A49）はグループ 768a168d と 6ad387ac にあり、双子のキャラは
// この 2 つを表示しないので、顔と同じ方法で常時表示のグループ 0 へ移す。Rope=0 なら、目隠しと
// 同じ方法で縄の部品を描画されないようにする（どちらもモデルの写しに対して行う）
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

// ---- 肌の色 -------------------------------------------------------------
// 肌のテクスチャ（BC1 の g1t）に、別のテクスチャの「なだらかな色の分布」を移す。顔・手足の
// テクスチャは、千歳・紗重・八重で配置（UV）が同じなので、場所ごとの色の比を掛ければ、絵柄を
// 保ったまま肌の色だけが移る。手順は tools\skin_tone.py と同じで、整数だけで計算する
// （出力をバイト単位で突き合わせるため）。
//   1. 両方の最上位ミップを RGB に展開する
//   2. 64 ピクセル角のます目ごとに色の合計を取り、[1,2,1] のぼかしを縦横に 2 回かける
//   3. ます目ごとに 色の元 / 対象 の比（12 ビット固定小数、上限 4 倍）を作る
//   4. 比をピクセルへ双線形で広げて対象に掛ける
//   5. ミップを 2x2 の平均で作り直し、すべて BC1 に圧縮して、対象の g1t の画素部分へ書き戻す

constexpr int     kToneCell = 64, kTonePasses = 2;
constexpr int64_t kToneBias = 4;              // 暗い所で比が暴れないように、分子と分母に足す量
constexpr int64_t kToneMax  = 4 << 12;

struct G1tInfo { uint32_t w, h, mips; size_t data; };

// BC1 のテクスチャ 1 枚だけの g1t に限る。+0xC 表の位置、+0x10 枚数、表の先にテクスチャのヘッダー
// （+0 上位 4 ビットがミップ数、+1 形式、+2 上位が高さ・下位が幅の log2）。画素は末尾に並ぶ
bool ReadG1t(const std::vector<uint8_t>& d, G1tInfo& out)
{
    if (d.size() < 0x24 || memcmp(d.data(), "GT1G", 4) != 0 || Rd<uint32_t>(&d[0x10]) != 1) return false;
    const size_t table = Rd<uint32_t>(&d[0xC]);
    if (table + 4 > d.size()) return false;
    const size_t th = table + Rd<uint32_t>(&d[table]);
    if (th + 8 > d.size() || d[th + 1] != 0x59) return false;
    out.mips = d[th] >> 4;
    out.w = 1u << (d[th + 2] & 15);
    out.h = 1u << (d[th + 2] >> 4);
    if (out.mips == 0 || out.w % kToneCell || out.h % kToneCell ||
        (out.w >> (out.mips - 1)) < 4 || (out.h >> (out.mips - 1)) < 4) return false;
    size_t size = 0;
    for (uint32_t m = 0; m < out.mips; ++m) size += static_cast<size_t>((out.w >> m) / 4) * ((out.h >> m) / 4) * 8;
    if (size + th + 8 > d.size()) return false;
    out.data = d.size() - size;
    return true;
}

void Expand565(uint32_t c, int e[3])
{
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    e[0] = (r << 3) | (r >> 2);
    e[1] = (g << 2) | (g >> 4);
    e[2] = (b << 3) | (b >> 2);
}

// BC1 のブロック列を RGB（1 ピクセル 3 バイト）に展開する
void Bc1Decode(const uint8_t* src, uint32_t w, uint32_t h, std::vector<uint8_t>& out)
{
    out.resize(static_cast<size_t>(w) * h * 3);
    for (uint32_t by = 0; by < h / 4; ++by)
        for (uint32_t bx = 0; bx < w / 4; ++bx, src += 8)
        {
            const uint32_t c0 = Rd<uint16_t>(src), c1 = Rd<uint16_t>(src + 2), bits = Rd<uint32_t>(src + 4);
            int pal[4][3];
            Expand565(c0, pal[0]);
            Expand565(c1, pal[1]);
            for (int k = 0; k < 3; ++k)
            {
                pal[2][k] = c0 > c1 ? (2 * pal[0][k] + pal[1][k] + 1) / 3 : (pal[0][k] + pal[1][k]) / 2;
                pal[3][k] = c0 > c1 ? (pal[0][k] + 2 * pal[1][k] + 1) / 3 : 0;
            }
            for (int i = 0; i < 16; ++i)
            {
                const int* p = pal[(bits >> (2 * i)) & 3];
                uint8_t* q = &out[(static_cast<size_t>(by * 4 + i / 4) * w + bx * 4 + i % 4) * 3];
                q[0] = static_cast<uint8_t>(p[0]);
                q[1] = static_cast<uint8_t>(p[1]);
                q[2] = static_cast<uint8_t>(p[2]);
            }
        }
}

// RGB を BC1 に圧縮して out の末尾へ足す。端点は、ブロック内の色の範囲を少し内側へ寄せたもの
void Bc1Encode(const std::vector<uint8_t>& img, uint32_t w, uint32_t h, std::vector<uint8_t>& out)
{
    for (uint32_t by = 0; by < h / 4; ++by)
        for (uint32_t bx = 0; bx < w / 4; ++bx)
        {
            int px[16][3], mn[3] = {255, 255, 255}, mx[3] = {0, 0, 0};
            for (int i = 0; i < 16; ++i)
            {
                const uint8_t* q = &img[(static_cast<size_t>(by * 4 + i / 4) * w + bx * 4 + i % 4) * 3];
                for (int k = 0; k < 3; ++k)
                {
                    px[i][k] = q[k];
                    if (q[k] < mn[k]) mn[k] = q[k];
                    if (q[k] > mx[k]) mx[k] = q[k];
                }
            }
            for (int k = 0; k < 3; ++k)
            {
                const int inset = (mx[k] - mn[k]) >> 4;
                mn[k] += inset;
                mx[k] -= inset;
            }
            auto pack = [](const int v[3]) {
                return static_cast<uint32_t>((((v[0] * 31 + 127) / 255) << 11) |
                                             (((v[1] * 63 + 127) / 255) << 5) | ((v[2] * 31 + 127) / 255));
            };
            const uint32_t c0 = pack(mx), c1 = pack(mn);
            uint32_t bits = 0;
            if (c0 != c1)
            {
                int pal[4][3];
                Expand565(c0, pal[0]);
                Expand565(c1, pal[1]);
                for (int k = 0; k < 3; ++k)
                {
                    pal[2][k] = (2 * pal[0][k] + pal[1][k] + 1) / 3;
                    pal[3][k] = (pal[0][k] + 2 * pal[1][k] + 1) / 3;
                }
                for (int i = 0; i < 16; ++i)
                {
                    int best = 0, bestD = INT_MAX;
                    for (int p = 0; p < 4; ++p)
                    {
                        int dist = 0;
                        for (int k = 0; k < 3; ++k) dist += (px[i][k] - pal[p][k]) * (px[i][k] - pal[p][k]);
                        if (dist < bestD) { bestD = dist; best = p; }
                    }
                    bits |= static_cast<uint32_t>(best) << (2 * i);
                }
            }
            const size_t at = out.size();
            out.resize(at + 8);
            Wr<uint16_t>(&out[at], static_cast<uint16_t>(c0));
            Wr<uint16_t>(&out[at + 2], static_cast<uint16_t>(c1));
            Wr<uint32_t>(&out[at + 4], bits);
        }
}

// ます目ごとの色の合計に、[1,2,1] のぼかしを縦横にかけたもの（割らずに持つ）
void ToneGrid(const std::vector<uint8_t>& img, uint32_t w, uint32_t h, std::vector<int64_t>& grid)
{
    const uint32_t gw = w / kToneCell, gh = h / kToneCell;
    grid.assign(static_cast<size_t>(gw) * gh * 3, 0);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            for (int k = 0; k < 3; ++k)
                grid[(static_cast<size_t>(y / kToneCell) * gw + x / kToneCell) * 3 + k] +=
                    img[(static_cast<size_t>(y) * w + x) * 3 + k];
    std::vector<int64_t> tmp(grid.size());
    for (int pass = 0; pass < kTonePasses; ++pass)
    {
        for (uint32_t y = 0; y < gh; ++y)       // 横
            for (uint32_t x = 0; x < gw; ++x)
                for (int k = 0; k < 3; ++k)
                {
                    const uint32_t l = x ? x - 1 : 0, r = x + 1 < gw ? x + 1 : gw - 1;
                    tmp[(static_cast<size_t>(y) * gw + x) * 3 + k] =
                        grid[(static_cast<size_t>(y) * gw + l) * 3 + k] +
                        2 * grid[(static_cast<size_t>(y) * gw + x) * 3 + k] +
                        grid[(static_cast<size_t>(y) * gw + r) * 3 + k];
                }
        for (uint32_t y = 0; y < gh; ++y)       // 縦
            for (uint32_t x = 0; x < gw; ++x)
                for (int k = 0; k < 3; ++k)
                {
                    const uint32_t u = y ? y - 1 : 0, b = y + 1 < gh ? y + 1 : gh - 1;
                    grid[(static_cast<size_t>(y) * gw + x) * 3 + k] =
                        tmp[(static_cast<size_t>(u) * gw + x) * 3 + k] +
                        2 * tmp[(static_cast<size_t>(y) * gw + x) * 3 + k] +
                        tmp[(static_cast<size_t>(b) * gw + x) * 3 + k];
                }
    }
}

// target（g1t）に source（g1t）の色味を移す。大きさとミップ数が同じであること
bool Recolor(std::vector<uint8_t>& target, const std::vector<uint8_t>& source)
{
    G1tInfo t, s;
    if (!ReadG1t(target, t) || !ReadG1t(source, s) || t.w != s.w || t.h != s.h || t.mips != s.mips)
        return false;
    const uint32_t w = t.w, h = t.h, gw = w / kToneCell, gh = h / kToneCell;
    std::vector<uint8_t> img, other;
    Bc1Decode(&target[t.data], w, h, img);
    Bc1Decode(&source[s.data], w, h, other);
    std::vector<int64_t> lowT, lowS;
    ToneGrid(img, w, h, lowT);
    ToneGrid(other, w, h, lowS);
    int64_t bias = kToneBias * kToneCell * kToneCell;
    for (int pass = 0; pass < kTonePasses; ++pass) bias *= 16;
    std::vector<int64_t> ratio(lowT.size());
    for (size_t i = 0; i < ratio.size(); ++i)
    {
        const int64_t r = ((lowS[i] + bias) << 12) / (lowT[i] + bias);
        ratio[i] = r < kToneMax ? r : kToneMax;
    }
    // ピクセル x に対する、左のます目・右のます目・右の重み（0〜127）
    auto axis = [](uint32_t n, uint32_t g, std::vector<uint32_t>& g0, std::vector<uint32_t>& g1,
                   std::vector<int64_t>& wt) {
        g0.resize(n); g1.resize(n); wt.resize(n);
        for (uint32_t i = 0; i < n; ++i)
        {
            const int v = 2 * static_cast<int>(i) + 1 - kToneCell;
            g0[i] = v < 0 ? 0 : static_cast<uint32_t>(v / (2 * kToneCell));
            wt[i] = v < 0 ? 0 : v % (2 * kToneCell);
            g1[i] = g0[i] + 1 < g ? g0[i] + 1 : g - 1;
        }
    };
    std::vector<uint32_t> x0, x1, y0, y1;
    std::vector<int64_t> wx, wy;
    axis(w, gw, x0, x1, wx);
    axis(h, gh, y0, y1, wy);
    const int64_t full = 2 * kToneCell;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            for (int k = 0; k < 3; ++k)
            {
                auto at = [&](uint32_t gy, uint32_t gx) { return ratio[(static_cast<size_t>(gy) * gw + gx) * 3 + k]; };
                const int64_t r = (at(y0[y], x0[x]) * (full - wx[x]) * (full - wy[y]) +
                                   at(y0[y], x1[x]) * wx[x] * (full - wy[y]) +
                                   at(y1[y], x0[x]) * (full - wx[x]) * wy[y] +
                                   at(y1[y], x1[x]) * wx[x] * wy[y]) >> 14;
                uint8_t& p = img[(static_cast<size_t>(y) * w + x) * 3 + k];
                const int64_t v = (p * r + 2048) >> 12;
                p = static_cast<uint8_t>(v < 255 ? v : 255);
            }
    std::vector<uint8_t> out(target.begin(), target.begin() + t.data);
    uint32_t mw = w, mh = h;
    for (uint32_t m = 0; m < t.mips; ++m)
    {
        Bc1Encode(img, mw, mh, out);
        std::vector<uint8_t> half(static_cast<size_t>(mw / 2) * (mh / 2) * 3);
        for (uint32_t y = 0; y < mh / 2; ++y)
            for (uint32_t x = 0; x < mw / 2; ++x)
                for (int k = 0; k < 3; ++k)
                {
                    const size_t a = (static_cast<size_t>(2 * y) * mw + 2 * x) * 3 + k;
                    half[(static_cast<size_t>(y) * (mw / 2) + x) * 3 + k] = static_cast<uint8_t>(
                        (img[a] + img[a + 3] + img[a + static_cast<size_t>(mw) * 3] +
                         img[a + static_cast<size_t>(mw) * 3 + 3] + 2) >> 2);
                }
        img.swap(half);
        mw /= 2;
        mh /= 2;
    }
    if (out.size() != target.size()) return false;
    target.swap(out);
    return true;
}

// ktid（8 バイトの並び: 番号, オブジェクト）の中で、オブジェクト from を to に差し替える
bool ReplaceObject(std::vector<uint8_t>& ktid, uint32_t from, uint32_t to)
{
    int found = 0;
    for (size_t i = 0; i + 8 <= ktid.size(); i += 8)
        if (Rd<uint32_t>(&ktid[i + 4]) == from) { Wr<uint32_t>(&ktid[i + 4], to); ++found; }
    return found == 1;
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
    // 紗重・八重・千歳
    for (Look look : {kSae, kYae, kChitose})
    {
        if (g_main != look && g_sub != look) continue;
        // モデルの写しを、使っていない双子の初期衣装に置く。
        // 中身は写し元、エントリの付属データは置き場所のものを使う
        const Extra& ex = ExtraOf(look);
        const Home& home = HomeFor(look);
        const bool skin = SkinChanged(look);
        for (int k = 0; k < 2; ++k)
        {
            const ModelFiles& from = ex.files[k];
            const ModelFiles& to = home.files[k];
            const uint32_t pairs[5][2] = {
                {from.g1m, to.g1m}, {from.grp, to.grp}, {from.mtl, to.mtl}, {from.oid, to.oid},
                {from.ktid, to.ktid}};
            File copy[5], dest[5];
            for (int n = 0; n < 5; ++n)
                if (!ReadEntry(src, pairs[n][0], copy[n]) || !ReadEntry(src, pairs[n][1], dest[n]))
                    return false;
            if (look == kChitose)
            {
                if (!FitChitose(copy[0]))
                {
                    Log("[NG] g1m 0x%08X: could not fit Chitose's skeleton", from.g1m);
                    return false;
                }
            }
            // 縄: 表示するなら、縄のグループを常時表示のグループ 0 へ移す（双子のキャラはこの 2 つを
            // 表示しない）。表示しないなら、縄の部品を描画されないようにする（移さないだけだと、
            // カットシーンではモデル定義の表示設定が使われて縄が出る）
            else if (g_rope)
            {
                if (!MergeGroups(copy[0], copy[1], kRopeGroups, sizeof(kRopeGroups) / sizeof(kRopeGroups[0])))
                    Log("[NG] Could not show the rope of %s; she will appear without it", kLookNamesA[look]);
            }
            else if (!HideGroups(copy[0], copy[1], kRopeGroups, sizeof(kRopeGroups) / sizeof(kRopeGroups[0])))
                Log("[NG] Could not hide the rope of %s; it may appear in cutscenes", kLookNamesA[look]);
            if (skin && (!ReplaceObject(copy[4].data, ex.faceObj[k], home.slotObj[0]) ||
                         !ReplaceObject(copy[4].data, ex.handObj[k], home.slotObj[1])))
            {
                Log("[NG] ktid 0x%08X: the skin textures are not found", from.ktid);
                return false;
            }
            for (int n = 0; n < 5; ++n)
            {
                dest[n].data = std::move(copy[n].data);
                files.push_back(std::move(dest[n]));
            }
            if (from.db)
            {
                // kidsobjdb: "_DOK0000" …、+0x14 に DB の識別子、+0x1C から "IDOK0000" のオブジェクトが
                // 1 個で、+0x28 がその名前（= ファイルのハッシュ）。識別子と名前は置き場所のものにする
                File db, homeDb;
                if (!ReadEntry(src, from.db, db) || !ReadEntry(src, to.db, homeDb)) return false;
                if (db.data.size() < 0x2C || homeDb.data.size() < 0x2C ||
                    memcmp(&db.data[0], "_DOK", 4) != 0 || memcmp(&db.data[0x1C], "IDOK", 4) != 0 ||
                    Rd<uint32_t>(&db.data[0x28]) != from.db || Rd<uint32_t>(&homeDb.data[0x28]) != to.db)
                {
                    Log("[NG] kidsobjdb 0x%08X has an unknown layout", from.db);
                    return false;
                }
                Wr<uint32_t>(&db.data[0x14], Rd<uint32_t>(&homeDb.data[0x14]));
                Wr<uint32_t>(&db.data[0x28], to.db);
                homeDb.data = std::move(db.data);
                files.push_back(std::move(homeDb));
            }
        }
        if (skin)   // 千歳は紗重の、紗重・八重は千歳の色味を移す
        {
            const Extra& tone = look == kChitose ? kSaeExtra : kChitoseExtra;
            const uint32_t pairs[2][2] = {{ex.faceG1t, tone.faceG1t}, {ex.handG1t, tone.handG1t}};
            for (int n = 0; n < 2; ++n)
            {
                File own, other, slot;
                if (!ReadEntry(src, pairs[n][0], own) || !ReadEntry(src, pairs[n][1], other) ||
                    !ReadEntry(src, home.slotG1t[n], slot))
                    return false;
                if (!Recolor(own.data, other.data))
                {
                    Log("[NG] g1t 0x%08X: could not change the skin colour", pairs[n][0]);
                    return false;
                }
                slot.data = std::move(own.data);
                files.push_back(std::move(slot));
            }
        }
        ++fixed;
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

    const std::wstring cs = ini::String(file, L"Swap", L"ChitoseSkin", L"default");
    if (_wcsicmp(cs.c_str(), L"human") == 0) g_chitoseHuman = true;
    else if (_wcsicmp(cs.c_str(), L"default") != 0)
        Log("[NG] [Swap] ChitoseSkin=%s is not default or human; using default", Utf8(cs).c_str());
    const std::wstring ss = ini::String(file, L"Swap", L"SaeYaeSkin", L"default");
    if (_wcsicmp(ss.c_str(), L"ghost") == 0) g_saeYaeGhost = true;
    else if (_wcsicmp(ss.c_str(), L"default") != 0)
        Log("[NG] [Swap] SaeYaeSkin=%s is not default or ghost; using default", Utf8(ss).c_str());

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
    sprintf_s(tag, "%s main=%s sub=%s rope=%d blindfold=%s human=%d ghost=%d", kCacheTag,
              kLookNamesA[g_main], kLookNamesA[g_sub], g_rope ? 1 : 0, kBlindfoldNames[g_blindfold],
              g_chitoseHuman ? 1 : 0, g_saeYaeGhost ? 1 : 0);
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
