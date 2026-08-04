# Hephaiston

Hephaiston（ヘパイストン）は、建築設計ワークフローをプラグインで拡張していくことを前提にした、C++20 / Dear ImGui ベースのデスクトップ型エディタです。現在は Core UI シェルのみを実装しています。

## 現在の実装範囲

- Dear ImGui Docking Branch + GLFW + OpenGL の CMake プロジェクト
- AutoCAD / Unity / Unreal Editor 風のエディタ UI シェル
- 上部メニューバー（File / Edit / View / Addons / Window / Help）
- FBO を使った常時背景のメインビューポート
- 折りたたみ・幅変更が可能な左サイドパネル
- 折りたたみ・幅変更が可能な右ヒエラルキーパネル
- 上部メニューから開ける複数のフローティングウィンドウ
- アドオン登録時に左パネル UI を表示できるレジストリ構造
- ヒエラルキー登録を見据えた右パネル枠
- FBO 上のクリック / ドラッグ / ホイール入力取得
- 3D orbit カメラ操作と 2D CAD カメラ操作
- 2D / 3D 表示モード切り替え
- 可視 FBO 領域へ追従する左下マップ風スケールバー・右下ビューギズモ
- 実行中レイアウト状態の保持と簡易設定ファイル保存
- メニューバー右端の FPS 表示と max FPS 選択（デフォルト 60 FPS）
- OSウィンドウタイトルは Core 単体では `Hephaiston`、アドオン有効時は `Hephaiston - {アドオン名}`
- 2D ギズモは N/E 方位矢印、3D ギズモはカメラに連動する E/N/Z 軸表示とドラッグ回転

## ドキュメント

- [How to Start](docs/how_to_start.md) - ビルドから起動までの最小手順

## ビルド方法

### 前提

- CMake 3.24 以上
- C++20 対応コンパイラ
- GLFW 3
- OpenGL
- Dear ImGui Docking Branch

このリポジトリでは Dear ImGui Docking Branch を `external/imgui` に配置する構成です。
未配置の場合は以下で取得してください。

```bash
git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git external/imgui
```

macOS + Homebrew の場合、GLFW は以下で導入できます。

```bash
brew install glfw
```

### ビルド

```bash
cmake -S . -B build
cmake --build build -j 8
```

### 実行

```bash
./build/hephaiston
```

起動すると、中央に FBO ビューポート、左にアドオン操作パネル、右にヒエラルキー、上部にメニューバーを持つ空のエディタ UI が表示されます。

## ディレクトリ構成

```text
.
├── CMakeLists.txt
├── README.md
├── external/
│   └── imgui/                  # Dear ImGui Docking Branch
├── include/                    # プラグイン向け短縮 forwarding headers
├── include/hephaiston/
│   ├── Application.h
│   ├── EditorContext.h
│   ├── EditorRegistry.h
│   ├── EditorShell.h
│   ├── EditorTypes.h
│   ├── Framebuffer.h
│   ├── Plugin.h
│   ├── PluginManager.h
│   ├── SceneRegistry.h
│   ├── SelectionManager.h
│   └── ViewportRenderer.h
├── plugins/                    # DLL/dylib プラグイン置き場 / CMakeサブディレクトリ
└── src/
    ├── Application.cpp
    ├── EditorRegistry.cpp
    ├── EditorShell.cpp
    ├── Framebuffer.cpp
    ├── PluginManager.cpp
    ├── ViewportRenderer.cpp
    └── main.cpp
```

## UI 構成

`EditorShell` が Core UI 全体を統括します。

- `drawMainMenu()` で上部メニューを描画
- `drawViewportBackground()` で FBO テクスチャをエディタ背景として描画
- `drawLeftPanel()` でアドオン操作 UI を描画
- `drawRightPanel()` でヒエラルキーを描画
- `drawFloatingWindows()` でメニューから開くウィンドウを描画
- `drawOverlays()` でビューポート専用オーバーレイを描画
- `drawStatusBar()` で下部ステータスバーを描画

左右パネルの占有幅とステータスバーを考慮して、毎フレーム `ViewportVisibleRect` を算出します。左下のスケール表示と右下のビューギズモは、アプリ全体の角ではなく、この可視 FBO 領域の角へ配置されます。

## FBO の描画構造

OpenGL リソースは `Framebuffer` と `ViewportRenderer` に分離しています。

