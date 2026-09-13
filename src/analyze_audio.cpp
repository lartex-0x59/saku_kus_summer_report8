// analyze_audio.cpp
//
// 「数学で読み解くピアノの音 ― 周波数解析と平均律・うなりの数学的考察」
// 自由研究用 音声解析プログラム
//
// 機能:
//   1. WAVファイルの読み込み（libsndfile使用、ステレオ→モノラル化）
//   2. 波形のCSV出力（間引きあり／DFT用は間引かない）
//   3. 発音開始位置（onset）候補の検出
//   4. 解析区間の抽出（t0 + start_offset ～ t0 + end_offset）
//   5. 自作DFT（離散フーリエ変換）を N=4096 と N=16384 の両方で実行し、
//      周波数分解能の違いを比較できるようにする
//   6. 周波数スペクトルをNごとに別CSVで出力
//   7. 「全体最大ピーク」と「対象音の理論周波数付近のピーク」を区別して検出
//      （対象音は単音・和音（複数音）どちらにも対応）
//   8. 理論周波数との比較（誤差Hz、誤差%、セント誤差）をNごとに算出
//   9. gnuplot用スクリプトの生成（Nごとの単独表示 + 理論値付近の拡大比較）
//  10. DFT計算時間の計測（std::chrono）。N=4096とN=16384の計算時間を比較し、
//      O(N^2)の計算量からの理論的な増加倍率と実測を比較する
//
// ------------------------------------------------------------
// 【今回の変更点】
// ------------------------------------------------------------
// (a) gnuplotウィンドウ内の文字化け対策
//     Windows(MSYS2)環境のgnuplotは、日本語フォント設定なしで
//     日本語タイトル・ラベルを表示すると文字化けすることがある。
//     そのため、gnuplotスクリプト内のタイトル・軸ラベル・凡例等は
//     すべて英語(ASCII文字)のみで生成するように変更した。
//     （C++コード自体のコメントは引き続き日本語のまま）
//
// (b) 録音一覧CSV(List_of_recordings.csv)による対象音の自動判定
//     これまでは「対象音はC4固定」だったが、実際には録音ファイルごとに
//     異なる音（単音・シャープ・和音）が含まれている。
//     そこで、WAVファイル名末尾の数字（例: voice_9.wav なら 9）を
//     録音一覧CSVの「file number」列と照合し、対応する
//     「Recorded sound」列（例: "C4", "C♯4", "C4-E4-G4"）から
//     理論周波数を自動計算するようにした。
//     これに伴い、以前は "plot_spectrum_C4_comparison.gnu" のように
//     C4固定の名前だったファイルも、実際の音名に基づく名前
//     （例: voice_9_plot_spectrum_compare_C4-E4-G4.gnu）に変更した。
//
// コンパイル方法（MSYS2 UCRT64）:
//   g++ -std=c++17 -O2 analyze_audio.cpp -o analyze_audio -lsndfile
//
// 実行方法:
//   ./analyze_audio voice_2.wav
//   ./analyze_audio voice_2.wav 0.5 1.5
//   ./analyze_audio voice_2.wav 0.5 1.5 List_of_recordings.csv   (CSVパスを明示)
//
// CSVを指定しない場合、カレントディレクトリの "List_of_recordings.csv" を
// 自動的に探しに行く。見つからない場合や該当行が無い場合は、
// フォールバックとして FALLBACK_NOTE_NAME / FALLBACK_FREQUENCY_HZ を使用する。
//
// 出力ファイル名について:
//   入力WAVファイル名（拡張子除く）を接頭辞として、全出力ファイルに付与する。
//   例: voice_2.wav（CSV上でC4と判定された場合）
//     voice_2_waveform.csv
//     voice_2_onset_candidates.csv
//     voice_2_spectrum_N4096.csv
//     voice_2_spectrum_N16384.csv
//     voice_2_analysis_comparison.txt
//     voice_2_plot_waveform.gnu
//     voice_2_plot_spectrum_N4096.gnu
//     voice_2_plot_spectrum_N16384.gnu
//     voice_2_plot_spectrum_compare_C4.gnu
//
// 今回はFFTライブラリを一切使用せず、DFTの定義式を素直に実装している。
//
// 作者: (自由研究プログラム)

#include <sndfile.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// ============================================================
// フォールバック用の対象音（CSVで判定できなかった場合に使用）
// ============================================================
//
// 12平均律において、ある音の理論周波数は基準音（通常A4）からの半音数nを用いて
//     f = f_reference * 2^(n/12)
// で計算される。C4はA4から9半音下なので、
//     f(C4) = 440 * 2^(-9/12) = 261.625565... Hz
//
// 【将来A4を解析する場合】
// CSVに"A4"という行があれば自動的にそちらが使われるが、
// CSVが無い環境でテストする場合などは、以下の2行を書き換えることで
// フォールバック時の対象音を変更できる。
constexpr const char* FALLBACK_NOTE_NAME = "C4";
constexpr double FALLBACK_FREQUENCY_HZ = 261.625565;

// ============================================================
// その他の定数
// ============================================================

// 「理論周波数付近」のピークを探索する際の探索半幅 [Hz]
constexpr double PEAK_SEARCH_RANGE_HZ = 10.0;

// gnuplotで理論周波数付近を拡大表示する際のズーム余白 [Hz]
constexpr double ZOOM_MARGIN_HZ = 15.0;

// スペクトル単独表示(0~Xhz)のデフォルト上限[Hz]。
// 対象音の周波数がこれより高い場合は自動的に広げる。
constexpr double DEFAULT_SPECTRUM_XRANGE_MAX_HZ = 1000.0;

// 比較するDFTのサンプル数（N）の一覧
// N=4096  : Δf = 48000/4096  ≈ 11.71875 Hz
// N=16384 : Δf = 48000/16384 ≈  2.9296875 Hz
constexpr std::array<int, 2> DFT_SIZES = {4096, 16384};

// 録音一覧CSVのデフォルトファイル名
constexpr const char* DEFAULT_RECORDINGS_CSV = "List_of_recordings.csv";

// 円周率（std::acosを使わず明示的に定義しておく）
constexpr double PI = 3.14159265358979323846;

