# Hephaiston

Hephaiston（ヘパイストン）は、建築設計ワークフローをプラグインで拡張していくことを前提にした、C++20 / Dear ImGui ベースのデスクトップ型エディタです。Core UI とDLL/dylib形式の `Atlas Polys` プラグインを実装しています。

## 現在の実装範囲

- Dear ImGui Docking Branch + GLFW + OpenGL の CMake プロジェクト
- AutoCAD / Unity / Unreal Editor 風のエディタ UI シェル
- 上部メニューバー（File / Edit / View / Plugins / Window / Help）
- FBO を使った常時背景のメインビューポート
- 折りたたみ・幅変更が可能な左サイドパネル
- 折りたたみ・幅変更が可能な右ヒエラルキーパネル
- 上部メニューから開ける複数のフローティングウィンドウ
- プラグイン登録時に左パネル UI を表示できるレジストリ構造
- ヒエラルキー登録を見据えた右パネル枠
- FBO 上のクリック / ドラッグ / ホイール入力取得
- 3D orbit カメラ操作と 2D CAD カメラ操作
- 2D / 3D 表示モード切り替え
- 可視 FBO 領域へ追従する左下マップ風スケールバー・右下ビューギズモ
- 実行中レイアウト状態の保持と簡易設定ファイル保存
- メニューバー右端の FPS 表示と max FPS 選択（デフォルト 60 FPS）
- OSウィンドウタイトルは Core 単体では `Hephaiston`、プラグイン有効時は `Hephaiston - {プラグイン名}`
- 2D ギズモは N/E 方位矢印、3D ギズモはカメラに連動する E/N/Z 軸表示とドラッグ回転
- `Atlas Polys` DLLプラグイン: 地球儀フォールバック、平面地図、敷地ポリゴン、KML/CSV出力

## ドキュメント

- [How to Start](docs/how_to_start.md) - ビルドから起動までの最小手順

## ビルド方法

### 前提

- CMake 3.24 以上
- C++20 対応コンパイラ
- GLFW 3
- OpenGL
- spdlog（`external/cpp-logger` の依存）

Dear ImGui Docking Branch は、固定リビジョンを CMake `FetchContent` で
`build/_deps/hephaiston_imgui-src/` へ取得します。Earth の低ズーム背景テクスチャも、
初回 configure 時に NASA の公式 Blue Marble を
`build/assets/nasa_bluemarble_2048.png` へ取得します。どちらもリポジトリ管理外です。

macOS + Homebrew の場合、GLFW は以下で導入できます。

```bash
brew install glfw spdlog
```

### ビルド

```bash
cmake -S . -B build
cmake --build build -j 8
```

Coreとプラグインは別ビルド単位です。通常のビルドはCoreのみを生成します。組み込みのAtlas Polys DLL/dylibを生成するには、明示的にプラグインターゲットを指定します。

```bash
cmake --build build --target atlas_polys -j 8
```

一括ビルドが必要なCIなどでは、構成時に `-DHEPHAISTON_BUILD_BUNDLED_PLUGINS=ON` を指定してください。

### 実行

```bash
./build/hephaiston
```

起動すると、中央に FBO ビューポート、左にプラグイン操作パネル、右にヒエラルキー、上部にメニューバーを持つ空のエディタ UI が表示されます。

## ディレクトリ構成

