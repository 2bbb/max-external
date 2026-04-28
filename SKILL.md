---
name: max-external
description: Max/MSPのエクスターナルオブジェクトをmin-api + CMakeで作成する
---

# max-external

## 前提

- macOS / Windows
- CMake 3.19+
- macOS: Xcode CLI tools
- Windows: Visual Studio 2022 (C++ デスクトップ開発ワークロード)
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
│   │       └── bbb.xxx.yyy.cpp
│   └── bbb/                    # 共有ヘッダ (任意)
├── help/                       # .maxhelp ファイル (正規の場所)
├── externals/                  # ビルド成果物 (.mxo / .mxe64)
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
Jitter MOP オブジェクトの場合は `templates/external_jitter.cpp` をベースにする。
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

成果物は `externals/<NAME>.mxo` (macOS) / `externals/<NAME>.mxe64` (Windows) に出力される。

### bbb_add_external() の内部処理

`cmake/bbb_external.cmake` は min-api の `min-pretarget.cmake` / `min-posttarget.cmake`
を include する。pretarget/posttarget は各 external ごとに `project()` が呼ばれることを
前提としているが、`bbb_add_external()` はマクロ内でこれらを include した後、
ディレクトリスコープ変数を `PARENT_SCOPE` で伝播させることで `add_subdirectory()` 
構造でも正しく動作するようにしている。

プラットフォームごとの設定は pretarget/posttarget スクリプト経由で適用される:
- **macOS**: `BUNDLE True`, `BUNDLE_EXTENSION "mxo"`, Universal Binary (`x86_64;arm64`),
  `MaxAudioAPI` / `JitterAPI` framework リンク, `PkgInfo` コピー, ad-hoc codesign
- **Windows**: `SUFFIX ".mxe64"`, `MaxAPI.lib` / `MaxAudio.lib` / `jitlib.lib` リンク,
  `RUNTIME_OUTPUT_DIRECTORY` 設定, `WIN_VERSION` 定義
- **共通**: `CXX_STANDARD 17`, `PREFIX ""`, `DC74_MIN_API` 定義, `C74_MIN_API_DIR` / `C74_MAX_SDK_DIR` 自動導出

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
)
```

内部で以下を処理する:
- `C74_MIN_API_DIR` の自動探索 (`deps/min-api/` or `extern/min-api/`)
- min-api の pretarget / posttarget script の include
- `add_library(MODULE ...)` による .mxo / .mxe64 ビルド
- macOS Universal Binary (`x86_64;arm64`) の設定
- Windows `.mxe64` サフィックス・lib リンク・出力ディレクトリ設定
- `MACOS_ONLY` / `WIN32_ONLY` オプションによるプラットフォームフィルタリング

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

## Jitter (matrix_operator<>) External

Jitter MOP オブジェクトを作成する場合は `matrix_operator<>` を継承し、
`templates/external_jitter.cpp` をベースにする。

### 基本構造

```cpp
class my_jitter : public c74::min::object<my_jitter>,
                  public c74::min::matrix_operator<> {
    // ...
};
```

**テンプレート引数なし。** `matrix_operator<my_jitter>` はコンパイルエラー。

### calc_cell パターン

`calc_cell` はピクセル単位で呼ばれる。`plane_count` は 1, 4, 32 等で複数実体化されるため、
`if constexpr` でガード必須:

```cpp
template <class matrix_type, size_t plane_count>
c74::min::cell<matrix_type, plane_count> calc_cell(
    c74::min::cell<matrix_type, plane_count> input,
    const c74::min::matrix_info& info,
    c74::min::matrix_coord& position)
{
    if constexpr (plane_count == 4) { /* RGBA */ }
    return input;
}
```

### Sink (入力 matrix を処理)

`info.m_bip` が生ピクセルデータポインタ。position(0,0) で一括キャプチャ:

```cpp
if (position.x() == 0 && position.y() == 0) {
    auto size = info.width() * info.height() * info.planecount() * info.cellsize();
    m_frame_buffer.resize(size);
    std::memcpy(m_frame_buffer.data(), info.m_bip, size);
}
```

### Generator (出力 matrix を生成)

入力 matrix なし。`calc_cell` 内でデコード済みデータをピクセルごとに書き込む。
outlet type は `"jit_matrix"`。

### outlet

Jitter MOP の outlet type は `"jit_matrix"`。メッセージ用 outlet は `""`:

```cpp
c74::min::outlet<> matrix_out{this, "(jit_matrix)", "jit_matrix"};
c74::min::outlet<> message_out{this, "(anything)"};
```

### 注意点

- `docs/pitfalls.md` の #13 (calc_cell テンプレート), #14 (m_bip 生ポインタ) を必ず参照
- Worker thread からの outlet 出力は `queue<>` 経由 (pitfall #2)

## トラップ・注意事項

**必ず `docs/pitfalls.md` を読むこと。** attribute の遅延設定、スレッド制限、enum_map、NIL 衝突等の重要な注意点が記載されている。
