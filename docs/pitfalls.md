# min-api トラップ集

Max/MSP external 開発で min-api (C74_MIN) を使う際の注意点。
新規 external 作成時やデバッグ時に必ず参照すること。

---

## 1. attribute はコンストラクタ完了後に設定される

`@bind_ip`, `@mode` 等の attribute はコンストラクタ実行時にはまだデフォルト値。
初期化処理は `m_init_timer.delay(0)` で遅延させること:

```cpp
c74::min::timer<c74::min::timer_options::defer_delivery> m_init_timer{this,
    MIN_FUNCTION {
        init();   // ここで attribute が反映済み
        return {};
    }
};

my_object() {
    m_init_timer.delay(0);
}
```

## 2. outlet への出力はメインスレッドのみ

worker thread から `output.send()` を呼んでも何も出ない。
timer callback はメインスレッドで実行されるので、定期送信には timer を使う:

```cpp
c74::min::timer<c74::min::timer_options::defer_delivery> m_timer{this,
    MIN_FUNCTION {
        // output.send() が使える
        return {};
    }
};
```

**worker thread から timer の delay() を呼んでも発火しない。**

worker thread → メインスレッドへの結果受け渡しには `c74::min::queue<>` を使う:

```cpp
c74::min::queue<> m_queue{this, MIN_FUNCTION {
    deliver_results();
    return {};
}};

// worker thread 内で:
m_queue.set();  // これならメインスレッドでコールバックが発火する
```

## 3. attribute の enum_map

int attribute を attrui でドロップダウン表示するには `enum_map` を使う:

```cpp
c74::min::attribute<int> mode{this, "mode", 0,
    c74::min::description{"Output mode."},
    c74::min::enum_map{"automatic", "bang", "update", "change", "forced"}
};
```

**`range{"a", "b"}` + `style::enum_index` は "bad number" エラーになる。`enum_map` を使うこと。**

## 4. attribute の setter callback

attribute 変更時に副作用を入れたい場合:

```cpp
c74::min::attribute<int> mode{this, "mode", 0,
    c74::min::enum_map{"automatic", "bang", "update"},
    c74::min::setter{[this](const c74::min::atoms& args, int) -> c74::min::atoms {
        // 副作用
        return args;  // 必ず args を返す。空を返すとデフォルト値になる
    }}
};
```

## 5. cout / cerr

min-api では `cout` / `cerr` はメンバ変数。グローバルではない:

```cpp
cout << "message" << c74::min::endl;  // OK (Maxコンソールに出る)
std::cout << "message" << std::endl;  // NG (Maxコンソールに出ない)
```

## 6. outlet::send API

selector + atoms を送る場合、selector を atoms の先頭に symbol として入れて `send(atoms)` を使う:

```cpp
// NG: send("selector", atoms)
// OK:
c74::min::atoms a;
a.push_back(c74::min::symbol("list"));
a.push_back(42);
output.send(a);
```

## 7. NIL マクロ衝突

Max SDK の `ext_mess.h` が `#define NIL ((void *)0)` を定義。
oscpp 等の `NIL` enum 値と衝突する場合:

```cpp
#pragma push_macro("NIL")
#undef NIL
// ... osc include ...
// pop_macro は使わない (bbb-osc 側で NIL を使わない前提)
```

## 8. クラス名・external名の命名規則

| レイヤー | 形式 | 例 |
|----------|------|-----|
| ディレクトリ/ファイル名 | `bbb.xxx.yyy` (ドット区切り) | `bbb.osc.send` |
| C++ クラス名 | `bbb_xxx_yyy` (アンダースコア区切り) | `bbb_osc_send` |
| MIN_EXTERNAL マクロ引数 | クラス名と同一 | `bbb_osc_send` |
| .mxo ファイル名 | ディレクトリ名と同一 | `bbb.osc.send.mxo` |

