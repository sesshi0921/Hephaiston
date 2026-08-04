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
