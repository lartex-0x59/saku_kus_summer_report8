# analyze_audio 使用マニュアル

`analyze_audio.cpp` のコンパイル方法、実行時オプション、gnuplotによる複数のグラフ表示方法、
そして「ピアノの音と数学」の関係についてまとめる。

現在のプログラムは、DFTのサンプル数 **N=4096** と **N=16384** の両方で同じ区間を解析し、
周波数分解能の違い・計算時間の違いを比較できるようになっている。

**【今回の変更点】**
- **gnuplotの文字化け対策**: gnuplotスクリプト内のタイトル・軸ラベル・凡例をすべて英語(ASCII)化した（Windows環境のgnuplotは日本語フォント未設定だと文字化けしやすいため）。
- **録音一覧CSVによる対象音の自動判定**: `List_of_recordings.csv` を読み込み、WAVファイル名の数字（例: `voice_9.wav`→9）を「file number」列と照合して対応する音（単音・シャープ・和音）を自動特定するようにした。これに伴い、以前は `voice_2_plot_spectrum_C4_comparison.gnu` のようにC4固定だったファイル名も、実際の音名に基づく名前（例: `voice_10_plot_spectrum_compare_C4-E4-G4.gnu`）に変更した。


---

## 1. コンパイル

### 1.1 基本コマンド（MSYS2 UCRT64）

```bash
g++ -std=c++17 -O2 analyze_audio.cpp -o analyze_audio -lsndfile
```

| オプション | 意味 |
|---|---|
| `-std=c++17` | C++17規格でコンパイル |
| `-O2` | 最適化レベル2（DFTがO(N²)なので効果が大きい。特にN=16384で顕著） |
| `-o analyze_audio` | 出力する実行ファイル名 |
| `-lsndfile` | libsndfileをリンク |

### 1.2 libsndfileが見つからない場合

```
fatal error: sndfile.h: No such file or directory
```

MSYS2 UCRT64端末で以下を実行してインストールする。

```bash
pacman -S mingw-w64-ucrt-x86_64-libsndfile
```

リンク時に `undefined reference to sf_open` 等が出る場合は、`-lsndfile` の位置をソースファイルより**後ろ**に置く（リンカはリスト順に依存関係を解決するため）。

```bash
# NG（リンクエラーになりやすい）
g++ -std=c++17 -O2 -lsndfile analyze_audio.cpp -o analyze_audio

# OK
g++ -std=c++17 -O2 analyze_audio.cpp -o analyze_audio -lsndfile
```

### 1.3 デバッグ用ビルド（任意）

```bash
g++ -std=c++17 -O0 -g -Wall -Wextra analyze_audio.cpp -o analyze_audio_debug -lsndfile
```

N=16384のDFTはO(N²)計算のため`-O0`（最適化なし）だとかなり遅くなる点に注意。デバッグ時は解析区間を短くするか、`-O2`のまま`-g`を付けるとよい。

---

## 2. 実行ファイルの実行方法とオプション

### 2.1 コマンド書式

```
./analyze_audio <wavファイル> [start_offset] [end_offset] [recordings_csv]
```

| 引数 | 必須 | デフォルト | 意味 |
|---|---|---|---|
| `<wavファイル>` | ○ | なし | 解析対象のWAVファイルパス |
| `start_offset` | × | `0.5` | onset(t0)からの解析開始オフセット [秒] |
| `end_offset` | × | `1.5` | onset(t0)からの解析終了オフセット [秒] |
| `recordings_csv` | × | `List_of_recordings.csv` | 録音一覧CSVのパス |

解析区間は `t0 + start_offset` ～ `t0 + end_offset` の範囲で決まり、その先頭からNサンプル（N=4096およびN=16384）がそれぞれ独立にDFTに使われる。

### 2.1b 対象音の自動判定について