この変換は `bbb_add_external()` が自動処理するため、ディレクトリ名さえ正しければ `.mxo` 名は正しくなる。

## 9. attribute\<symbol\> から std::string への変換

`std::string(attr)` はコンパイルエラーになる。`c74::min::symbol` を経由すること:

```cpp
// NG: コンパイルエラー
auto s = std::string(my_symbol_attr);

// OK: .get().c_str() で曖昧さなく1行変換
auto s = std::string(my_symbol_attr.get().c_str());

// または明示的に2段階で
c74::min::symbol sym = my_symbol_attr;
auto s = std::string(sym);
```

## 10. std::filesystem は使えない

min-api の pretarget script が `CMAKE_OSX_DEPLOYMENT_TARGET` を `10.11` に設定するため、
`std::filesystem` (要 10.15) はすべて `unavailable` エラーになる。
パス操作は `c74::min::path` クラス（Max 特有のパス形式を抽象化）を優先し、
それで足りない部分を `std::string` の `find_last_of` / `substr` 等で代替すること。

## 11. Max オブジェクトへのアクセス

`m_maxobj` は private メンバ。パッチャー情報等の Max API 呼び出しには public メソッド `maxobj()` を使う。
以下は `MIN_FUNCTION` 内の例（他の文脈では return 文を適切に変えること）:

```cpp
// NG: private アクセスエラー
auto obj = static_cast<c74::max::t_object*>(this->m_maxobj);

// OK: public メソッド経由 (patcher の NULL チェックを忘れずに)
auto obj = this->maxobj();
auto patcher = c74::max::object_attr_getobj(obj, c74::max::gensym("patcher"));
if (!patcher) {
    cerr << "patcher not available" << c74::min::endl;
    return {};  // ※文脈により変えること: MIN_FUNCTION→return {}、void関数→return;、setter→return args;
}
c74::min::symbol filepath = c74::max::object_attr_getsym(patcher, c74::max::gensym("filepath"));
```

## 12. プラットフォーム依存 API と MACOS_ONLY ガード

以下の API/ヘッダは Windows に存在せず、ビルドエラーになる:

- `CommonCrypto/CommonDigest.h` → Windows: CryptoAPI または OpenSSL
- `CoreServices/CoreServices.h` → Windows: `ReadDirectoryChangesW`
- `popen()` / `pclose()` → Windows: `_popen()` / `_pclose()` (挙動が異なる)
- `<regex.h>` (POSIX regex) → Windows: C++11 `<regex>`
- `<unistd.h>` → Windows に存在しない

macOS 専用の external は `CMakeLists.txt` で `MACOS_ONLY` を指定すること:

```cmake
bbb_add_external(MACOS_ONLY)
```

クロスプラットフォーム対応の external では `#ifdef _WIN32` で実装を分けること。
`bbb_add_external()` の `MACOS_ONLY` / `WIN32_ONLY` オプションは
`cmake/bbb_external.cmake` 側でプラットフォーム判定を行い、一致しない場合は
ビルド対象から除外される。

## 13. MIN_TAGS はカンマ区切り1文字列

タグを複数指定する場合、brace-enclosed list ではなくカンマ区切りの**単一文字列**にすること:

```cpp
// NG: brace-enclosed list — コンパイルエラー
MIN_TAGS{"timecode", "ltc", "smpte", "audio"};

// OK: カンマ区切り1文字列
MIN_TAGS{"timecode, ltc, smpte, audio"};
```

内部的に `str::split(class_tags, ',')` で分割される。

## 14. sample_operator のテンプレート引数

`sample_operator` のテンプレート引数は `<input_count, output_count>`。
クラス名を渡さないこと:

```cpp
// NG: クラス名を渡す — コンパイルエラー
class my_object : public c74::min::sample_operator<my_object, 1> { ... };

// OK: 入出力数のみ
class my_object : public c74::min::sample_operator<1, 1> { ... };
```

## 15. atom からの int 取得