// ============================================================
// ユーティリティ: 文字列のトリム（前後の空白・CRを除去）
// ============================================================
std::string trim(const std::string& s) {
    size_t start = 0, end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

// ============================================================
// ユーティリティ: 入力WAVファイル名から出力ファイルの接頭辞を作る
// ============================================================
//
// 例: "voice_2.wav"          -> "voice_2"
//     "C:\rec\voice_2.wav"   -> "voice_2"
//     "./data/voice_2.WAV"   -> "voice_2"
std::string getBaseNameNoExt(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    std::string filename = (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);
    size_t last_dot = filename.find_last_of('.');
    if (last_dot != std::string::npos && last_dot != 0) {
        filename = filename.substr(0, last_dot);
    }
    return filename;
}

// ============================================================
// ユーティリティ: 文字列末尾の連続する数字を抽出する
// ============================================================
//
// 例: "voice_9" -> 9,  "rec12" -> 12,  "abc" -> std::nullopt
// これを使い、WAVファイル名（例: voice_9.wav）と
// 録音一覧CSVの「file number」列（例: 9）を対応付ける。
std::optional<int> extractTrailingNumber(const std::string& s) {
    int i = static_cast<int>(s.size()) - 1;
    std::string digits;
    while (i >= 0 && std::isdigit(static_cast<unsigned char>(s[i]))) {
        digits = s[static_cast<size_t>(i)] + digits;
        --i;
    }
    if (digits.empty()) return std::nullopt;
    try {
        return std::stoi(digits);
    } catch (...) {
        return std::nullopt;
    }
}

// ============================================================
// ユーティリティ: 音名(例 "C4", "C#4", "C♯4")を周波数[Hz]に変換する
// ============================================================
//
// 12平均律において、音名は「音名(A~G)＋任意のシャープ記号＋オクターブ番号」
// の形式で表される（例: C4, C#4, A4）。
// 半音の周波数比は 2^(1/12) であり、A4=440Hzを基準として
//     f = 440 * 2^((オクターブ差*12 + 半音差) / 12)
// で計算できる。
//
// 半音インデックス（オクターブ内の位置、Cを0とする）:
//     C=0, C#=1, D=2, D#=3, E=4, F=5, F#=6, G=7, G#=8, A=9, A#=10, B=11
// Aのインデックスが9であることに注意（A4を基準にするため）。
//
// シャープ記号は "#"（半角）と "♯"（U+266F、UTF-8で0xE2 0x99 0xAF）の
// どちらにも対応する（録音一覧CSVでは "♯" が使われている）。
std::optional<double> noteNameToFrequency(const std::string& note_raw) {
    std::string note = trim(note_raw);
    if (note.empty()) return std::nullopt;

    char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(note[0])));
    static const std::map<char, int> letter_index = {
        {'C', 0}, {'D', 2}, {'E', 4}, {'F', 5}, {'G', 7}, {'A', 9}, {'B', 11}};
    auto it = letter_index.find(letter);
    if (it == letter_index.end()) return std::nullopt;
    int idx = it->second;

    size_t pos = 1;
    bool sharp = false;
    if (pos < note.size()) {
        if (note[pos] == '#') {
            sharp = true;
            pos += 1;
        } else if (pos + 2 < note.size() && static_cast<unsigned char>(note[pos]) == 0xE2 &&
                   static_cast<unsigned char>(note[pos + 1]) == 0x99 &&
                   static_cast<unsigned char>(note[pos + 2]) == 0xAF) {
            // U+266F (MUSIC SHARP SIGN) の UTF-8 表現
            sharp = true;
            pos += 3;
        }
    }
    if (sharp) idx += 1;

    std::string octave_str = trim(note.substr(pos));
    if (octave_str.empty()) return std::nullopt;
    int octave;
    try {
        octave = std::stoi(octave_str);
    } catch (...) {
        return std::nullopt;
    }

    int semitone_from_A4 = (octave - 4) * 12 + (idx - 9);
    return 440.0 * std::pow(2.0, semitone_from_A4 / 12.0);
}

// ============================================================
// ユーティリティ: 音名をgnuplot表示用のASCII安全な文字列に変換する
// （"♯" -> "#" に置き換える。gnuplotの文字化け対策の一環）
// ============================================================
std::string sanitizeNoteAscii(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xE2 && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x99 &&
            static_cast<unsigned char>(s[i + 2]) == 0xAF) {
            out += '#';
            i += 2;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

// ============================================================
// ユーティリティ: 音名をファイル名に安全な文字列に変換する
// （"♯" -> "s" に置き換える。"#"はシェルで特殊文字になりうるため
//   ファイル名には使わずアルファベットの"s"を使う）
// ============================================================
std::string sanitizeNoteForFilename(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xE2 && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x99 &&
            static_cast<unsigned char>(s[i + 2]) == 0xAF) {
            out += 's';
            i += 2;
        } else if (c == '#') {
            out += 's';
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

// ============================================================
// データ構造
// ============================================================

// WAVファイルから読み込んだ情報一式
struct WavData {
    int sample_rate = 0;
    int channels = 0;
    sf_count_t frames = 0;
    std::vector<double> mono;
};

// 発音開始位置候補
struct OnsetCandidate {
    double time_sec;
    double change;
};

// DFT結果1点分（正の周波数成分のみ）
struct SpectrumPoint {
    double frequency_hz = 0.0;
    double magnitude = 0.0;
    double magnitude_normalized = 0.0;
};

// 理論周波数との比較結果
struct FrequencyComparison {
    double measured_frequency_hz = 0.0;
    double error_hz = 0.0;
    double error_percent = 0.0;
    double cent_error = 0.0;
};

// 対象音1つ分（単音なら1個、和音ならその音数だけ生成される）
struct TargetNote {
    std::string name;          // 例: "C4", "C#4"
    double frequency_hz = 0.0;
};

// N=4096, N=16384 それぞれについて、対象音1つ分のピーク検出＋比較結果
struct TargetNotePeakResult {
    TargetNote note;
    SpectrumPoint peak;
    FrequencyComparison comparison;
};

// N=4096, N=16384 それぞれについての解析結果一式
struct NAnalysisResult {
    int N = 0;
    double frequency_resolution_hz = 0.0;
    double window_start_sec = 0.0;
    double dft_time_ms = 0.0;
    std::vector<SpectrumPoint> spectrum;
    SpectrumPoint global_peak;
    std::vector<TargetNotePeakResult> target_peaks;  // 対象音ごとの結果（和音対応）
    bool global_peak_matches_any_target = false;
};

// ============================================================
// 録音一覧CSV(List_of_recordings.csv)の読み込み
// ============================================================
//
// 想定するCSV形式（1行目はヘッダとして読み飛ばす）:
//     conting number,file number,Recorded sound, other message
//     1,2,C4,,
//     9,10,C4-E4-G4,,
//     10,none,none,This square does not exist due to a recording error.,
//
// 「Recorded sound」列は "C4-E4-G4" のようにハイフン区切りで
// 複数音（和音）を表すことがある。この関数ではその文字列を
// そのまま file_number -> recorded_sound の対応として保持し、
// 音名への分解は呼び出し側（parseTargetNotes）で行う。
//
// CSVはダブルクォートで囲まれたフィールド内にカンマを含む場合があるため
// （例: "Due to a recording error, this is designated as No. 38."）、
// 簡易的なクォート対応CSVパーサーを実装している。
std::vector<std::string> parseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    bool in_quotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur += '"';
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                cur += c;
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                fields.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        }
    }
    fields.push_back(cur);
    return fields;
}

