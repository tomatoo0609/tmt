# 胡桃の模様を、ミップ付き (大きい順に 1024,512,256,128 / 髪は 512,256,128) の BC1 にして SD 用の tex0.bin tex1.bin tex2.bin を作る。
# 使い方: PMX のあるフォルダ (tex/hair2.png = 发.png の別名) で python3 -I hutao_mips.py <出力フォルダ>
import sys, os, numpy as np
from PIL import Image
src = open('/home/claude/tmt/tools/hutao_export.py').read()
a = src.index('def bc1(img):'); b = src.index("with open(sys.argv[2]")
ns = {'np': np}; exec(src[a:b], ns); bc1 = ns['bc1']
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
jobs = [('tex/面.png', 1024, 0), ('tex/hair2.png', 512, 1), ('tex/服.png', 1024, 2)]
for path, sz0, idx in jobs:
    im0 = Image.open(path).convert('RGBA'); data = b''; sz = sz0
    while sz >= 128:
        im = im0.resize((sz, sz), Image.LANCZOS); data += bc1(np.array(im)).tobytes(); sz //= 2
    open(os.path.join(out, 'tex%d.bin' % idx), 'wb').write(data); print(idx, len(data))