プログラムは起動時に、カレントディレクトリ（または明示的に指定したパス）にある `List_of_recordings.csv` を読み込む。WAVファイル名の末尾の数字（例: `voice_9.wav` なら `9`）を、CSVの「file number」列と照合し、対応する「Recorded sound」列（例: `C4`, `C♯4`, `C4-E4-G4`）から対象音とその理論周波数を自動的に決定する。

```
conting number,file number,Recorded sound, other message
1,2,C4,,
9,10,C4-E4-G4,,
13,13,C♯4,,
```

- 単音（例: `C4`）→ 1つの理論周波数を対象とする
- 和音（例: `C4-E4-G4`）→ ハイフン区切りの各音をすべて対象とし、それぞれ独立にピーク検出・誤差計算を行う
- CSVが見つからない、該当するfile numberが無い、音名を解釈できない場合は、ソース冒頭の `FALLBACK_NOTE_NAME`（デフォルト`C4`）にフォールバックし、警告を標準エラー出力に表示する
- **フォールバック時のファイル名について**: 対象音がCSVから確定できなかった場合、`plot_spectrum_compare_*.gnu` のファイル名から音名部分を省略し、`<wav名>_plot_spectrum_compare.gnu` という名前になる。これは、確定していない（推測に過ぎない）音名をファイル名に含めると誤解を招くためである。

実行例:
```bash
# List_of_recordings.csv がカレントディレクトリにある場合（推奨）
./analyze_audio voice_2.wav

# CSVのパスを明示する場合
./analyze_audio voice_9.wav 0.5 1.5 List_of_recordings.csv
```

### 2.2 実行例

```bash
# 最低限（デフォルトのoffsetを使用）
./analyze_audio voice_2.wav

# offsetを明示的に指定
./analyze_audio voice_2.wav 0.5 1.5

# 発音直後（アタック直後）を解析したい場合
./analyze_audio voice_2.wav 0.05 1.05
```

### 2.3 出力ファイル

入力ファイル名（拡張子を除いた部分）が接頭辞として全出力ファイルに付与される。

`voice_2.wav`（CSV上でC4と判定された場合）を解析した場合:

```
voice_2_waveform.csv                        # 波形（間引きあり、表示用）
voice_2_onset_candidates.csv                # 検出されたonset候補
voice_2_spectrum_N4096.csv                  # N=4096のスペクトル
voice_2_spectrum_N16384.csv                 # N=16384のスペクトル
voice_2_analysis_comparison.txt             # N=4096とN=16384の比較結果
voice_2_plot_waveform.gnu                   # 波形表示用gnuplotスクリプト
voice_2_plot_spectrum_N4096.gnu             # N=4096スペクトル表示用
voice_2_plot_spectrum_N16384.gnu            # N=16384スペクトル表示用
voice_2_plot_spectrum_compare_C4.gnu        # C4理論値付近の拡大比較用
```

`voice_10.wav`（CSV上で和音C4-E4-G4と判定された場合）は以下のようになる。

```
voice_10_plot_spectrum_compare_C4-E4-G4.gnu   # 3音すべてに縦線を表示して拡大比較
```

`voice_13.wav`（CSV上でC♯4と判定された場合）は、ファイル名に使いにくい「♯」記号を
`s`に置き換えて生成される（gnuplot内の表示テキストでは読みやすいよう`#`表記になる）。

```
voice_13_plot_spectrum_compare_Cs4.gnu
```

**CSVから対象音を確定できなかった場合**（CSVが見つからない、file numberが該当しない、音名を解釈できない等）は、音名部分を省略した名前になる。

```
voice_2_plot_spectrum_compare.gnu
```

---

## 3. gnuplotの使い方

### 3.0 文字化けについて（対応済み）

以前のバージョンでは、gnuplotスクリプト内のタイトルや軸ラベルに日本語（例:「波形」「周波数スペクトル」）を直接書き込んでいたため、Windows環境のgnuplot（wxt/qtターミナルなど）が日本語フォントを正しく認識できず、文字化けすることがあった。