std::map<int, std::string> loadRecordingsCsv(const std::string& path, bool& loaded_ok) {
    std::map<int, std::string> result;
    std::ifstream ifs(path);
    if (!ifs) {
        loaded_ok = false;
        return result;
    }

    std::string line;
    bool first_line = true;
    while (std::getline(ifs, line)) {
        if (first_line) {
            first_line = false;
            continue;  // ヘッダ行を読み飛ばす
        }
        if (trim(line).empty()) continue;

        std::vector<std::string> fields = parseCsvLine(line);
        if (fields.size() < 3) continue;

        std::string file_number_str = trim(fields[1]);
        std::string recorded_sound = trim(fields[2]);

        if (file_number_str.empty() || file_number_str == "none") continue;

        try {
            int file_number = std::stoi(file_number_str);
            result[file_number] = recorded_sound;
        } catch (...) {
            continue;  // 数値に変換できない行はスキップ
        }
    }

    loaded_ok = true;
    return result;
}

// ============================================================
// 「Recorded sound」文字列（例: "C4", "C4-E4-G4"）を
// 対象音（TargetNote）のリストに変換する
// ============================================================
std::vector<TargetNote> parseTargetNotes(const std::string& recorded_sound) {
    std::vector<TargetNote> notes;
    if (recorded_sound.empty() || recorded_sound == "none") return notes;

    std::stringstream ss(recorded_sound);
    std::string token;
    while (std::getline(ss, token, '-')) {
        std::string t = trim(token);
        if (t.empty()) continue;
        auto freq = noteNameToFrequency(t);
        if (freq.has_value()) {
            notes.push_back({t, *freq});
        } else {
            std::cerr << "[警告] 音名を解釈できませんでした: \"" << t << "\" (無視します)"
                      << std::endl;
        }
    }
    return notes;
}

// ============================================================
// 1. WAVファイルの読み込み
// ============================================================
WavData readWavFile(const std::string& path) {
    SF_INFO info;
    std::memset(&info, 0, sizeof(info));

    SNDFILE* file = sf_open(path.c_str(), SFM_READ, &info);
    if (!file) {
        std::cerr << "[エラー] WAVファイルを開けませんでした: " << path << std::endl;
        std::cerr << "  詳細: " << sf_strerror(nullptr) << std::endl;
        std::exit(1);
    }

    WavData data;
    data.sample_rate = info.samplerate;
    data.channels = info.channels;
    data.frames = info.frames;

    std::cout << "==== WAVファイル情報 ====" << std::endl;
    std::cout << "ファイル名        : " << path << std::endl;
    std::cout << "サンプルレート     : " << data.sample_rate << " Hz" << std::endl;
    std::cout << "チャンネル数       : " << data.channels << std::endl;
    std::cout << "総サンプル数(フレーム): " << data.frames << std::endl;
    std::cout << "収録時間          : " << (double)data.frames / data.sample_rate << " 秒"
              << std::endl;

    std::vector<double> interleaved(static_cast<size_t>(data.frames) * data.channels);
    sf_count_t read_count = sf_readf_double(file, interleaved.data(), data.frames);
    if (read_count != data.frames) {
        std::cerr << "[警告] 読み込んだサンプル数が期待値と異なります: " << read_count << " / "
                  << data.frames << std::endl;
    }
    sf_close(file);

    data.mono.resize(data.frames);
    if (data.channels == 2) {
        for (sf_count_t n = 0; n < data.frames; ++n) {
            double left = interleaved[static_cast<size_t>(n) * 2 + 0];
            double right = interleaved[static_cast<size_t>(n) * 2 + 1];
            data.mono[n] = (left + right) / 2.0;
        }
    } else if (data.channels == 1) {
        for (sf_count_t n = 0; n < data.frames; ++n) {
            data.mono[n] = interleaved[n];
        }
    } else {
        std::cerr << "[警告] 想定外のチャンネル数 (" << data.channels
                  << ") です。全チャンネル平均でモノラル化します。" << std::endl;
        for (sf_count_t n = 0; n < data.frames; ++n) {
            double sum = 0.0;
            for (int c = 0; c < data.channels; ++c) {
                sum += interleaved[static_cast<size_t>(n) * data.channels + c];
            }
            data.mono[n] = sum / data.channels;
        }
    }

    return data;
}

// ============================================================
// 2. 波形をCSVに出力（間引きあり）
// ============================================================
void writeWaveformCSV(const WavData& data, const std::string& out_path,
                       size_t max_points = 20000) {
    std::ofstream ofs(out_path);
    if (!ofs) {
        std::cerr << "[エラー] " << out_path << " を書き込めません。" << std::endl;
        return;
    }

    ofs << "time_sec,amplitude\n";
    ofs << std::fixed << std::setprecision(6);

    size_t total = data.mono.size();
    size_t step = 1;
    if (total > max_points) {
        step = total / max_points;
        if (step < 1) step = 1;
    }

    for (size_t n = 0; n < total; n += step) {
        double t = static_cast<double>(n) / data.sample_rate;
        ofs << t << "," << data.mono[n] << "\n";
    }

    std::cout << "波形CSVを出力しました: " << out_path << " (間引き間隔=" << step
              << ", 出力点数=" << (total / step + 1) << ")" << std::endl;
}