`atom::get<int>()` は min-api に存在しない。`static_cast<int>()` を使う:

```cpp
// NG: 存在しない
int val = c74::min::atom::get<int>(args[0]);

// OK: 暗黙変換をキャスト
int val = static_cast<int>(args[0]);
```

## 16. attribute::get() は非 const

`attribute<T>::get()` は `const` 修飾されていない。const メンバ関数内では
暗黙の `operator T()` 変換を使う:

```cpp
// NG: const メソッド内で .get() を呼ぶとコンパイルエラー
int val = fps.get();

// OK: 暗黙変換を使う
int val = static_cast<int>(fps);
// または switch 文で直接: switch (fps) { ... }
```

## 17. min-api の pretarget/posttarget とサブディレクトリ構造

min-api の `min-pretarget.cmake` / `min-posttarget.cmake` は各 external ごとに
`project()` を呼ぶことを前提としている。`bbb_add_external()` はこれらをマクロ内で
include し、ディレクトリスコープ変数を `PARENT_SCOPE` で伝播させることで
`add_subdirectory()` 構造でも動作するように対応済み。

`bbb_add_external()` を使わずに自前で CMake を書く場合、以下の設定要素を
独自に処理する必要がある:
- `C74_MIN_API_DIR` / `C74_MAX_SDK_DIR` / `C74_SUPPORT_DIR` の導出
- `CXX_STANDARD 17`
- macOS: `BUNDLE True`, `BUNDLE_EXTENSION "mxo"`, `MaxAudioAPI` / `JitterAPI` のリンク, `PkgInfo` コピー, codesign
- Windows: `SUFFIX ".mxe64"`, `MaxAPI.lib` / `MaxAudio.lib` / `jitlib.lib` のリンク, `WIN_VERSION` 定義
- 共通: `PREFIX ""`, include directories, `DC74_MIN_API` 定義

## 18. Windows: RUNTIME_OUTPUT_DIRECTORY

Windows では `.mxe64` (DLL) は `RUNTIME_OUTPUT_DIRECTORY` に出力される。
`LIBRARY_OUTPUT_DIRECTORY` だけでは不満足。両方設定すること:

```cmake
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${C74_LIBRARY_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${C74_LIBRARY_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${C74_LIBRARY_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${C74_LIBRARY_OUTPUT_DIRECTORY}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${C74_LIBRARY_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY "${C74_LIBRARY_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${C74_LIBRARY_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${C74_LIBRARY_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${C74_LIBRARY_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${C74_LIBRARY_OUTPUT_DIRECTORY}"
)
```

## 19. Jitter external のメッセージハンドラ

Jitter オブジェクト (`jit.*`) の場合、matrix 入力は `jit_matrix` メッセージで受け取る。
GL texture 入力は `jit_gl_texture` メッセージで、**texture 名 (symbol)** が渡される:

```cpp
// jit_matrix: 入力 jit.matrix の名前 (symbol) が渡される
c74::min::message<> jit_matrix{this, "jit_matrix",
    MIN_FUNCTION {
        if (args.empty()) return {};
        auto name = static_cast<c74::min::symbol>(args[0]);
        // jit.matrix からデータを取得
        return {};
    }
};

// jit_gl_texture: 内部 jit.gl.texture の名前 (symbol) が渡される
c74::min::message<> jit_gl_texture{this, "jit_gl_texture",
    MIN_FUNCTION {
        if (args.empty()) return {};
        auto name = static_cast<c74::min::symbol>(args[0]);
        // jit.gl.texture から GL texture name を取得
        return {};
    }
};
```

### jit_matrix からピクセルデータを取得