現在のバージョンでは、gnuplotスクリプトが生成するテキスト（タイトル・軸ラベル・凡例・音名ラベル）はすべて英語(ASCII文字)のみで構成されるように変更した（例:「C♯4」は`C#4`と表示）。これにより、フォント設定に依存せず文字化けを回避できる。念のため各スクリプトの先頭には`set encoding utf8`も付与している。

もし依然として文字化けが発生する場合は、gnuplotのターミナル設定（`set terminal wxt`や`set terminal qt`など）自体がWindows環境で正しく初期化されていない可能性があるため、以下を試すとよい。

```bash
gnuplot -e "set terminal windows" -persist voice_2_plot_waveform.gnu
```

### 3.1 単純な表示（生成されたスクリプトをそのまま使う）

```bash
gnuplot -persist voice_2_plot_waveform.gnu
gnuplot -persist voice_2_plot_spectrum_N4096.gnu
gnuplot -persist voice_2_plot_spectrum_N16384.gnu
gnuplot -persist voice_2_plot_spectrum_compare_C4.gnu
```

`-persist` を付けるとgnuplotのウィンドウを閉じてもプロセスが終了せず、ウィンドウが表示され続ける。

`plot_spectrum_compare_<対象音>.gnu`（例: `plot_spectrum_compare_C4.gnu`） は N=4096 と N=16384 のスペクトルを、C4理論周波数（261.625565 Hz）付近に拡大して**重ね書き**する。N=16384のほうが分解能が細かく、ピークがより鋭く・理論値に近い位置に現れることを視覚的に確認できる。

### 3.2 画像ファイル(PNG)として保存する

```bash
gnuplot -e "set terminal pngcairo size 1000,600; set output 'voice_2_N4096_vs_N16384.png'" -persist voice_2_plot_spectrum_compare_C4.gnu
```

### 3.3 N=4096とN=16384の分解能の違いを1つのウィンドウに並べて表示する（multiplot）

```gnuplot
# multiplot_N_compare.gnu
set datafile separator ','
set multiplot layout 2,1 title 'DFTサンプル数Nによる分解能の違い (voice_2.wav)'

set title 'N=4096 (Δf≈11.72Hz)'
set xlabel 'Frequency (Hz)'
set ylabel 'Magnitude'
set xrange [0:1000]
plot 'voice_2_spectrum_N4096.csv' using 1:2 with lines notitle

set title 'N=16384 (Δf≈2.93Hz)'
set xlabel 'Frequency (Hz)'
set ylabel 'Magnitude'
set xrange [0:1000]
plot 'voice_2_spectrum_N16384.csv' using 1:2 with lines notitle

unset multiplot
```

```bash
gnuplot -persist multiplot_N_compare.gnu
```

### 3.4 複数のWAVファイル（複数の音）を比較する

例えば `voice_2.wav`（C4）と `voice_5.wav`（別の音）を解析した場合、それぞれの `*_spectrum_N16384.csv` を重ね書きできる。

```bash
gnuplot -persist -e "
set datafile separator ',';
set xlabel 'Frequency (Hz)';
set ylabel 'Magnitude';
set xrange [0:1000];
set grid;
plot 'voice_2_spectrum_N16384.csv' using 1:2 with lines title 'voice_2 (C4)', \
     'voice_5_spectrum_N16384.csv' using 1:2 with lines title 'voice_5'
"
```

### 3.5 複数のWAVファイルを一括でグラフ化する（シェルループ）

```bash
for f in *_plot_spectrum_N16384.gnu; do
    name="${f%.gnu}"
    gnuplot -e "set terminal pngcairo size 1000,600; set output '${name}.png'" "$f"
done
```

### 3.6 対数スケールでスペクトルを見る

倍音構造やうなり（beat）の微小な成分まで見たい場合、Y軸を対数にすると見やすいことがある。

