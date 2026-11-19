#!/usr/bin/env python3
"""
ビルド後にアップデート用ファイルを準備するスクリプト。
使い方: python3 tools/make_update.py <version> <nso_normal> <nso_en>
例:     python3 tools/make_update.py 111 build_normal/subsdk9 build_en/subsdk9
"""
import sys, os, binascii, shutil

def crc32(path):
    with open(path, 'rb') as f:
        return binascii.crc32(f.read()) & 0xFFFFFFFF

def main():
    if len(sys.argv) < 4:
        print(f"使い方: {sys.argv[0]} <version> <nso_normal> <nso_en>")
        sys.exit(1)

    ver = int(sys.argv[1])
    nso_normal = sys.argv[2]
    nso_en = sys.argv[3]

    # NSO マジック確認
    for path in [nso_normal, nso_en]:
        with open(path, 'rb') as f:
            magic = f.read(4)
        if magic != b'NSO0':
            print(f"エラー: {path} は NSO ファイルではありません (先頭: {magic})")
            sys.exit(1)

    sz_normal = os.path.getsize(nso_normal)
    sz_en = os.path.getsize(nso_en)
    crc_normal = crc32(nso_normal)
    crc_en = crc32(nso_en)

    # ディレクトリ作成
    update_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'update')
    for kind in ['tmt', 'tmt_en']:
        dst_dir = os.path.join(update_dir, kind, str(ver))
        os.makedirs(dst_dir, exist_ok=True)

    shutil.copy2(nso_normal, os.path.join(update_dir, 'tmt', str(ver), 'subsdk9'))
    shutil.copy2(nso_en, os.path.join(update_dir, 'tmt_en', str(ver), 'subsdk9'))

    # version.txt 更新
    vt = f"tmt {ver} {sz_normal} {crc_normal:08x}\ntmt_en {ver} {sz_en} {crc_en:08x}\n"
    vt_path = os.path.join(update_dir, 'version.txt')
    with open(vt_path, 'w') as f:
        f.write(vt)

    print(f"version.txt:")
    print(vt)
    print(f"tmt/{ver}/subsdk9    : {sz_normal} bytes, CRC32 {crc_normal:08x}")
    print(f"tmt_en/{ver}/subsdk9 : {sz_en} bytes, CRC32 {crc_en:08x}")
    print("完了!")

if __name__ == '__main__':
    main()