```cpp
void* mat = c74::max::jit_object_findregistered(name);
if (!mat) return;
if (c74::max::jit_object_classname(mat) != c74::max::gensym("jit_matrix")) return;

c74::max::t_jit_matrix_info info;
c74::max::jit_object_method(mat, c74::max::gensym("getinfo"), &info);
c74::max::t_atom_long savelock = (c74::max::t_atom_long)c74::max::jit_object_method(mat, c74::max::gensym("lock"), (void*)1);
char* data = (char*)c74::max::jit_object_method(mat, c74::max::gensym("getdata"));
if (data) {
    // info.dim[0], info.dim[1], info.type, info.planecount 等を参照して処理
}
c74::max::jit_object_method(mat, c74::max::gensym("lock"), (void*)savelock);
```

### jit_gl_texture から GL texture ID を取得

```cpp
void* tex = c74::max::jit_object_findregistered(name);
if (!tex) return;
uint32_t gl_name = (uint32_t)(uintptr_t)c74::max::jit_object_method(tex, c74::max::gensym("gl_texture"));
// gl_name が GLuint の GL texture name
```

## 20. help/ ディレクトリが maxhelp の正規の場所

`.maxhelp` ファイルは `help/` ディレクトリに直接配置して git で管理する。
`source/projects/` には置かない。配布パッケージでは `help/` ごとコピーする。

```
help/
├── bbb.xxx.send.maxhelp
├── bbb.xxx.receive.maxhelp
└── ...
```

## 21. 依存ライブラリを add_subdirectory で組み込む場合

静的ライブラリを external と一緒にビルドする場合、ルート `CMakeLists.txt` で
`add_subdirectory()` を呼ぶ前にテスト・例題を無効化する:

```cmake
set(MYLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MYLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/mylib)
```

依存先が `CMAKE_MSVC_RUNTIME_LIBRARY` を設定している場合、
`cmake_policy(SET CMP0091 NEW)` を `project()` より前に置くこと:

```cmake
cmake_minimum_required(VERSION 3.19)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
project(my_project)
```

## 18. Jitter matrix_operator<> の calc_cell テンプレート

`matrix_operator<>` はテンプレート引数を取らない。`vector_operator<MyClass>` のように
書かないこと:

```cpp
// OK
class my_jitter : public c74::min::object<my_jitter>, public c74::min::matrix_operator<> {};

// NG: コンパイルエラー
class my_jitter : public c74::min::object<my_jitter>, public c74::min::matrix_operator<my_jitter> {};
```

`calc_cell` は `plane_count` が 1, 4, 32 など複数の値でテンプレート実体化される。
RGBA 処理は `if constexpr` で plane_count をガードすること:

```cpp
template <class matrix_type, size_t plane_count>
c74::min::cell<matrix_type, plane_count> calc_cell(
    c74::min::cell<matrix_type, plane_count> input,
    const c74::min::matrix_info& info,
    c74::min::matrix_coord& position)
{
    if constexpr (plane_count == 4) {
        long x = position.x();
        long y = position.y();
        // e.g. return {input[0], input[1], input[2], input[3]};
        return input;
    }
    return input;
}
```

`if constexpr` を使わないと plane_count == 1 で4要素 return が型エラーになる。

## 19. Jitter の matrix_info::m_bip は生ポインタ

`m_bip` は matrix の生ピクセルデータポインタ (RGBA = `char*`, planecount=4)。
フレーム全体を一括でキャプチャする場合、`calc_cell` 内で position(0,0) の時だけ
処理し、それ以外は return input で通過させる:

```cpp
template <class matrix_type, size_t plane_count>
c74::min::cell<matrix_type, plane_count> calc_cell(
    c74::min::cell<matrix_type, plane_count> input,
    const c74::min::matrix_info& info,
    c74::min::matrix_coord& position)
{
    if (position.x() == 0 && position.y() == 0) {
        // cellsize は planecount を含む。dimstride で行ごとコピー (パディング対応)
        auto row_bytes = info.width() * info.cellsize();
        m_frame_buffer.resize(row_bytes * info.height());
        auto src = static_cast<const char*>(info.m_bip);
        auto dst = m_frame_buffer.data();
        for (long y = 0; y < static_cast<long>(info.height()); ++y) {
            std::memcpy(dst + y * row_bytes, src + y * info.dimstride[1], row_bytes);
        }
    }
    return input;
}
```