- `Framebuffer`
  - color texture
  - depth/stencil renderbuffer
  - framebuffer object
  - RAII による破棄
  - コピー禁止・ムーブ対応
- `ViewportRenderer`
  - FBO サイズ変更
  - グリッドと最小限の 3D 軸表示
  - 2D / 3D モード切り替え

描画の流れは以下です。

1. ImGui のフレーム開始後に表示サイズを取得
2. メニューバー・ステータスバーを除いた FBO サイズを計算
3. FBO を必要に応じてリサイズ
4. FBO にグリッド等を OpenGL で描画
5. ImGui の背景ウィンドウに FBO texture を `ImGui::Image` として表示
6. 左右パネル、オーバーレイ、フローティングウィンドウを上に重ねる

## アドオン登録の仕組み

将来のプラグイン化を見据え、Core には以下のインターフェイスを用意しています。

```cpp
class IEditorPanel {
public:
    virtual ~IEditorPanel() = default;
    virtual const char* id() const = 0;
    virtual const char* displayName() const = 0;
    virtual void draw() = 0;
};
```

同様に、以下も登録できます。

- `IEditorWindow` : フローティングウィンドウ
- `IViewportOverlay` : FBO 上のオーバーレイ
- `EditorCommand` : コマンド

登録先は `EditorRegistry` です。

```cpp
class EditorRegistry {
public:
    void registerPanel(std::unique_ptr<IEditorPanel> panel);
    void registerWindow(std::unique_ptr<IEditorWindow> window);
    void registerOverlay(std::unique_ptr<IViewportOverlay> overlay);
    void registerCommand(EditorCommand command);
};
```

現段階では Core 起動時にサンプルアドオンは登録していません。左パネルと右パネルは、将来アドオン/プラグインから UI やヒエラルキーが登録されるまで空のホストとして表示されます。


## Core UI API（プラグイン向け）

`EditorRegistry` は、将来のDLLプラグインから各UI領域を埋めるためのAPI境界として使います。

### 表示制御

```cpp
auto& menu = registry.menuVisibility();
menu.showFile = true;
menu.showEdit = false;
menu.showFpsControl = true;

auto& viewport = registry.viewportRenderSettings();
viewport.showHorizontalGrid = true;
viewport.showOriginAxes = false;
```

### プラグイン側からのinclude

`include/` 直下に forwarding header を用意しているため、`plugins/` 配下のソースでは短い名前でCore APIをincludeできます。

```cpp
#include "EditorRegistry.h"
#include "ViewportRenderSettings.h"
#include "ViewportSceneLayer.h"
#include "PluginAPI.h"
```

CMakeでは `hephaiston_plugin_api` interface target を提供しています。

```cmake
add_library(my_plugin MODULE plugin.cpp)
target_link_libraries(my_plugin PRIVATE hephaiston_plugin_api)
```

`plugins/CMakeLists.txt` は存在する場合に自動で `add_subdirectory(plugins)` されます。

### draw-only UI インターフェース

プラグインが直接UI内容を描画する領域は、かなり緩い `draw()` だけのインターフェースを使います。これらを継承して登録しない限り、該当領域には表示されません。

```cpp
class MyPanel : public IMainMenuPanel {
public:
    void draw() override { /* left panel UI */ }
};

class MyHierarchy : public IHierarchyPanel {
public:
    void draw() override { /* right hierarchy UI */ }
};

class MyWindow : public IFloatingWindow {
public:
    void draw() override { /* floating window UI */ }
};
```

登録時にIDや表示名を渡します。

```cpp
registry.registerMainMenuPanel("my.panel", "My Panel", std::make_unique<MyPanel>());
registry.registerHierarchyPanel("my.hierarchy", "My Hierarchy", std::make_unique<MyHierarchy>());
registry.registerFloatingWindow("my.window", "My Window", std::make_unique<MyWindow>());
```

### 登録できる領域