// ============================================================
// 3. 発音開始位置（onset）候補の検出
// ============================================================
std::vector<OnsetCandidate> detectOnsets(const WavData& data, double k_sigma = 4.0,
                                          double debounce_sec = 0.05) {
    const auto& x = data.mono;
    size_t n_total = x.size();
    std::vector<double> change(n_total, 0.0);

    for (size_t n = 1; n < n_total; ++n) {
        change[n] = std::fabs(x[n] - x[n - 1]);
    }

    double mean = std::accumulate(change.begin(), change.end(), 0.0) / n_total;
    double variance = 0.0;
    for (double c : change) {
        variance += (c - mean) * (c - mean);
    }
    variance /= n_total;
    double stddev = std::sqrt(variance);

    double threshold = mean + k_sigma * stddev;

    std::cout << "==== onset候補検出 ====" << std::endl;
    std::cout << "差分の平均値       : " << mean << std::endl;
    std::cout << "差分の標準偏差      : " << stddev << std::endl;
    std::cout << "検出閾値 (mean+" << k_sigma << "*sigma): " << threshold << std::endl;

    std::vector<OnsetCandidate> candidates;
    size_t debounce_samples = static_cast<size_t>(debounce_sec * data.sample_rate);
    long long last_index = -static_cast<long long>(debounce_samples) - 1;

    for (size_t n = 1; n < n_total; ++n) {
        if (change[n] > threshold) {
            if (static_cast<long long>(n) - last_index >
                static_cast<long long>(debounce_samples)) {
                OnsetCandidate c;
                c.time_sec = static_cast<double>(n) / data.sample_rate;
                c.change = change[n];
                candidates.push_back(c);
                last_index = static_cast<long long>(n);
            }
        }
    }

    std::cout << "検出されたonset候補数: " << candidates.size() << std::endl;

    return candidates;
}

void writeOnsetCSV(const std::vector<OnsetCandidate>& candidates, const std::string& out_path) {
    std::ofstream ofs(out_path);
    if (!ofs) {
        std::cerr << "[エラー] " << out_path << " を書き込めません。" << std::endl;
        return;
    }
    ofs << "time_sec,change\n";
    ofs << std::fixed << std::setprecision(6);
    for (const auto& c : candidates) {
        ofs << c.time_sec << "," << c.change << "\n";
    }
    std::cout << "onset候補CSVを出力しました: " << out_path << std::endl;
}

// ============================================================
// 4. 解析区間の抽出
// ============================================================
std::vector<double> extractAnalysisWindow(const WavData& data, double t0, double start_offset,
                                           double end_offset, int N,
                                           double& out_window_start_sec) {
    double window_start_sec = t0 + start_offset;
    double window_end_sec = t0 + end_offset;

    long long start_index =
        static_cast<long long>(std::round(window_start_sec * data.sample_rate));
    long long end_index = static_cast<long long>(std::round(window_end_sec * data.sample_rate));

    std::cout << "==== 解析区間 (N=" << N << ") ====" << std::endl;
    std::cout << "onset基準時刻 t0    : " << t0 << " 秒" << std::endl;
    std::cout << "解析区間開始       : " << window_start_sec << " 秒 (sample index "
              << start_index << ")" << std::endl;
    std::cout << "解析区間終了       : " << window_end_sec << " 秒 (sample index " << end_index
              << ")" << std::endl;

    long long n_total = static_cast<long long>(data.mono.size());
    if (start_index < 0) start_index = 0;
    if (start_index + N > n_total) {
        std::cerr << "[エラー] 解析区間(N=" << N << ")がファイル末尾を超えています。"
                  << "音声ファイルの長さ、またはonset位置・offsetの指定を確認してください。"
                  << std::endl;
        std::exit(1);
    }
    if (end_index <= start_index) {
        std::cerr << "[警告] 解析区間の終了が開始以前です。start_offset/end_offsetの指定を確認"
                     "してください。"
                  << std::endl;
    }

    std::vector<double> window(N);
    for (int i = 0; i < N; ++i) {
        window[i] = data.mono[static_cast<size_t>(start_index) + i];
    }

    out_window_start_sec = static_cast<double>(start_index) / data.sample_rate;
    return window;
}

// ============================================================
// 5. 自作DFT（離散フーリエ変換）
// ============================================================
//
// 【数学的背景】
//     X[k] = Σ_{n=0}^{N-1} x[n] * exp(-2πi * k * n / N)      (k = 0, 1, ..., N-1)
//
// オイラーの公式 exp(-iθ) = cos(θ) - i sin(θ) より、
//     X[k] = Σ_n x[n] cos(2πkn/N)  -  i Σ_n x[n] sin(2πkn/N)
// つまりX[k]の実部・虚部は、それぞれ周波数k*(fs/N)のcos波・sin波との
// 相関を表す。|X[k]|が振幅、arg(X[k])が位相に対応する。
//
// frequency = k * fs / N,  周波数分解能 Δf = fs / N。
//
// 【計算量】
// この実装はO(N^2)。N=4096→約1670万回、N=16384→約2億6800万回の
// 複素乗算が必要（比は(16384/4096)^2=16倍）。
// FFT(O(N log N))は今回未使用。後で比較するためのベースラインとする。
std::vector<std::complex<double>> computeDFT(const std::vector<double>& x, int N) {
    std::vector<std::complex<double>> X(N, std::complex<double>(0.0, 0.0));

    for (int k = 0; k < N; ++k) {
        std::complex<double> sum(0.0, 0.0);
        for (int n = 0; n < N; ++n) {
            double theta = -2.0 * PI * static_cast<double>(k) * static_cast<double>(n) /
                           static_cast<double>(N);
            std::complex<double> twiddle(std::cos(theta), std::sin(theta));
            sum += std::complex<double>(x[n], 0.0) * twiddle;
        }
        X[k] = sum;
    }

    return X;
}

// ============================================================
// 6. 周波数スペクトルの構築とCSV出力
// ============================================================
std::vector<SpectrumPoint> buildSpectrum(const std::vector<std::complex<double>>& X,
                                          int sample_rate, int N) {
    int half = N / 2;
    std::vector<SpectrumPoint> spectrum(half + 1);

    double max_mag = 0.0;
    for (int k = 0; k <= half; ++k) {
        double mag = std::abs(X[k]);
        double freq = static_cast<double>(k) * sample_rate / static_cast<double>(N);
        spectrum[k].frequency_hz = freq;
        spectrum[k].magnitude = mag;
        if (mag > max_mag) max_mag = mag;
    }

    for (auto& p : spectrum) {
        p.magnitude_normalized = (max_mag > 0.0) ? (p.magnitude / max_mag) : 0.0;
    }

    return spectrum;
}

void writeSpectrumCSV(const std::vector<SpectrumPoint>& spectrum, const std::string& out_path) {
    std::ofstream ofs(out_path);
    if (!ofs) {
        std::cerr << "[エラー] " << out_path << " を書き込めません。" << std::endl;
        return;
    }
    ofs << "frequency_hz,magnitude,magnitude_normalized\n";
    ofs << std::fixed << std::setprecision(6);
    for (const auto& p : spectrum) {
        ofs << p.frequency_hz << "," << p.magnitude << "," << p.magnitude_normalized << "\n";
    }
    std::cout << "スペクトルCSVを出力しました: " << out_path << std::endl;
}

