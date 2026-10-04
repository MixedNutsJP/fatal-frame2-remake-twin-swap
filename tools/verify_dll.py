# プラグインの生成物を、参照実装（twinswap_ref.py）の出力とバイト単位で比べる。
#
#   python verify_dll.py <テスト用のゲームフォルダ> [<mod-loader の dist>]
#
# テスト用のゲームフォルダには fdata_package（root.rdb / root.rdx と fdata）と harness.exe を置いておく。
# harness.exe は xinput1_4.dll を読み込み、fdata_package\root.rdx / root.rdb / Mod の fdata を開く。
# ローダーのプロキシ（dist\dinput8.dll）は名前を変えても動くので、xinput1_4.dll としてコピーする。
# ローダー一式（MixedNuts\）と、このプラグイン（dist\MixedNuts\Mods\twinswap\）もコピーし、
# ini の組み合わせごとに、ローダーのキャッシュ（MixedNuts\cache\fdata_package\）を参照実装と比べる。
import sys, os, shutil, subprocess, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import twinswap_ref as R

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.join(HERE, '..', 'dist', 'MixedNuts', 'Mods', 'twinswap')
LOADER = os.path.join(HERE, '..', '..', 'mod-loader', 'loader', 'dist')
INI = """[Swap]
Main=%s
Sub=%s
Rope=%d
Blindfold=%s
[General]
Enabled=1
Log=1
"""
D = 'default'
CASES = (('mayu', 'mio', 1, D), ('mio', 'mio', 1, D), ('mayu', 'mayu', 1, D),
         ('sae', 'mio', 1, D), ('mayu', 'sae', 1, D), ('yae', 'sae', 1, D), ('sae', 'yae', 1, D),
         ('mio', 'yae', 1, D), ('sae', 'yae', 0, D), ('mio', 'sae', 0, D),
         ('mayu', 'mio', 1, 'show'), ('mayu', 'mio', 1, 'hide'), ('mayu', 'mayu', 1, 'show'),
         ('mio', 'mayu', 1, 'hide'), ('mio', 'mayu', 1, 'show'))


def sha(b):
    return hashlib.sha256(b).hexdigest()[:16]


def main():
    game = sys.argv[1]
    loader = sys.argv[2] if len(sys.argv) > 2 else LOADER
    pkg = os.path.join(game, 'fdata_package')
    root = os.path.join(game, 'MixedNuts')
    mod = os.path.join(root, 'Mods', 'twinswap')
    shutil.copy2(os.path.join(loader, 'dinput8.dll'), os.path.join(game, 'xinput1_4.dll'))
    shutil.copytree(os.path.join(loader, 'MixedNuts'), root, dirs_exist_ok=True)
    shutil.copytree(PLUGIN, mod, dirs_exist_ok=True)
    rdb = open(os.path.join(pkg, 'root.rdb'), 'rb').read()
    rdx = open(os.path.join(pkg, 'root.rdx'), 'rb').read()
    ok_all = True
    for main_, sub, rope, bf in CASES:
        open(os.path.join(mod, 'twinswap.ini'), 'w').write(INI % (main_, sub, rope, bf))
        shutil.rmtree(os.path.join(root, 'cache'), ignore_errors=True)
        for log in (os.path.join(mod, 'twinswap.log'), os.path.join(root, 'loader.log')):
            if os.path.exists(log):
                os.remove(log)
        subprocess.run([os.path.join(game, 'harness.exe')], cwd=game, check=True, capture_output=True)
        want = dict(zip(('root.rdb', 'root.rdx', '0x%08x.fdata' % R.FDATA_HASH),
                        R.build(pkg, rdb, rdx, main_, sub, bool(rope), bf)))
        for name, b in want.items():
            p = os.path.join(root, 'cache', 'fdata_package', name)
            got = open(p, 'rb').read() if os.path.exists(p) else b''
            same = got == b
            ok_all &= same
            print('%-5s %-5s rope=%d %-7s %-18s %s  plugin=%s ref=%s' % (main_, sub, rope, bf, name, 'OK ' if same else 'NG ', sha(got), sha(b)))
        log = open(os.path.join(mod, 'twinswap.log'), encoding='utf-8-sig').read()
        if '[NG]' in log:
            ok_all = False
            print(log)
    print('ALL OK' if ok_all else 'MISMATCH')


if __name__ == '__main__':
    main()
