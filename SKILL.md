---
name: max-external
description: Max/MSPのエクスターナルオブジェクトをmin-api + CMakeで作成する
---

# max-external

## 前提

- macOS
- CMake 3.19+
- Xcode CLI tools
- min-api / max-sdk-base は git submodule として `deps/min-api/` に配置済みであること

## プロジェクト構造

```
project/
├── CMakeLists.txt              # ルートCMake (templates/CMakeLists.root.txt ベース)
├── cmake/
│   ├── bbb_external.cmake      # bbb_add_external() 定義 (共通)
│   └── generate_version.cmake  # バージョン生成 (オプション)
├── deps/
│   └── min-api/                # git submodule (max-sdk-base を含む)
├── source/
│   ├── projects/
│   │   └── bbb.xxx.yyy/        # external ごとのディレクトリ
│   │       ├── CMakeLists.txt  # bbb_add_external() 呼び出しのみ (1〜5行)
│   │       ├── bbb.xxx.yyy.cpp
│   │       └── bbb.xxx.yyy.maxhelp
│   └── bbb/                    # 共有ヘッダ (任意)
├── externals/                  # ビルド成果物 (.mxo)
├── help/                       # helpファイルのコピー
└── package-info.json
```

## 新規 external の追加手順

外部名を `NAME` (例: `bbb.osc.send`) とする。

### 1. ディレクトリとソース作成

```
source/projects/<NAME>/<NAME>.cpp
source/projects/<NAME>/CMakeLists.txt
```

cpp は `templates/external.cpp` をベースに生成する。
プレースホルダ置換ルール:

| プレースホルダ | 置換値 | 例 |
|---|---|---|
| `__CLASS_NAME__` | ドットをアンダースコアに | `bbb_osc_send` |
| `__EXTERNAL_NAME__` | そのまま | `bbb.osc.send` |
| `__DESCRIPTION__` | external の説明 | `Send OSC messages over UDP` |
| `__TAGS__` | カンマ区切りタグ | `osc, udp, network` |
| `__AUTHOR__` | 作者名 | `2bit` |

### 2. サブプロジェクト CMakeLists.txt

`templates/CMakeLists.project.txt` をベースにする。
基本的にコメントアウトを外すだけで動く:

```cmake
# 依存ライブラリがある場合:
bbb_add_external(
    DEPS pthread somelib
    INCLUDES ${SOME_DIR}/include
)

# 依存なし (最小構成):
bbb_add_external()
```

### 3. ルート CMakeLists.txt は変更不要

`SUBDIRLIST` マクロが `source/projects/` 以下を自動スキャンする。
新しいディレクトリを追加するだけで認識される。

### 4. package-info.json の filelist に .mxo を追加

```json
"filelist": {
    "externals/bbb.osc.send.mxo": {}
}
```

### 5. ビルド

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

成果物は `externals/<NAME>.mxo` に出力される。

## bbb_add_external() リファレンス

`cmake/bbb_external.cmake` で定義される共通 function。

```cmake
bbb_add_external(
    [MACOS_ONLY]                  # macOS のみビルド (Windows ではスキップ)
    [WIN32_ONLY]                  # Windows のみビルド (macOS ではスキップ)
    [DEPS lib1 lib2 ...]          # target_link_libraries に渡す依存
    [INCLUDES dir1 dir2 ...]      # target_include_directories に渡す追加パス
    [SOURCES file1.cpp ...]       # 追加ソース (省略時: *.cpp を自動収集)
    [RPATH path]                  # BUILD_RPATH / INSTALL_RPATH を設定
    [NO_HELP_COPY]                # help ファイルの自動コピーを無効化
)
```

内部で以下を処理する:
- `C74_MIN_API_DIR` の自動探索 (`deps/min-api/` or `extern/min-api/`)
- min-api の pretarget / post-target script の include
- `add_library(MODULE ...)` による .mxo / .mxe64 ビルド
- macOS Universal Binary (`x86_64;arm64`) の設定
- `MACOS_ONLY` / `WIN32_ONLY` によるプラットフォームガード
- help ファイルの `help/` へのコピー

## 命名規則

| レイヤー | 形式 | 例 |
|----------|------|-----|
| ディレクトリ/ファイル名 | `bbb.xxx.yyy` (ドット区切り) | `bbb.osc.send` |
| C++ クラス名 | `bbb_xxx_yyy` (アンダースコア区切り) | `bbb_osc_send` |
| MIN_EXTERNAL マクロ引数 | クラス名と同一 | `bbb_osc_send` |
| .mxo ファイル名 | ディレクトリ名と同一 | `bbb.osc.send.mxo` |

## バージョン管理 (オプション)

git commit count から自動生成する。
`templates/generate_version.cmake` と `templates/CMakeLists.root.txt` のバージョン生成セクションを参照。
`VERSION_MACRO` 変数でマクロ名を指定する (例: `BBB_OSC_VERSION`)。

## トラップ・注意事項

**必ず `docs/pitfalls.md` を読むこと。** attribute の遅延設定、スレッド制限、enum_map、NIL 衝突等の重要な注意点が記載されている。