```gnuplot
set logscale y
set format y "10^{%L}"
plot 'voice_2_spectrum_N16384.csv' using 1:2 with lines title 'magnitude (log)'
```

### 3.7 対話モードでその場から試す

```bash
gnuplot
gnuplot> set datafile separator ','
gnuplot> plot 'voice_2_spectrum_N16384.csv' using 1:2 with lines
gnuplot> set xrange [200:300]
gnuplot> replot
gnuplot> exit
```

---

## 4. analysis_comparison.txt の読み方

`analysis_comparison.txt` には、録音環境への注意事項・CSVから判定された対象音の一覧に続き、N=4096・N=16384それぞれについて以下が記録される。

| 項目 | 内容 |
|---|---|
| `target_source` | 対象音がどこから決定されたか（例: `List_of_recordings.csv (file number 10 = "C4-E4-G4")`、CSVが無い場合は`fallback (...)`） |
| `analysis_start_time_sec` | DFTに使用した区間の開始時刻 |
| `frequency_resolution_hz` | Δf = sample_rate / N |
| `dft_calculation_time_ms` | DFT計算時間（std::chrono計測） |
| `global_peak_*` | スペクトル全周波数中の最大ピーク（雑音由来の可能性あり） |
| `matches_any_target_note` | 全体最大ピークが、いずれかの対象音の理論周波数付近ピークと同一ビンとみなせるか（`yes`/`no`） |
| `near_target_peak_frequency_hz [音名]` など | 対象音ごと（和音の場合は音の数だけ）に、理論値 ± 10 Hz の範囲で探索した最大ピークとその誤差(`error_hz`/`error_percent`/`cent_error`) |

ファイル末尾には、N=4096→N=16384の**計算時間比**（実測 vs. O(N²)から予想される理論値16倍）と、**分解能の改善比**（Δfが何倍細かくなったか）がまとめて記録される。

`matches_any_target_note = no` の場合、全体最大ピークは雑音や別の音である可能性が高い。その場合でも `spectrum_N*.csv` を目視で確認し、本当に対象音がどのあたりに現れているかを別途確認することを推奨する。

---

## 5. A4など他の音を解析する場合

**通常は何もする必要がない。** `List_of_recordings.csv` に該当するfile numberとRecorded sound（例: `A4`）の行があれば、プログラムが自動的にA4(440Hz)として解析する。

CSVを使わずに単発でテストしたい場合や、CSVに載っていないファイルを解析する場合のフォールバック値は、ソースコード冒頭付近の以下の2行で変更できる。

```cpp
constexpr const char* FALLBACK_NOTE_NAME = "C4";
constexpr double FALLBACK_FREQUENCY_HZ = 261.625565;   // 例: A4なら 440.0 または 442.0
```

音名から周波数への変換は `noteNameToFrequency()` が12平均律の定義に基づいて自動計算するため、`"D4"`や`"G♯4"`などCSVに記載された任意の音名にそのまま対応できる。

---

## 6. 補足: onset_candidates.csv の確認

自動検出したonset位置（発音開始位置）が意図と異なる場合は、`voice_2_onset_candidates.csv` と `voice_2_waveform.csv` をgnuplotで重ねて表示し、目視で確認するとよい。

```gnuplot
set datafile separator ','
set xlabel '時間 [秒]'
set ylabel '振幅'
plot 'voice_2_waveform.csv' using 1:2 with lines title 'waveform', \
     'voice_2_onset_candidates.csv' using 1:(0) with points pt 7 ps 2 lc rgb 'red' title 'onset candidates'
```

---

## 7. ピアノの音と数学の関係性

この自由研究の背景にある数学的なアイデアを簡単に整理する。

### 7.1 音は「単一の周波数」ではなく「周波数の重ね合わせ」

ピアノや電子オルガンが「C4（ド）」の音を出すとき、その波形は261.6Hzちょうどの正弦波1本ではなく、
基音（261.6Hz）とその整数倍の周波数を持つ**倍音**（523.25Hz、784.88Hz、…）が重なり合ってできている。

