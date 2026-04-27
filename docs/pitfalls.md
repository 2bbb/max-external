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
    RUNTIME_OUTPUT_DIRECTORY "${C74_LIBRARY_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${C74_LIBRARY_OUTPUT_DIRECTORY}"
)
```