// ============================================================
// 7. 最大ピークの検出（DC成分を除く、全周波数中の最大値）
// ============================================================
//
// 【重要な注意】雑音や打鍵音由来の可能性があり、対象音の基音とは限らない。
SpectrumPoint findPeak(const std::vector<SpectrumPoint>& spectrum) {
    SpectrumPoint peak = spectrum[1];
    for (size_t i = 1; i < spectrum.size(); ++i) {
        if (spectrum[i].magnitude > peak.magnitude) {
            peak = spectrum[i];
        }
    }
    return peak;
}

// ============================================================
// 7b. 指定した周波数付近のピークを検出する
// ============================================================
SpectrumPoint findPeakNearFrequency(const std::vector<SpectrumPoint>& spectrum,
                                     double target_freq, double range_hz) {
    double lo = target_freq - range_hz;
    double hi = target_freq + range_hz;

    SpectrumPoint peak;
    bool found = false;
    for (const auto& p : spectrum) {
        if (p.frequency_hz >= lo && p.frequency_hz <= hi) {
            if (!found || p.magnitude > peak.magnitude) {
                peak = p;
                found = true;
            }
        }
    }

    if (!found) {
        double best_diff = 1e18;
        for (const auto& p : spectrum) {
            double diff = std::fabs(p.frequency_hz - target_freq);
            if (diff < best_diff) {
                best_diff = diff;
                peak = p;
            }
        }
    }

    return peak;
}

// ============================================================
// 理論周波数との比較計算
// ============================================================
//
// cent = 1200 * log2(f2 / f1)  (12平均律・半音=100セントの単位)
FrequencyComparison compareToTarget(double measured_freq, double target_freq) {
    FrequencyComparison c;
    c.measured_frequency_hz = measured_freq;
    c.error_hz = measured_freq - target_freq;
    c.error_percent = (c.error_hz / target_freq) * 100.0;
    c.cent_error = 1200.0 * std::log2(measured_freq / target_freq);
    return c;
}

// ============================================================
// N=4096 / N=16384 それぞれについてDFT解析一式を実行する
// （対象音は単音・和音どちらにも対応: target_notesは1個以上の要素を持つ）
// ============================================================
NAnalysisResult analyzeWithN(const WavData& data, double t0, double start_offset,
                              double end_offset, int N,
                              const std::vector<TargetNote>& target_notes) {
    NAnalysisResult result;
    result.N = N;
    result.frequency_resolution_hz = static_cast<double>(data.sample_rate) / N;

    std::vector<double> window =
        extractAnalysisWindow(data, t0, start_offset, end_offset, N, result.window_start_sec);

    std::cout << "\nDFT計算中... (N=" << N << ")" << std::endl;
    auto dft_start = std::chrono::high_resolution_clock::now();
    std::vector<std::complex<double>> X = computeDFT(window, N);
    auto dft_end = std::chrono::high_resolution_clock::now();
    result.dft_time_ms = std::chrono::duration<double, std::milli>(dft_end - dft_start).count();
    std::cout << "DFT計算完了 (N=" << N << "): " << result.dft_time_ms << " ms" << std::endl;

    result.spectrum = buildSpectrum(X, data.sample_rate, N);
    result.global_peak = findPeak(result.spectrum);

    result.global_peak_matches_any_target = false;
    for (const auto& note : target_notes) {
        TargetNotePeakResult tpr;
        tpr.note = note;
        tpr.peak = findPeakNearFrequency(result.spectrum, note.frequency_hz, PEAK_SEARCH_RANGE_HZ);
        tpr.comparison = compareToTarget(tpr.peak.frequency_hz, note.frequency_hz);
        result.target_peaks.push_back(tpr);

        if (std::fabs(result.global_peak.frequency_hz - tpr.peak.frequency_hz) <
            result.frequency_resolution_hz) {
            result.global_peak_matches_any_target = true;
        }
    }

    return result;
}

