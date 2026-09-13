// chord_summary.cpp
//
// analyze_audio が生成する voice_<N>_analysis_comparison.txt を読み込み、
// 2音の和音（例: C4-E4）について、理論周波数・実測周波数を
// 決まった書式のCSVにまとめて出力するプログラム。
//
// 今回は「2音の和音専用」（3音以上の和音には対応しない）。
//
// 実測周波数は、N=4096とN=16384のうち周波数分解能が細かく
// より理論値に近い値を示すN=16384の結果を採用する
// （N=16384 の Δf ≈ 2.93Hz は N=4096 の Δf ≈ 11.72Hz より高精度）。
//
// ------------------------------------------------------------
// 使い方
// ------------------------------------------------------------
// (a) 単一ファイルモード（引数1つ）
//     ./chord_summary <番号>
//
//     例:
//       ./chord_summary 9
//         → "voice_9_analysis_comparison.txt" を読み込み、
//           "voice_9_chord_summary.csv" を出力する（標準出力にも表示する）
//
// (b) 複数ファイルまとめモード（引数2つ：開始番号 終了番号）
//     ./chord_summary <開始番号> <終了番号>
//
//     例:
//       ./chord_summary 9 20
//         → "voice_9_analysis_comparison.txt" から
//           "voice_20_analysis_comparison.txt" までを順に処理し、
//           2音の和音として正しく読み取れたものだけをまとめて
//           "summarized_chord.csv" に複数行で出力する。
//         　ファイルが存在しない番号や、2音の和音でない番号（対象音が
//           1音のみ、または3音以上）は警告を表示してスキップし、
//           処理は継続する。
//
// ------------------------------------------------------------
// 出力形式（例、単一ファイルモード・複数ファイルまとめモード共通）
// ------------------------------------------------------------
//   音名, 1音名理論値, 2音目理論値, 1音目実測値, 2音目実測値,
//   L12, L13, L14, L67, L75,
//   C4-E4, 261.625565, 329.627557, 260.742188, 328.125000,
//
// 複数ファイルまとめモードでは、上記の1〜2行目（ヘッダ・セル参照行）は
// ファイル先頭に1回だけ出力し、3行目以降に処理できたファイルの数だけ
// データ行が並ぶ。
//
// 2行目の "L12, L13, L14, L67, L75," は、各列の値を貼り付ける先の
// （利用者が別途管理している）表計算シートのセル参照であり、
// このプログラムでは固定の文字列としてそのまま出力する。
//
// コンパイル方法（MSYS2 UCRT64、libsndfile不要）:
//   g++ -std=c++17 -O2 chord_summary.cpp -o chord_summary

#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// 2行目に固定で出力するセル参照ラベル（利用者の表計算シート上の転記先）
constexpr const char* CELL_LABELS_ROW = "L12, L13, L14, L67, L75,";

// ヘッダ行（1行目）
constexpr const char* HEADER_ROW = "音名, 1音名理論値, 2音目理論値, 1音目実測値, 2音目実測値,";

// 実測値としてどのNの結果を使うか
// （分解能が細かく理論値により近い値を示すため、N=16384を採用する）
constexpr int PREFERRED_N = 16384;

// 複数ファイルまとめモードの出力ファイル名
constexpr const char* SUMMARIZED_OUTPUT_PATH = "summarized_chord.csv";

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
// 対象音1つ分の情報
// ============================================================
struct NoteInfo {
    std::string name;
    double theoretical_hz = 0.0;
    double measured_hz = 0.0;
    bool measured_found = false;
};

// ============================================================
// analysis_comparison.txt 全体をパースし、対象音の一覧
// （理論周波数）を抽出する
// ============================================================
//
// 対象の行の形式（analyze_audio.cppの出力）:
//   "  note = C4   theoretical_frequency_hz = 261.625565"
std::vector<NoteInfo> parseTargetNotes(const std::vector<std::string>& lines) {
    std::vector<NoteInfo> notes;

    const std::string note_marker = "note = ";
    const std::string theo_marker = "theoretical_frequency_hz = ";

    for (const auto& raw_line : lines) {
        std::string line = raw_line;
        size_t note_pos = line.find(note_marker);
        size_t theo_pos = line.find(theo_marker);
        if (note_pos == std::string::npos || theo_pos == std::string::npos) continue;

        // "  note = C4   theoretical_frequency_hz = 261.625565"
        //          ^name_start          ^ここから数字
        size_t name_start = note_pos + note_marker.size();
        // 音名は次の空白文字まで
        size_t name_end = line.find_first_of(" \t", name_start);
        if (name_end == std::string::npos) continue;
        std::string name = trim(line.substr(name_start, name_end - name_start));

        std::string theo_str = trim(line.substr(theo_pos + theo_marker.size()));
        double theo_val;
        try {
            theo_val = std::stod(theo_str);
        } catch (...) {
            continue;
        }

        NoteInfo info;
        info.name = name;
        info.theoretical_hz = theo_val;
        notes.push_back(info);
    }

    return notes;
}

