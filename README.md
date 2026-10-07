# カードIDビューアー

C++17 / Win32 GUI。libaribb25 の `create_b_cas_card`、`init`、`get_id` で B-CAS / ACAS の20桁カードIDを取得します。

## 起動

`build/CardIDViewer.exe` を起動し、リーダーを選択して「ID取得」を押します。「コピー」で表示テキストをコピーできます。カードの交換後はもう一度「ID取得」を押してください。

通常は Windows のシステムディレクトリにある WinSCard.dll を絶対パスで読み込みます。カードリーダーとドライバー、Smart Card サービスが必要です。エラーコードの横に FormatMessageW で取得したWindowsの説明を表示します（言語はWindowsの設定に従います）。カード初期化・ID取得の失敗時も、libaribb25のエラーに加えて元のPC/SCエラーを表示します。説明が見つからないコードはその旨を表示します。

別の WinSCard.dll を使う場合は EXE と同じフォルダーに配置してから起動します。DLL が配置されている場合は、上部の選択欄で「EXE と同じ場所の WinSCard.dll」が初期選択されます。配置されていない場合は「Windows 標準 PC/SC」が初期選択されます。選択を変更するとリーダーを再検索します。標準のPC/SCも選び直せます。DLLはEXEと同じアーキテクチャで、SCardListReadersW / SCardConnectW を含むPC/SC関数を公開している必要があります。信頼できるDLLを使用してください。

IDは ARIB STD-B25 v6.7-E1 の 2.2.2.19、4.2.10、表4-52（規格書100ページ）に従い、ID識別子（上位3ビット）を10進1桁、ID本体（下位45ビット）を10進14桁、カードの応答に含まれるチェックコード（16ビット）を10進5桁として連結します。合計20桁を4桁ずつ5ブロックで表示します。チェックコードは独自計算せずカードから取得します。最初のIDはカードID、2番目以降はグループIDとして同じ形式で表示します。確認用に元の48ビットIDの12桁16進数も表示します。

取得中もUIは応答します。通信が終わるまでDLL変更とウィンドウの終了は待機します。独自DLLが通信から戻らない場合、待機が続くことがあります。

## ソースの取得

libaribb25 は `vendor/libaribb25` のGitサブモジュールとして管理し、検証したコミットに固定しています。

GitHubへの公開後は、次のように依存ソースもまとめて取得してください。

```powershell
git clone --recurse-submodules https://github.com/yyya-nico/CardIDViewer.git
cd CardIDViewer
```

通常の `git clone` で取得済みの場合は、リポジトリ内で次を実行してください。

```powershell
git submodule update --init --recursive
```

GitHubの「Download ZIP」にはサブモジュールのソースが含まれないため、Gitによる取得を推奨します。親リポジトリを更新した後も、上記の `git submodule update --init --recursive` を実行して依存ソースを指定コミットに合わせてください。

## ビルド

先にサブモジュールを取得してください。Visual Studio 2022 Community の「C++によるデスクトップ開発」をインストールし、このフォルダーで `build.bat` を実行すると x64 のEXEを生成します。別エディションを使う場合はバッチの vcvars64.bat のパスを変更してください。

CMakeでもビルドできます（Windows SDK と C/C++ コンパイラーが必要）。

```powershell
cmake -S . -B out -A x64
cmake --build out --config Release
```

32ビットのDLLを使用する場合は `-A Win32` でビルドしてください。

## 検証

`build.bat` の後に `test.bat` を実行します。模擬PC/SC DLLでリーダー列挙、複数IDの表示、存在しないリーダーのエラー、標準DLLへの切り替えを確認します。模擬DLLは `build/test` に隔離しています。実カード・実リーダーでの検証は未実施です。

## 構成とライセンス

- `src/main.cpp`: GUI、DLL読み込み、カードID取得
- `src/display_id.h`: 規格に沿った表示用応答の解析と20桁整形
- `src/pcsc_bridge.h`: libaribb25 の PC/SC 呼び出しを選択したDLLへ接続
- `vendor/libaribb25`: [libaribb25](https://github.com/tsukumijima/libaribb25)（Gitサブモジュール）
    - 固定コミット: `dc1d96a90ea554d8997b238fd6712eccf553cdb3`
    - ライセンス: Apache-2.0

libaribb25 のカード通信部分のみを静的に組み込みます。上流ソースは変更していません。PC/SCの応答をブリッジで読み取り、B_CAS_ID APIでは捨てられるチェックコードを保持します。

## Gitでの管理

`.gitmodules` と `vendor/libaribb25` のコミット参照は、親リポジトリに含めてコミットします。libaribb25のソース本体は上流リポジトリから取得されます。

`build/`、`out/`、`tmp/`、`.vs/` は `.gitignore` で除外しています。通常の `git add` では、EXE・DLLなどのビルド成果物や、規格書の抽出テキストなどの作業ファイルは追加されません。