// ============================================================
// 8. 比較結果テキスト(analysis_comparison.txt)の出力
// ============================================================
void writeComparisonResult(const std::string& out_path, const WavData& data,
                            const std::vector<TargetNote>& target_notes,
                            const std::string& target_source_description,
                            const std::vector<NAnalysisResult>& results) {
    std::ofstream ofs(out_path);
    if (!ofs) {
        std::cerr << "[エラー] " << out_path << " を書き込めません。" << std::endl;
        return;
    }

    ofs << std::fixed << std::setprecision(6);
    ofs << "==== 音声解析結果（N=4096 と N=16384 の比較） ====\n";
    ofs << "\n";
    ofs << "[解析上の注意]\n";
    ofs << "- この録音はスマートフォンのマイクを使用している。\n";
    ofs << "- 録音環境によっては周囲の雑音が含まれている可能性がある。\n";
    ofs << "- 雑音が含まれている場合、周波数スペクトルに目的の音以外の周波数成分が現れる可能性がある。\n";
    ofs << "- DFTの最大ピークが必ずしも録音した音の基音を意味するとは限らない。\n";
    ofs << "- 周波数分解能が有限である場合、理論値と測定値に差が生じる可能性がある。\n";
    ofs << "- 解析結果は「録音された信号において観測されたピーク」として扱う。\n";
    ofs << "\n";
    ofs << "[対象音]\n";
    ofs << "target_source = " << target_source_description << "\n";
    for (const auto& note : target_notes) {
        ofs << "  note = " << note.name << "   theoretical_frequency_hz = " << note.frequency_hz
            << "\n";
    }
    ofs << "peak_search_range_hz = ±" << PEAK_SEARCH_RANGE_HZ
        << "  (各対象音の理論周波数付近ピークの探索範囲)\n";
    ofs << "\n";
    ofs << "[入力ファイル情報]\n";
    ofs << "sample_rate_hz  = " << data.sample_rate << "\n";
    ofs << "channels        = " << data.channels << "\n";
    ofs << "total_frames    = " << data.frames << "\n";
    ofs << "\n";

    for (const auto& r : results) {
        ofs << "========================================\n";
        ofs << "[N = " << r.N << "]\n";
        ofs << "analysis_start_time_sec  = " << r.window_start_sec
            << "   (DFTに使用した区間の開始時刻)\n";
        ofs << "sample_count_N           = " << r.N << "\n";
        ofs << "frequency_resolution_hz  = " << r.frequency_resolution_hz
            << "   (= sample_rate / N)\n";
        ofs << "dft_calculation_time_ms  = " << r.dft_time_ms << "\n";
        ofs << "\n";

        ofs << "--- 全体最大ピーク（DC除く、スペクトル全周波数中で最大） ---\n";
        ofs << "# 注意: 雑音や対象音以外に由来する可能性があり、基音であるとは限らない。\n";
        ofs << "global_peak_frequency_hz   = " << r.global_peak.frequency_hz << "\n";
        ofs << "global_peak_magnitude      = " << r.global_peak.magnitude << "\n";
        ofs << "matches_any_target_note    = " << (r.global_peak_matches_any_target ? "yes" : "no")
            << "  (全体最大ピークが、いずれかの対象音の理論周波数付近ピークと同一ビンとみなせるか)\n";
        ofs << "\n";

        for (const auto& tpr : r.target_peaks) {
            ofs << "--- 対象音「" << tpr.note.name << "」理論周波数付近のピーク（"
                << tpr.note.frequency_hz << " Hz ± " << PEAK_SEARCH_RANGE_HZ << " Hz） ---\n";
            ofs << "near_target_peak_frequency_hz [" << tpr.note.name
                << "] = " << tpr.peak.frequency_hz << "\n";
            ofs << "near_target_peak_magnitude    [" << tpr.note.name
                << "] = " << tpr.peak.magnitude << "\n";
            ofs << "measured_frequency_hz         [" << tpr.note.name
                << "] = " << tpr.comparison.measured_frequency_hz << "\n";
            ofs << "error_hz                      [" << tpr.note.name
                << "] = " << tpr.comparison.error_hz << "\n";
            ofs << "error_percent                 [" << tpr.note.name
                << "] = " << tpr.comparison.error_percent << "\n";
            ofs << "cent_error                    [" << tpr.note.name
                << "] = " << tpr.comparison.cent_error << "\n";
            ofs << "\n";
        }
    }

    if (results.size() >= 2) {
        const auto& r1 = results[0];
        const auto& r2 = results[1];
        double n_ratio = static_cast<double>(r2.N) / static_cast<double>(r1.N);
        double theoretical_time_ratio = n_ratio * n_ratio;
        double actual_time_ratio =
            (r1.dft_time_ms > 0.0) ? (r2.dft_time_ms / r1.dft_time_ms) : 0.0;

        ofs << "========================================\n";
        ofs << "[計算時間の比較: N=" << r1.N << " → N=" << r2.N << "]\n";
        ofs << "N_ratio                       = " << n_ratio << "\n";
        ofs << "theoretical_time_ratio(N^2)   = " << theoretical_time_ratio
            << "   (計算量がO(N^2)であることから予想される倍率)\n";
        ofs << "actual_time_ratio             = " << actual_time_ratio
            << "   (実測: N=" << r2.N << "の時間 / N=" << r1.N << "の時間)\n";
        ofs << "frequency_resolution_ratio    = "
            << (r1.frequency_resolution_hz / r2.frequency_resolution_hz)
            << "   (Δf(N=" << r1.N << ") / Δf(N=" << r2.N << "), 分解能がどれだけ細かくなったか)\n";
    }

    ofs.close();

    std::cout << "\n==== N比較 解析結果 ====" << std::endl;
    for (const auto& r : results) {
        std::cout << "[N=" << r.N << "] Δf=" << r.frequency_resolution_hz << " Hz, "
                  << "計算時間=" << r.dft_time_ms << " ms" << std::endl;
        std::cout << "  全体最大ピーク: " << r.global_peak.frequency_hz << " Hz"
                  << (r.global_peak_matches_any_target ? "" : "  (対象音の理論値付近ピークとは別)")
                  << std::endl;
        for (const auto& tpr : r.target_peaks) {
            std::cout << "  [" << tpr.note.name << "] 理論値付近のピーク: "
                      << tpr.peak.frequency_hz << " Hz (理論値 " << tpr.note.frequency_hz
                      << " Hz, cent誤差 " << tpr.comparison.cent_error << ")" << std::endl;
        }
    }
    std::cout << "比較結果を出力しました: " << out_path << std::endl;
}

// ============================================================
// 9. gnuplotスクリプトの生成
// ============================================================
//
// 【文字化け対策】
// Windows(MSYS2)環境のgnuplotで日本語タイトル・ラベルを使うと、
// フォント設定によっては文字化けすることがある。
// そのため、以下のgnuplotスクリプト生成関数では、
// タイトル・軸ラベル・凡例などのテキストをすべて英語(ASCII)で
// 生成するようにしている。「set encoding utf8」も念のため付与する。

// 波形プロット用スクリプト
void writeWaveformGnuScript(const std::string& gnu_path, const std::string& csv_name) {
    std::ofstream ofs(gnu_path);
    ofs << "# " << gnu_path << "\n";
    ofs << "# Usage: gnuplot -persist " << gnu_path << "\n";
    ofs << "set encoding utf8\n";
    ofs << "set datafile separator ','\n";
    ofs << "set title 'Waveform (" << csv_name << ")'\n";
    ofs << "set xlabel 'Time (sec)'\n";
    ofs << "set ylabel 'Amplitude'\n";
    ofs << "set grid\n";
    ofs << "plot '" << csv_name << "' using 1:2 with lines title 'amplitude'\n";
    std::cout << "gnuplotスクリプトを出力しました: " << gnu_path << std::endl;
}

// N一つ分のスペクトルプロット用スクリプト（対象音の理論周波数に縦線）
void writeSpectrumGnuScript(const std::string& gnu_path, const std::string& csv_name, int N,
                             const std::vector<TargetNote>& target_notes) {
    // 対象音の最高周波数がデフォルト表示上限を超える場合は表示範囲を自動的に広げる
    double xrange_max = DEFAULT_SPECTRUM_XRANGE_MAX_HZ;
    for (const auto& note : target_notes) {
        double margin = note.frequency_hz * 1.3;
        if (margin > xrange_max) xrange_max = margin;
    }

    std::ofstream ofs(gnu_path);
    ofs << "# " << gnu_path << "\n";
    ofs << "# Usage: gnuplot -persist " << gnu_path << "\n";
    ofs << "set encoding utf8\n";
    ofs << "set datafile separator ','\n";
    ofs << "set title 'Spectrum (" << csv_name << ")  N=" << N << "'\n";
    ofs << "set xlabel 'Frequency (Hz)'\n";
    ofs << "set ylabel 'Magnitude'\n";
    ofs << std::fixed << std::setprecision(2);
    ofs << "set xrange [0:" << xrange_max << "]\n";
    ofs << "set grid\n";
    ofs << "\n";

    // 対象音（単音 or 和音）ごとに理論周波数の位置へ縦線を表示する
    for (size_t i = 0; i < target_notes.size(); ++i) {
        const auto& note = target_notes[i];
        std::string ascii_name = sanitizeNoteAscii(note.name);
        std::string var_name = "TARGET_FREQ_" + std::to_string(i);
        ofs << var_name << " = " << std::fixed << std::setprecision(6) << note.frequency_hz
            << "\n";
        ofs << "set arrow from " << var_name << ", graph 0 to " << var_name
            << ", graph 1 nohead lc rgb 'red' lw 2 dt 2\n";
        double label_height = 0.95 - 0.05 * static_cast<double>(i);
        ofs << "set label '" << ascii_name << " (" << std::fixed << std::setprecision(2)
            << note.frequency_hz << " Hz)' at " << var_name << ", graph " << label_height
            << " tc rgb 'red' offset 1,0\n";
    }
    ofs << "\n";
    ofs << "plot '" << csv_name << "' using 1:2 with lines title 'magnitude (N=" << N << ")'\n";
    std::cout << "gnuplotスクリプトを出力しました: " << gnu_path << std::endl;
}