// ============================================================
// 指定したN（例: 16384）のセクションだけを抜き出す
// ============================================================
//
// analysis_comparison.txt は
//   ========================================
//   [N = 4096]
//   ...
//   ========================================
//   [N = 16384]
//   ...
//   ========================================
//   [計算時間の比較: ...]
// という構造になっている。
// "[N = <N>]" という行から、次の "========================================"
// （またはファイル末尾）までを、そのNのセクションとして抜き出す。
std::vector<std::string> extractNSection(const std::vector<std::string>& lines, int N) {
    std::string target_marker = "[N = " + std::to_string(N) + "]";
    std::vector<std::string> section;

    bool in_section = false;
    for (const auto& line : lines) {
        if (!in_section) {
            if (line.find(target_marker) != std::string::npos) {
                in_section = true;
            }
            continue;
        }
        // 区切り線（次のセクションの先頭）に到達したら終了
        if (line.find("========================================") != std::string::npos) {
            break;
        }
        section.push_back(line);
    }

    return section;
}

// ============================================================
// 指定したNセクション内から、各対象音の実測周波数
// (measured_frequency_hz [音名] = ...) を探して埋める
// ============================================================
void fillMeasuredFrequencies(const std::vector<std::string>& section,
                              std::vector<NoteInfo>& notes) {
    for (auto& note : notes) {
        std::string key = "[" + note.name + "]";
        for (const auto& line : section) {
            // "measured_frequency_hz         [C4] = 260.742188" のような行を探す
            if (line.find("measured_frequency_hz") == std::string::npos) continue;
            if (line.find(key) == std::string::npos) continue;

            size_t eq_pos = line.find_last_of('=');
            if (eq_pos == std::string::npos) continue;
            std::string val_str = trim(line.substr(eq_pos + 1));
            try {
                note.measured_hz = std::stod(val_str);
                note.measured_found = true;
            } catch (...) {
                // 変換できない場合は見つからなかった扱いのままにする
            }
            break;
        }
    }
}

// ============================================================
// 1つの analysis_comparison.txt を処理し、CSVのデータ行1行分
// （末尾の改行なし）を返す。
// 失敗した場合は std::nullopt を返し、error_message に理由を入れる。
// ============================================================
std::optional<std::string> processOneFile(const std::string& number,
                                           std::string& error_message) {
    std::string in_path = "voice_" + number + "_analysis_comparison.txt";

    std::ifstream ifs(in_path);
    if (!ifs) {
        error_message = "ファイルを開けませんでした: " + in_path;
        return std::nullopt;
    }

    std::vector<std::string> lines;
    {
        std::string line;
        while (std::getline(ifs, line)) {
            lines.push_back(line);
        }
    }

    // --- 対象音（理論周波数）を抽出 ---
    std::vector<NoteInfo> notes = parseTargetNotes(lines);

    if (notes.size() != 2) {
        std::ostringstream msg;
        msg << "2音の和音ではありません（検出された対象音の数: " << notes.size() << "） ("
            << in_path << ")";
        error_message = msg.str();
        return std::nullopt;
    }

    // --- N=16384セクションから実測周波数を抽出 ---
    std::vector<std::string> section = extractNSection(lines, PREFERRED_N);
    if (section.empty()) {
        error_message =
            "N=" + std::to_string(PREFERRED_N) + " のセクションが見つかりませんでした: " + in_path;
        return std::nullopt;
    }
    fillMeasuredFrequencies(section, notes);

    for (const auto& n : notes) {
        if (!n.measured_found) {
            error_message = "音 \"" + n.name + "\" の実測周波数(N=" +
                             std::to_string(PREFERRED_N) + ")が見つかりませんでした: " + in_path;
            return std::nullopt;
        }
    }

    // --- 和音名（例: "C4-E4"）を作る ---
    std::string chord_name = notes[0].name + "-" + notes[1].name;

    std::ostringstream row;
    row << std::fixed << std::setprecision(6);
    row << chord_name << ", " << notes[0].theoretical_hz << ", " << notes[1].theoretical_hz
        << ", " << notes[0].measured_hz << ", " << notes[1].measured_hz << ",";

    return row.str();
}

