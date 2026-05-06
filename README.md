# rp2350b_gamepad

Raspberry Pi Pico 2 によるゲームパッドのベアメタル実装。  
[pico_hid_minimal](https://github.com/dskk/pico_hid_minimal) の multimode サンプルをベースに作成。

[24f2672](https://github.com/dskk/rp2350b_gamepad/commit/24f2672c33e5cf9d05913a92e62e332bfde2b36e) のコミットが最低限の実装を示したもので、GPIO の読みをそのまま HID Report として送信する動作になっている。

# build

.bashrc で export するなどの方法で、環境変数 `PICO_SDK_PATH` は設定済みとする。

```bash
mkdir build
cd build
cmake -DPICO_BOARD=pico2 ..
make
```

cmake のオプションはボード次第。 `cmake -DPICO_BOARD=pimoroni_pico_plus2_rp2350 ..` など。