// N=4096とN=16384を対象音の理論周波数付近で拡大比較するスクリプト
// （和音の場合は全ての対象音を含む範囲まで表示し、各音に縦線を引く）
void writeComparisonZoomGnuScript(const std::string& gnu_path, const std::string& csv_name_a,
                                   int N_a, const std::string& csv_name_b, int N_b,
                                   const std::vector<TargetNote>& target_notes) {
    double lo = 0.0, hi = 0.0;
    if (!target_notes.empty()) {
        lo = target_notes.front().frequency_hz;
        hi = target_notes.front().frequency_hz;
        for (const auto& note : target_notes) {
            lo = std::min(lo, note.frequency_hz);
            hi = std::max(hi, note.frequency_hz);
        }
        lo -= ZOOM_MARGIN_HZ;
        hi += ZOOM_MARGIN_HZ;
        if (lo < 0.0) lo = 0.0;
    } else {
        lo = 0.0;
        hi = DEFAULT_SPECTRUM_XRANGE_MAX_HZ;
    }

    // タイトル用に対象音名を "C4+E4+G4" のようにASCII安全な形で連結する
    std::string notes_label;
    for (size_t i = 0; i < target_notes.size(); ++i) {
        if (i > 0) notes_label += "+";
        notes_label += sanitizeNoteAscii(target_notes[i].name);
    }
    if (notes_label.empty()) notes_label = "target";

    std::ofstream ofs(gnu_path);
    ofs << "# " << gnu_path << "\n";
    ofs << "# Usage: gnuplot -persist " << gnu_path << "\n";
    ofs << "# Compare N=" << N_a << " and N=" << N_b << " spectra, zoomed near the target note(s)"
        << " (" << notes_label << ").\n";
    ofs << "set encoding utf8\n";
    ofs << "set datafile separator ','\n";
    ofs << "set title 'Zoom near " << notes_label << " (N=" << N_a << " vs N=" << N_b << ")'\n";
    ofs << "set xlabel 'Frequency (Hz)'\n";
    ofs << "set ylabel 'Magnitude'\n";
    ofs << std::fixed << std::setprecision(6);
    ofs << "set xrange [" << lo << ":" << hi << "]\n";
    ofs << "set grid\n";
    ofs << "\n";

    for (size_t i = 0; i < target_notes.size(); ++i) {
        const auto& note = target_notes[i];
        std::string ascii_name = sanitizeNoteAscii(note.name);
        std::string var_name = "TARGET_FREQ_" + std::to_string(i);
        ofs << var_name << " = " << note.frequency_hz << "\n";
        ofs << "set arrow from " << var_name << ", graph 0 to " << var_name
            << ", graph 1 nohead lc rgb 'red' lw 2 dt 2\n";
        double label_height = 0.95 - 0.05 * static_cast<double>(i);
        ofs << "set label '" << ascii_name << " (" << std::fixed << std::setprecision(2)
            << note.frequency_hz << " Hz)' at " << var_name << ", graph " << label_height
            << " tc rgb 'red' offset 1,0\n";
    }
    ofs << "\n";
    ofs << std::fixed << std::setprecision(6);
    ofs << "plot '" << csv_name_a << "' using 1:2 with linespoints pt 7 ps 0.5 title 'N=" << N_a
        << " (df=" << (48000.0 / N_a) << "Hz)', \\\n";
    ofs << "     '" << csv_name_b << "' using 1:2 with linespoints pt 7 ps 0.5 title 'N=" << N_b
        << " (df=" << (48000.0 / N_b) << "Hz)'\n";
    std::cout << "gnuplotスクリプトを出力しました: " << gnu_path << std::endl;
}

