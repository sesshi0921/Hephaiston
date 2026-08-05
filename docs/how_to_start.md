# How to Start Hephaiston

Hephaiston Core UI をローカルでビルドして起動するための最小手順です。

## 1. 前提ツール

- CMake 3.24 以上
- C++20 対応コンパイラ
- GLFW 3
- OpenGL
- Dear ImGui Docking Branch

macOS + Homebrew の場合、GLFW は以下で導入できます。

```bash
brew install glfw
```

## 2. Dear ImGui Docking Branch の取得

`external/imgui` が存在しない場合は、以下を実行してください。

```bash
git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git external/imgui
```

## 3. ビルド

リポジトリルートで以下を実行します。

```bash
cmake -S . -B build
cmake --build build -j 8
```

## 4. 起動

```bash
./build/hephaiston
```

起動すると、グリッドのみの FBO ビューポート、空の左右パネル、上部メニューを持つ Hephaiston Core UI が表示されます。

## 5. 基本操作

- 左パネル: Core 単体では空です。将来アドオン登録時に操作 UI を表示します。
- 右パネル: Core 単体では空です。将来アドオン/設計データ登録時にヒエラルキーを表示します。
- 上部メニュー: フローティングウィンドウの表示
- メニューバー右端の `FPS: XXX`: 独立したクリック領域。クリックで max FPS を数値入力（0以下は Unlimited、デフォルト 60）
- 左下の小型スケールバー: 0.2m〜10,000,000m範囲で表示。ホバーで詳細表示、クリックで固定、`×` で閉じる
- FBO左上: 2D / 3D 表示切り替え
- 右下ギズモ: 方位/軸表示
- 2D ギズモ: 上が N、右が E
- 3D ギズモ: E/N/Z 軸表示。ギズモをドラッグして orbit 回転
- OSウィンドウタイトル: Core 単体では `Hephaiston`、アドオン有効時は `Hephaiston - {アドオン名}`
- FBO ビュー: クリック、ドラッグ、ホイール入力の取得
- 3D ビュー: 左ドラッグで orbit 回転、右/中ドラッグでパン、ホイールでドリー
- 2D ビュー: ドラッグでパン、ホイールでカーソル位置中心ズーム
- FBO ビュー上で `R`: カメラ/ズームをリセット

## 6. Core UI API（プラグイン向け）

プラグイン/DLL化前段として、`EditorRegistry` から以下を登録・制御できます。

- `menuVisibility()` で File / Edit / View / Addons / Window / Help / FPS 表示のON/OFF
- `viewportRenderSettings()` で FBO の水平グリッド、原点XYZ軸のON/OFF
- `registerMenuItem()` でメニューバー項目を追加
- `registerMainMenuPanel()` で `IMainMenuPanel` 継承UIを左メインパネルへ追加
- `registerViewportSceneLayer()` で FBO に線分データを追加
- `registerHierarchyPanel()` で `IHierarchyPanel` 継承UIを右ヒエラルキーへ追加、または `registerHierarchyProvider()` でツリー項目を追加
- `registerStatusBarItem()` で下ステータスバー項目を追加
- `registerOverlay()` でFBO上のオーバーレイを追加

### 6.1 プラグイン側からのinclude

Core API は `include/` 直下にも forwarding header を用意しています。`plugins/` 配下のCMakeターゲットは、以下のように短い名前でincludeできます。

```cpp
#include "EditorRegistry.h"
#include "ViewportRenderSettings.h"
#include "ViewportSceneLayer.h"
#include "PluginAPI.h"
```

プラグインターゲットでは、推奨として `hephaiston_plugin_api` をリンクしてください。

```cmake
add_library(my_plugin MODULE plugin.cpp)
target_link_libraries(my_plugin PRIVATE hephaiston_plugin_api)
```

### 6.2 draw-only UI インターフェース

左メインパネル、右ヒエラルキー、メニューバーから開くフローティングウィンドウは、`draw()` だけを持つ緩いインターフェースを継承して登録します。

```cpp
class MyPanel : public hephaiston::IMainMenuPanel {
public:
    void draw() override {}
};

class MyHierarchy : public hephaiston::IHierarchyPanel {
public:
    void draw() override {}
};

class MyWindow : public hephaiston::IFloatingWindow {
public:
    void draw() override {}
};
```

登録例:

```cpp
context.registry().registerMainMenuPanel("my.panel", "My Tool", std::make_unique<MyPanel>());
context.registry().registerHierarchyPanel("my.hierarchy", "My Data", std::make_unique<MyHierarchy>());
context.registry().registerFloatingWindow("my.window", "My Window", std::make_unique<MyWindow>());
```

## 7. DLL / dylib プラグイン API

Hephaiston は Core 起動時に以下のディレクトリから動的ライブラリを探索します。

- `plugins/`
- `build/plugins/`

プラグインは `IPlugin` を実装し、`HEPHAISTON_DECLARE_PLUGIN()` で C ABI の作成/破棄エントリポイントを公開します。

```cpp
#include "PluginAPI.h"

class MyPlugin final : public hephaiston::IPlugin {
public:
    hephaiston::PluginDescriptor descriptor() const override {
        return {"my.plugin", "My Plugin", "Vendor", "0.1.0", hephaiston::kPluginApiVersion};
    }

    bool onLoad(hephaiston::EditorContext& context) override {
        context.registry().registerMainMenuPanel("my.panel", "My Plugin", std::make_unique<MyPanel>());
        context.registry().viewportRenderSettings().showOriginAxes = false;
        return true;
    }

    void onUnload(hephaiston::EditorContext&) override {}
};

HEPHAISTON_DECLARE_PLUGIN(MyPlugin)
```

`plugins/CMakeLists.txt` では helper を使えます。

```cmake
hephaiston_add_plugin(my_plugin MyPlugin.cpp)
```

この helper は `MODULE` ライブラリを作成し、成果物を `build/plugins/` へ出力します。

## 8. 追加された拡張インターフェース

- `EditorContext`: Registry / Layout / Viewport / Selection / Scene への短命アクセスポイント
- `SelectionManager`: ヒエラルキーやFBO上の選択状態を共有
- `SceneRegistry`: プラグインが右ヒエラルキーへ表示するシーンオブジェクトを登録
- `IMenuBarContributor`: メニューバーへ任意のImGui UIを描画
- `IStatusBarWidget`: ステータスバーへ動的UIを描画
- `IViewportTool`: CAD系ツールモード。viewport入力とツールバー描画を受け取る
- `IEditorCommand`: `EditorContext` 付きの実行可能コマンド
- `IPropertiesPanel`: 選択項目に応じたプロパティUI
- `IContextMenuProvider`: ヒエラルキー/Viewport用コンテキストメニュー拡張
- `ISceneProvider`: 毎フレーム収集型のシーンツリー提供
- `IViewportSceneLayer`: FBOへ線分データを供給
- `IViewportOverlay`: FBO上のImGuiオーバーレイを供給

現時点のDLL APIは「同じC++コンパイラ/ランタイムでビルドするプラグイン」を前提にした C++ ABI + C entrypoint 方式です。将来的にサードパーティ配布を強く意識する段階では、`std::string` / `std::unique_ptr` / C++ virtual interface をDLL境界で渡さない、完全C ABIの `HephaistonPluginApi` テーブルへ移行する余地を残しています。