- メニューバー: `registerMenuItem(EditorMenuItem)` / `registerMenuBarContributor()`
- 左メインパネル: `registerMainMenuPanel(id, name, std::unique_ptr<IMainMenuPanel>)`
- FBO表示データ: `registerViewportSceneLayer(std::unique_ptr<IViewportSceneLayer>)`
- FBOツール: `registerViewportTool(std::unique_ptr<IViewportTool>)`
- FBOオーバーレイ: `registerOverlay(std::unique_ptr<IViewportOverlay>)`
- 右ヒエラルキー: `registerHierarchyPanel()` / `registerHierarchyProvider()` / `registerSceneProvider()` / `EditorContext::scene()`
- プロパティ: `registerPropertiesPanel(std::unique_ptr<IPropertiesPanel>)`
- コンテキストメニュー: `registerContextMenuProvider(std::unique_ptr<IContextMenuProvider>)`
- 下ステータスバー: `registerStatusBarItem(StatusBarItem)` / `registerStatusBarWidget()`
- フローティングウィンドウ: `registerFloatingWindow(id, title, std::unique_ptr<IFloatingWindow>)`
- コマンド: `registerCommand(EditorCommand)` / `registerCommand(std::unique_ptr<IEditorCommand>)`

### FBOへの線分追加例

```cpp
class MyLayer : public IViewportSceneLayer {
public:
    const char* id() const override { return "my.layer"; }
    const char* displayName() const override { return "My Layer"; }
    void collectViewportLines(std::vector<ViewportLine>& out) override {
        out.push_back({0, 0, 0, 10, 0, 0, ImVec4(1, 0, 0, 1)});
    }
};
```

## DLL / dylib プラグイン化

Core は `PluginManager` を持ち、起動時に `plugins/` と `build/plugins/` から `.dylib` / `.so` / `.dll` を探索します。

プラグインは `IPlugin` を実装し、`HEPHAISTON_DECLARE_PLUGIN(MyPlugin)` で C ABI のエントリポイントを公開します。

```cpp
#include "PluginAPI.h"

class MyPlugin final : public hephaiston::IPlugin {
public:
    hephaiston::PluginDescriptor descriptor() const override {
        return {"my.plugin", "My Plugin", "Vendor", "0.1.0", hephaiston::kPluginApiVersion};
    }

    bool onLoad(hephaiston::EditorContext& context) override {
        context.registry().registerMainMenuPanel("my.panel", "My Plugin", std::make_unique<MyPanel>());
        return true;
    }

    void onUnload(hephaiston::EditorContext&) override {}
};

HEPHAISTON_DECLARE_PLUGIN(MyPlugin)
```

`plugins/CMakeLists.txt` では以下の helper が使えます。成果物は `build/plugins/` に出力されます。

```cmake
hephaiston_add_plugin(my_plugin MyPlugin.cpp)
```

現段階は「同じC++コンパイラ/ランタイムでビルドするプラグイン」向けの C++ ABI + C entrypoint 方式です。サードパーティ配布を強く意識する段階では、`std::string` / `std::unique_ptr` / C++ virtual interface をDLL境界で渡さない完全C ABIテーブル方式へ移行できるよう、`PluginDescriptor::apiVersion` と `PluginManager` の境界を分けています。

将来的な対象:

- 地図・地球儀ビューア
- 建築基準法 / 用途地域 / 斜線制限 / 日影規制 DB 連携
- 建築可能ボリューム生成
- 間取り生成
- GLB / KML / LandXML / IFC エクスポート

## 操作メモ

- 左パネル上部の `<` で折りたたみ、細いタブの `>` で再展開できます。
- 右パネル上部の `>` で折りたたみ、細いタブの `<` で再展開できます。
- 左右パネル端の細い splitter をドラッグすると幅を変更できます。
- Core 単体ではサンプルアドオンを表示しません。アドオン登録後に左パネルへ操作 UI を表示する想定です。
- メニューバー右端の `FPS: XXX` は独立したクリック領域として表示され、max FPS を数値入力できます。0以下は Unlimited 表示/扱いです。デフォルトは 60 FPS です。
- `Window` メニューから Project Settings / Addon Manager / Console などを複数開けます。
- 2D/3D切替ボタンはFBO左上に表示されます。
- 2D ギズモは常に上方向が `N`、右方向が `E` です。
- 右下ギズモは小型・枠なし表示です。3D時はワールド +X=`E`、+Y=`N`、+Z=`Z` としてカメラ回転に追従し、ギズモ部分のドラッグでも orbit 回転できます。
- 左下スケールバーは小型のマップアプリ風表示です。スケールバー距離は0.2m〜10,000,000mの範囲で丸め表示します。ホバー中に詳細情報を表示し、クリックすると `×` で閉じるまで固定表示できます。