// ============================================================
// メイン処理
// ============================================================
//
// コマンドライン引数:
//   argv[1] : WAVファイルのパス（必須）
//   argv[2] : start_offset（秒、省略時0.5）
//   argv[3] : end_offset  （秒、省略時1.5）
//   argv[4] : 録音一覧CSVのパス（省略時 "List_of_recordings.csv"）
//
// 例:
//   ./analyze_audio voice_2.wav
//   ./analyze_audio voice_2.wav 0.5 1.5
//   ./analyze_audio voice_9.wav 0.5 1.5 List_of_recordings.csv
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "使い方: " << argv[0]
                  << " <wavファイル> [start_offset=0.5] [end_offset=1.5] [recordings_csv]"
                  << std::endl;
        std::cerr << "例    : " << argv[0] << " voice_2.wav 0.5 1.5" << std::endl;
        return 1;
    }

    std::string wav_path = argv[1];
    double start_offset = 0.5;
    double end_offset = 1.5;
    std::string csv_path = DEFAULT_RECORDINGS_CSV;
    if (argc >= 3) start_offset = std::stod(argv[2]);
    if (argc >= 4) end_offset = std::stod(argv[3]);
    if (argc >= 5) csv_path = argv[4];

    std::string base_name = getBaseNameNoExt(wav_path);
    std::string prefix = base_name + "_";

    // --- 録音一覧CSVから対象音を自動判定 ---
    bool csv_loaded_ok = false;
    std::map<int, std::string> recordings = loadRecordingsCsv(csv_path, csv_loaded_ok);

    std::vector<TargetNote> target_notes;
    std::string target_source_description;

    // true: CSVから対象音を正しく特定できた（＝音名をファイル名に含めてよい）
    // false: 何らかの理由でフォールバックを使用した（＝音名は推測に過ぎないため
    //        ファイル名には含めない）
    bool target_notes_confirmed_by_csv = false;

    std::optional<int> file_number = extractTrailingNumber(base_name);

    if (!csv_loaded_ok) {
        std::cerr << "[警告] 録音一覧CSV \"" << csv_path
                  << "\" を読み込めませんでした。フォールバックとして "
                  << FALLBACK_NOTE_NAME << " (" << FALLBACK_FREQUENCY_HZ
                  << " Hz) を使用します。" << std::endl;
        target_notes.push_back({FALLBACK_NOTE_NAME, FALLBACK_FREQUENCY_HZ});
        target_source_description = "fallback (CSV not found)";
    } else if (!file_number.has_value()) {
        std::cerr << "[警告] WAVファイル名から番号を抽出できませんでした: " << base_name
                  << " 。フォールバックとして " << FALLBACK_NOTE_NAME << " ("
                  << FALLBACK_FREQUENCY_HZ << " Hz) を使用します。" << std::endl;
        target_notes.push_back({FALLBACK_NOTE_NAME, FALLBACK_FREQUENCY_HZ});
        target_source_description = "fallback (file number not found in filename)";
    } else {
        auto it = recordings.find(*file_number);
        if (it == recordings.end()) {
            std::cerr << "[警告] file number " << *file_number << " がCSVに見つかりません。"
                      << "フォールバックとして " << FALLBACK_NOTE_NAME << " ("
                      << FALLBACK_FREQUENCY_HZ << " Hz) を使用します。" << std::endl;
            target_notes.push_back({FALLBACK_NOTE_NAME, FALLBACK_FREQUENCY_HZ});
            target_source_description = "fallback (file number " + std::to_string(*file_number) +
                                         " not in CSV)";
        } else {
            target_notes = parseTargetNotes(it->second);
            if (target_notes.empty()) {
                std::cerr << "[警告] CSVの音名 \"" << it->second
                          << "\" を解釈できませんでした。フォールバックとして "
                          << FALLBACK_NOTE_NAME << " (" << FALLBACK_FREQUENCY_HZ
                          << " Hz) を使用します。" << std::endl;
                target_notes.push_back({FALLBACK_NOTE_NAME, FALLBACK_FREQUENCY_HZ});
                target_source_description =
                    "fallback (could not parse CSV entry \"" + it->second + "\")";
            } else {
                target_source_description = csv_path + " (file number " +
                                             std::to_string(*file_number) + " = \"" + it->second +
                                             "\")";
                target_notes_confirmed_by_csv = true;
            }
        }
    }

    std::cout << "解析対象ファイル: " << wav_path << std::endl;
    std::cout << "出力接頭辞      : " << prefix << std::endl;
    std::cout << "start_offset    : " << start_offset << " 秒" << std::endl;
    std::cout << "end_offset      : " << end_offset << " 秒" << std::endl;
    std::cout << "対象音の判定元   : " << target_source_description << std::endl;
    std::cout << "対象音          : ";
    for (size_t i = 0; i < target_notes.size(); ++i) {
        if (i > 0) std::cout << " + ";
        std::cout << target_notes[i].name << "(" << target_notes[i].frequency_hz << "Hz)";
    }
    std::cout << std::endl << std::endl;

    // --- 1. WAV読み込み ---
    WavData data = readWavFile(wav_path);

    // --- 2. 波形CSV出力 ---
    std::string waveform_csv = prefix + "waveform.csv";
    writeWaveformCSV(data, waveform_csv);

    // --- 3. onset候補検出 ---
    std::vector<OnsetCandidate> onsets = detectOnsets(data);
    writeOnsetCSV(onsets, prefix + "onset_candidates.csv");

    if (onsets.empty()) {
        std::cerr << "[エラー] onset候補が検出できませんでした。"
                  << "録音の音量や検出パラメータ(k_sigma)を見直してください。" << std::endl;
        return 1;
    }

    double t0 = onsets.front().time_sec;

    // --- 4～8. N=4096, N=16384 それぞれについてDFT解析を実行 ---
    std::vector<NAnalysisResult> results;
    for (int N : DFT_SIZES) {
        NAnalysisResult r = analyzeWithN(data, t0, start_offset, end_offset, N, target_notes);

        std::string spectrum_csv = prefix + "spectrum_N" + std::to_string(N) + ".csv";
        writeSpectrumCSV(r.spectrum, spectrum_csv);

        results.push_back(std::move(r));
    }

    // --- 比較結果テキストの出力 ---
    writeComparisonResult(prefix + "analysis_comparison.txt", data, target_notes,
                           target_source_description, results);

    // --- 9. gnuplotスクリプト出力 ---
    writeWaveformGnuScript(prefix + "plot_waveform.gnu", waveform_csv);

    for (const auto& r : results) {
        std::string spectrum_csv = prefix + "spectrum_N" + std::to_string(r.N) + ".csv";
        std::string spectrum_gnu = prefix + "plot_spectrum_N" + std::to_string(r.N) + ".gnu";
        writeSpectrumGnuScript(spectrum_gnu, spectrum_csv, r.N, target_notes);
    }

    if (results.size() >= 2) {
        std::string csv_a = prefix + "spectrum_N" + std::to_string(results[0].N) + ".csv";
        std::string csv_b = prefix + "spectrum_N" + std::to_string(results[1].N) + ".csv";

        // ファイル名は実際の対象音名に基づいて生成する
        // （以前のように "C4" 固定ではなく、D4やC4-E4-G4等にも対応）
        //
        // ただし、CSVから対象音を確定できなかった場合（CSVが見つからない、
        // 該当するfile numberが無い、音名を解釈できない等）は、
        // フォールバック値（推測に過ぎない音名）をファイル名に含めると
        // 誤解を招くため、音名部分を省略した
        // "<wav名>_plot_spectrum_compare.gnu" という名前にする。
        std::string comparison_gnu;
        if (target_notes_confirmed_by_csv) {
            std::string notes_for_filename;
            for (size_t i = 0; i < target_notes.size(); ++i) {
                if (i > 0) notes_for_filename += "-";
                notes_for_filename += sanitizeNoteForFilename(target_notes[i].name);
            }
            comparison_gnu = prefix + "plot_spectrum_compare_" + notes_for_filename + ".gnu";
        } else {
            comparison_gnu = prefix + "plot_spectrum_compare.gnu";
        }

        writeComparisonZoomGnuScript(comparison_gnu, csv_a, results[0].N, csv_b, results[1].N,
                                      target_notes);
    }

    std::cout << "\n全ての処理が完了しました。" << std::endl;
    return 0;
}