このような「複雑な周期信号を単純な正弦波・余弦波の足し算に分解する」という考え方はフーリエ級数・フーリエ変換の核心であり、DFTは有限個のサンプルからこの分解を数値的に行う手法である。
プログラム内のDFTは
```
X[k] = Σ x[n] exp(-2πikn/N)
```
という式で、信号 x[n] の中に周波数 k·(fs/N) の成分がどれだけ含まれているかを、複素数の相関（内積のようなもの）として計算している。

### 7.2 12平均律は「対数」でできている

ピアノの鍵盤は、隣り合う鍵（半音）の周波数比が常に2^(1/12)になるように設計されている（12平均律）。
これは「1オクターブ＝周波数2倍」を12等分した結果であり、人間の音程知覚が**周波数の比**（＝対数）に対して直線的に感じられることに対応している。

このプログラムで使っている「セント」という単位
```
cent = 1200 * log2(f2 / f1)
```
はこの対数的な感覚を数値化したもので、「Hzでの差」ではなく「音程としてどれだけズレているか」を表す。1セントは半音の1/100であり、人間が識別できる音程差のおおよその下限に近い。

### 7.3 周波数分解能とサンプル数のトレードオフ

DFTの周波数分解能は
```
Δf = sample_rate / N
```
で決まり、Nを大きくするほど細かい周波数まで区別できるようになる。
今回N=4096（Δf≈11.7Hz）とN=16384（Δf≈2.93Hz）を比較したのは、この「Nを大きくすると分解能が上がる」という関係を実際に確認するためである。

一方でNを大きくすると、解析に使う波形の時間幅も長くなる（N=4096なら約0.085秒分、N=16384なら約0.34秒分）。これは信号処理における「時間分解能と周波数分解能はトレードオフの関係にある」という一般的な性質（不確定性原理に類似した考え方）の具体例になっている。短い時間の中の変化を細かく見たいのか、周波数を精密に知りたいのかによって、適切なNは変わってくる。

### 7.4 計算量とFFTの意義

このプログラムの自作DFTは定義式をそのまま計算するため、計算量はおよそO(N²)である。
N=4096からN=16384へ（4倍）にすると、計算時間はおよそ16倍（4²倍）に増える——これは実際の計測結果（本プログラムの`analysis_comparison.txt`）でも確認できる。

音楽・音響分野で実用的に使われる高速フーリエ変換（FFT）は、この計算をO(N log N)まで削減する。N=16384なら、DFTの約2億6800万回の演算に対し、FFTは約23万回程度で済む計算になり、Nが大きくなるほどこの差は劇的に広がる。今回は「まず定義通りのDFTで仕組みを理解する」ことを優先しているが、この計算量の違いこそが、実用ソフトウェアがほぼ例外なくFFTを採用している理由である。

### 7.5 うなり（beat）と平均律の「ズレ」

電子オルガンやピアノの実際の音は、理論値ぴったりの周波数にはならないことが多い。これは楽器の物理的な特性（弦の張力や、電子オルガンの場合は発振・量子化の誤差など）や、調律のわずかなズレによるものである。
2つの音が近い周波数（例えば261.6Hzと263.6Hz）で同時に鳴ると、その周波数差（この場合2Hz）に応じてゆっくりと音量が周期的に強弱する「うなり」という現象が起きる。これは
```
sin(2π f1 t) + sin(2π f2 t) = 2 cos(2π (f1-f2)/2 · t) · sin(2π (f1+f2)/2 · t)
```
という三角関数の和積公式で説明できる現象であり、和音を解析する際にスペクトル上で近接した複数のピークとして観測されることがある。今回のように「全体最大ピーク」と「理論周波数付近のピーク」を区別しているのも、こうした複数の音・雑音が混在する可能性を踏まえた設計である。