```text
.
├── CMakeLists.txt
├── cmake/
│   └── RuntimeAssets.cmake      # リポジトリ外へ取得するランタイム画像
├── README.md
├── external/
│   └── cpp-logger/             # Git submodule: Core共有の非同期ログ実装
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
│   └── atlas_polys/            # 地理敷地ポリゴン機能（DLL/dylib）
├── modules/                    # Plugin非依存の再利用可能な低レベル機能
│   ├── earth/                  # 球体地球、ピッキング、地球上ライン
│   ├── planar_map/             # Web Mercator、地図カメラ、タイルソース記述子
│   ├── geospatial/             # WGS84/JGD2011相当、日本平面直角座標系 I〜XIX
│   ├── geometry/               # 自己交差・面積・CCW正規化
│   └── native_dialog/          # OSネイティブのフォルダ選択
├── tests/                      # モジュール単体テスト
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
- `drawLeftPanel()` でプラグイン操作 UI を描画
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

## プラグイン登録の仕組み

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

現段階では Core 起動時にサンプルプラグインは登録していません。左パネルと右パネルは、将来プラグインから UI やヒエラルキーが登録されるまで空のホストとして表示されます。

## Atlas Polys プラグイン

`cmake --build build --target atlas_polys` 実行後、`build/plugins/atlas_polys.so`（環境により拡張子は異なります）がPlugin Galleryの候補として表示されます。起動時にはプラグインDLLを一切自動ロードせず、最初にPlugin Galleryを開いてカードから明示的にロードします。閉じた後も `Plugins > Plugin Gallery...` から再表示できます。プラグインはCore所有のFBOに `IViewportSceneLayer` として描画し、別のGLFWウィンドウは作成しません。

- **Earth**: CMake が `build/assets/` へ取得するNASA Blue Marble（2048×1024）を低ズーム背景として球体へ貼り付けます。近接時は、画面範囲と縮尺から選んだ国土地理院XYZタイルを曲面パッチとして重ね、`.hephaiston_earth_tile_cache/` へ保存します。Planar Mapで選んだ標準地図・航空写真・色別標高図のレイヤー設定はEarthビューでも保持され、ズームアウト後に再接近しても同じレイヤーで復帰します。タイルのHTTP取得はバックグラウンドスレッドで行い、現在の縮尺に加えて前後2段階の縮尺を先読みします。Core orbitカメラと球面レイ交差ピッキングを使い、緯線経線は重ねて描画します。
- **Planar Map**: Web Mercatorのパン・ズーム・ピッキングを提供します。国土地理院の色別標高図（初期表示・ラベルなし）、標準地図、航空写真のXYZタイルを現在の縮尺に対応するズームレベルでバックグラウンド取得し、`.hephaiston_tile_cache/` に保存します。`gsi_photo` 選択時は、現在の表示タイルに加え前後2段階のズームレベルを先読みし、既定で標準地図から地名・道路線を半透明オーバーレイ表示します（左パネルの `Place names and roads` で切替可能）。新しい縮尺のタイルが未到着なら、キャッシュ済みの親タイルを地理範囲に合わせて切り出して表示するため、表示領域が黒く欠けません。通信不能時は緯度経度グリッドへ安全にフォールバックします。
- **Earth↔Planar遷移**: EarthではBlue Marble背景へ曲面の高解像度国土地理院タイルを縮尺に応じて重ねます。地表までの距離が約50 km以下になるとPlanar Mapへ、Planar Mapのズームレベルが5以下になるとEarthへ自動遷移します。どちらも中心座標とFOVから換算した縮尺を引き継ぎます。Earthモードでは縮尺バーを表示しません。左パネルのボタンで明示的に切り替えることもできます。
- **Earth回転範囲**: Atlas PolysのEarthビューは日本の地理範囲（本土・離島を含む）へ注視位置を制限します。回転・3Dギズモ操作・Planar Mapからの復帰後も、この範囲の外側へ焦点を移動できません。
- **Polygon Mode**: 左クリックで頂点追加、Backspace/Ctrl+Zで直前点削除、Enterまたは右クリックで確定、Escでキャンセルです。モード中はCoreのパン・ズーム・orbit・ギズモをロックします。
- **検証**: 日本平面直角座標系への投影後、重複点、短辺、追加辺の自己交差、閉じ辺の交差、面積ゼロを拒否します。確定時、外周を反時計回りへ正規化します。
- **Export**: KMLは常にEPSG:4326の閉じた外周リング、CSVはEPSG:4326またはJGD2011日本平面直角座標系I〜XIX（EPSG:6669〜6687）で出力します。最後のフォルダと系番号は `hephaiston_atlas_polys.ini` に保存します。

`modules/` はPluginやCore UIを参照せず、`plugins/atlas_polys/` のみがCore Plugin APIと各moduleを組み合わせます。現在のDLL境界は同一コンパイラ/標準ライブラリを前提とするC++ ABIです。外部配布用の長期ABIには、次段階でC ABIの関数テーブルへ移行してください。

### 外部依存・地表テクスチャとライセンス

- Dear ImGui Docking Branch: CMake `FetchContent` が `build/_deps/hephaiston_imgui-src/` に固定リビジョンを取得します。オフライン環境では `-DFETCHCONTENT_SOURCE_DIR_HEPHAISTON_IMGUI=/absolute/path/to/imgui` を指定できます。
- NASA Blue Marble: CMake が `build/assets/nasa_bluemarble_2048.png` に取得します。クレジットは NASA/GSFC SVS および NASA Earth Observatory / Reto Stockli です。オフラインで再構成する場合は、既存ファイルを残すか `-DHEPHAISTON_EARTH_TEXTURE_PATH=/absolute/path/to/bluemarble.png` を指定してください。画像が利用できない場合も、Earthビューは緯線経線グリッドへフォールバックします。
- `external/stb/stb_image.h`: stb single-file image loader（public domain または MIT License）。PNGのデコードだけに使用します。
- libcurl: XYZタイルのHTTP取得に使用します（curl license）。

## テスト

```bash
ctest --test-dir build --output-on-failure
```

`geo_module_tests` は三角形・四角形、CCW正規化、自己交差/短辺/重複/面積ゼロ拒否、日本平面直角座標系の往復変換とI〜XIXのEPSG表、Web Mercator中心ピッキングを検証します。


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

`ViewportRenderSettings::showViewModeToggle` を `false` にすると、CoreがFBO左上に表示する2D/3D切替ボタンを非表示にできます。ビュー種別をPluginが管理する場合に使用します。

`ViewportNavigationSettings` の表示上の `1.00x` は、従来の `0.75x` の操作応答です。2D・3D・Earthビューで共通の有効感度を適用します。3D orbitは近距離ほど自動的に感度を下げ、Earthビューは画面上の地表meters-per-pixelを基準にドラッグ量を換算するため、近接時も精密に操作できます。

View → `Trackpad Zoom` では、トラックパッドのズーム入力を `Two-finger Vertical Scroll`（既定）または `Pinch In / Out` から選べます。macOSではネイティブのMagnifyイベントを使用します。Atlas Polysプラグインは読み込み時に `Pinch In / Out` を選択します。

Pluginsメニューからは、DLL/dylib/shared libraryの絶対・相対パスを入力して動的に読み込みできます。既知の `plugins/` と `build/plugins/` は再スキャン可能で、`Unload All Loaded Plugins` はプラグインが登録したUI・描画・入力拡張を破棄してからライブラリを安全に解放します。

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
- Core 単体ではサンプルプラグインを表示しません。プラグイン登録後に左パネルへ操作 UI を表示する想定です。
- メニューバー右端の `FPS: XXX` は独立したクリック領域として表示され、max FPS を数値入力できます。0以下は Unlimited 表示/扱いです。デフォルトは 60 FPS です。
- `Window` メニューから Project Settings / Plugin Manager / Console などを複数開けます。
- 2D/3D切替ボタンはFBO左上に表示されます。
- 2D ギズモは常に上方向が `N`、右方向が `E` です。
- 右下ギズモは小型・枠なし表示です。3D時はワールド +X=`E`、+Y=`N`、+Z=`Z` としてカメラ回転に追従し、ギズモ部分のドラッグでも orbit 回転できます。
- 左下スケールバーは小型のマップアプリ風表示です。スケールバー距離は0.2m〜10,000,000mの範囲で丸め表示します。ホバー中に詳細情報を表示し、クリックすると `×` で閉じるまで固定表示できます。

## ログ

[`sesshi0921/cpp-logger`](https://github.com/sesshi0921/cpp-logger) を `external/cpp-logger` のGitサブモジュールとして組み込み、Coreが単一の非同期ロガーを所有します。開発ビルドでの出力先は `build/logs/cpp_logger.log` です（最大16個の1 MiBローテーションファイル）。

- Core: エディタ初期化・終了、プラグイン探索、DLLロード/アンロード、ABI不整合・ロード失敗を記録
- Atlas Polys: ビュー/レイヤー切替、Earth↔Planar遷移、ポリゴン編集の受理・拒否、設定保存を記録
- 失敗を握り潰す代わりに、外部入力・I/O・プラグイン境界で `warning` / `error` / `critical` を記録します。通常のフレーム描画はログを汚染しないよう記録しません。

DLLプラグインは `cpp_logger` を個別リンクせず、必ずCore共有のロガーを `EditorContext` 経由で使います。

```cpp
#include "PluginAPI.h"

bool MyPlugin::onLoad(hephaiston::EditorContext& context) {
    context.logger().info("[MyPlugin] initialized");
    context.logger().warning("[MyPlugin] optional data source is unavailable");
    return true;
}
```
