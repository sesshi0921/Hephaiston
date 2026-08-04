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
├── include/hephaiston/
│   ├── Application.h
│   ├── EditorRegistry.h
│   ├── EditorShell.h
│   ├── EditorTypes.h
│   ├── Framebuffer.h
│   └── ViewportRenderer.h
└── src/
    ├── Application.cpp
    ├── EditorRegistry.cpp
    ├── EditorShell.cpp
    ├── Framebuffer.cpp
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

## 今後 DLL プラグイン化する場合の拡張案

次の段階では、Core とプラグインの ABI 境界を明確化します。

- `IEditorPanel` などの C++ インターフェイスを直接 DLL 境界で渡さず、C ABI の登録関数を用意する
- 例: `extern "C" bool hephaiston_register_plugin(HephaistonPluginApi* api);`
- `HephaistonPluginApi` 経由で panel / window / command / overlay を登録する
- プラグインごとの lifetime を `PluginHandle` で管理する
- DLL unload 前に登録 UI とコマンドを安全に unregister する
- プラグイン manifest に ID / 表示名 / version / 対応 Core API version を持たせる
- 将来的な対象:
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
