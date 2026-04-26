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

// OK: .get() で1行変換
auto s = std::string(my_symbol_attr.get());

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

`m_maxobj` は private メンバ。パッチャー情報等の Max API 呼び出しには public メソッド `maxobj()` を使う:

```cpp
// NG: private アクセスエラー
auto obj = static_cast<c74::max::t_object*>(this->m_maxobj);

// OK: public メソッド経由 (patcher の NULL チェックを忘れずに)
auto obj = this->maxobj();
auto patcher = c74::max::object_attr_getobj(obj, c74::max::gensym("patcher"));
if (!patcher) {
    cerr << "patcher not available" << c74::min::endl;
    return {};  // MIN_FUNCTION → return {}、void関数 → return;、setter → return args;
}
c74::min::symbol filepath = c74::max::object_attr_getsym(patcher, c74::max::gensym("filepath"));
```