// ============================================================
// (a) 単一ファイルモード
// ============================================================
int runSingleFileMode(const std::string& number) {
    std::string error_message;
    std::optional<std::string> row = processOneFile(number, error_message);
    if (!row.has_value()) {
        std::cerr << "[エラー] " << error_message << std::endl;
        return 1;
    }

    std::string out_path = "voice_" + number + "_chord_summary.csv";

    std::ostringstream csv;
    csv << HEADER_ROW << "\n";
    csv << CELL_LABELS_ROW << "\n";
    csv << *row << "\n";
    std::string csv_text = csv.str();

    std::ofstream ofs(out_path);
    if (!ofs) {
        std::cerr << "[エラー] " << out_path << " を書き込めません。" << std::endl;
        return 1;
    }
    ofs << csv_text;
    ofs.close();

    std::cout << csv_text;
    std::cerr << "CSVを出力しました: " << out_path << std::endl;

    return 0;
}

// ============================================================
// (b) 複数ファイルまとめモード（開始番号〜終了番号）
// ============================================================
int runRangeMode(int start_number, int end_number) {
    if (start_number > end_number) {
        std::cerr << "[エラー] 開始番号が終了番号より大きくなっています: " << start_number
                   << " > " << end_number << std::endl;
        return 1;
    }

    std::vector<std::string> data_rows;
    int success_count = 0;
    int skip_count = 0;

    for (int n = start_number; n <= end_number; ++n) {
        std::string number = std::to_string(n);
        std::string error_message;
        std::optional<std::string> row = processOneFile(number, error_message);
        if (!row.has_value()) {
            std::cerr << "[警告] voice_" << number << " をスキップしました: " << error_message
                      << std::endl;
            ++skip_count;
            continue;
        }
        data_rows.push_back(*row);
        ++success_count;
    }

    std::ostringstream csv;
    csv << HEADER_ROW << "\n";
    csv << CELL_LABELS_ROW << "\n";
    for (const auto& row : data_rows) {
        csv << row << "\n";
    }
    std::string csv_text = csv.str();

    std::ofstream ofs(SUMMARIZED_OUTPUT_PATH);
    if (!ofs) {
        std::cerr << "[エラー] " << SUMMARIZED_OUTPUT_PATH << " を書き込めません。" << std::endl;
        return 1;
    }
    ofs << csv_text;
    ofs.close();

    std::cout << csv_text;
    std::cerr << "\n処理件数: 成功 " << success_count << " 件 / スキップ " << skip_count << " 件"
              << std::endl;
    std::cerr << "CSVを出力しました: " << SUMMARIZED_OUTPUT_PATH << std::endl;

    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2) {
        // 単一ファイルモード: ./chord_summary <番号>
        return runSingleFileMode(argv[1]);
    } else if (argc == 3) {
        // 複数ファイルまとめモード: ./chord_summary <開始番号> <終了番号>
        int start_number, end_number;
        try {
            start_number = std::stoi(argv[1]);
            end_number = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "[エラー] 開始番号・終了番号は整数で指定してください。" << std::endl;
            return 1;
        }
        return runRangeMode(start_number, end_number);
    } else {
        std::cerr << "使い方:" << std::endl;
        std::cerr << "  " << argv[0] << " <番号>                 単一ファイルを処理"
                  << std::endl;
        std::cerr << "  " << argv[0] << " <開始番号> <終了番号>   複数ファイルをまとめて処理"
                  << std::endl;
        std::cerr << "例:" << std::endl;
        std::cerr << "  " << argv[0]
                  << " 9      (voice_9_analysis_comparison.txt を読み込みます)" << std::endl;
        std::cerr << "  " << argv[0]
                  << " 9 20   (voice_9 ～ voice_20 をまとめて summarized_chord.csv に出力)"
                  << std::endl;
        return 1;
    }
}