**generator mode (受信側)** では入力 matrix が不要。`calc_cell` 内でデコード済みデータを
書き込む。outlet type は `"jit_matrix"` になる。

## 20. MSVC と ULONG_PTR

Windows API の多くは `ULONG_PTR` 型の引数を取るが、`nullptr` は暗黙変換されない。
MSVC では `0` を使うこと:

```cpp
// NG (MSVC): error C2664: cannot convert argument from 'nullptr' to 'ULONG_PTR'
transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, nullptr);

// OK:
transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
```

これは Media Foundation の `IMFTransform::ProcessMessage` に限らず、
Windows API 全般 (RegisterClassEx, CreateWindowEx 等) で同様。

## 21. IMFTransform に GetOutputType は存在しない

`IMFTransform` には現在の output media type を取得する `GetOutputType` メソッドがない。
出力タイプの取得には `GetOutputAvailableType(stream_id, index, &type)` を使う:

```cpp
// NG: リンカエラー (存在しないメソッド)
hr = transform_->GetOutputType(stream_id, &output_type);

// OK: 現在設定されている output type を取得
hr = transform_->GetOutputCurrentType(stream_id, &output_type);
// OK: サポートされている type を列挙する場合
hr = transform_->GetOutputAvailableType(stream_id, index, &output_type);
```

`GetOutputAvailableType` の第二引数は type index (通常 0)。
設定済みの output type を取得したい場合は `GetOutputCurrentType` を使う。

## 22. クロスプラットフォーム .mm / .cpp ソースの CMake 分離

macOS 専用の Objective-C++ ソース (`.mm`) と Windows 専用の C++ ソース (`.cpp`) は
CMake で `if(APPLE)` / `elseif(WIN32)` で分ける:

```cmake
if(APPLE)
    target_sources(my_lib PRIVATE
        src/encoder_videotoolbox.mm
        src/decoder_videotoolbox.mm
    )
    find_library(VIDEOTOOLBOX VideoToolbox)
    target_link_libraries(my_lib PUBLIC ${VIDEOTOOLBOX} objc)
    target_compile_options(my_lib PRIVATE -fobjc-arc)
elseif(WIN32)
    target_sources(my_lib PRIVATE
        src/encoder_mf.cpp
        src/decoder_mf.cpp
    )
    target_link_libraries(my_lib PUBLIC mfplat mf mfuuid strmiids)
endif()
```

`.mm` ファイルには `-fobjc-arc` を忘れずに。

## 23. 静的ライブラリの依存スコープ分離

プラットフォーム固有のフレームワーク/ライブラリ (VideoToolbox, Media Foundation 等) を
`PUBLIC` リンクしていると、そのライブラリに依存する全ての external に伝播する。
audio-only external が不要な video フレームワークをリンクしてしまうのを防ぐため、
依存スコープごとに静的ライブラリを分ける:

```cmake
# audio-only core (all externals link this)
add_library(bbb_core STATIC
    src/session.cpp
    src/codec.cpp
)
target_link_libraries(bbb_core PUBLIC datachannel)

# video extension (only video externals link this)
add_library(bbb_video STATIC
    src/video_encoder.cpp
    src/video_decoder.cpp
)
target_link_libraries(bbb_video PUBLIC bbb_core)
# platform-specific libs go here, not in bbb_core
if(APPLE)
    target_link_libraries(bbb_video PUBLIC ${VIDEOTOOLBOX})
elseif(WIN32)
    target_link_libraries(bbb_video PUBLIC mfplat mf)
endif()
```

audio external: `bbb_add_external(DEPS bbb_core)`
video external: `bbb_add_external(DEPS bbb_video)`
