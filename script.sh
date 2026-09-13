#!/usr/bin/env bash
#
# setup_github_repo.sh
#
# 「数学で読み解くピアノの音」自由研究フォルダを整理し、
# GitHub公開用のクリーンなgitリポジトリとして初期化するスクリプト。
#
# 想定環境: MSYS2 (Git Bash / MINGW64 shell) 上での実行

# 処理内容:
#   1. 重複フォルダ(voices, voices_converted, voices_txt)の削除
#   2. voices_organized/ を voices/ にリネーム（トップレベルへ移動）
#   3. voices_original/ を raw_audio/ にリネーム（トップレベルへ移動）
#   4. .exe / ビルドログ / LaTeX中間生成物の削除
#   5. ソースコード(*.cpp)を src/ へ、使い方md・CSVを docs/ へ、
#      集計csvを results/ へ移動
#   6. 「List of recordings.csv」(スペース入り)を
#      「List_of_recordings.csv」(アンダースコア)に統一してdocs/へ
#   7. .gitignore の配置
#   8. git init / add / commit
#   9. (オプション) GitHubへのpush

set -euo pipefail

# ============================================================
# 0. 事前チェック
# ============================================================

# このスクリプト自身があるディレクトリを基準にする
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "==== 作業ディレクトリ: $(pwd) ===="

# 安全確認: フォルダ名に "_github" が含まれているか確認する
# （誤って作業用オリジナルフォルダ(8th)で実行してしまうことを防ぐ簡易チェック）
CURRENT_DIR_NAME="$(basename "$(pwd)")"
if [[ "$CURRENT_DIR_NAME" != *_github* && "$CURRENT_DIR_NAME" != *github* ]]; then
  echo "[警告] 現在のフォルダ名に 'github' が含まれていません: $CURRENT_DIR_NAME"
  echo "        作業用オリジナルのフォルダ(8thなど)で実行しようとしていませんか？"
  read -r -p "        本当にこのフォルダで続行しますか？ [y/N]: " confirm
  if [[ "$confirm" != "y" && "$confirm" != "Y" ]]; then
    echo "中断しました。"
    exit 1
  fi
fi

# ============================================================
# 1. 重複フォルダの削除
# ============================================================
echo ""
echo "==== 1. 重複フォルダを削除 ===="

for dup_dir in "updated/voices" "updated/voices_converted" "updated/voices_txt"; do
  if [ -d "$dup_dir" ]; then
    echo "  削除: $dup_dir"
    rm -rf "$dup_dir"
  else
    echo "  スキップ（存在しない）: $dup_dir"
  fi
done

# ============================================================
# 2. voices_organized/ を トップレベルの voices/ へ
# ============================================================
echo ""
echo "==== 2. voices_organized/ を voices/ としてトップレベルへ移動 ===="

if [ -d "updated/voices_organized" ]; then
  if [ -d "voices" ]; then
    echo "  [警告] トップレベルに voices/ が既に存在するため、中身をマージします。"
    mv -v updated/voices_organized/* voices/ 2>/dev/null || true
    rmdir updated/voices_organized 2>/dev/null || true
  else
    mv -v updated/voices_organized voices
  fi
else
  echo "  スキップ（updated/voices_organized が見つかりません）"
fi

# voices/ 内に残っている、個別フォルダごとの実行ファイル・CSVコピーは
# 冗長なので削除する（正本は docs/List_of_recordings.csv、
# 実行ファイルはリポジトリに含めない方針のため）
if [ -d "voices" ]; then
  find voices -maxdepth 1 -iname "analyze_audio.exe" -delete
  find voices -maxdepth 1 -iname "List_of_recordings.csv" -delete
fi

# ============================================================
# 3. voices_original/ を raw_audio/ としてトップレベルへ
# ============================================================
echo ""
echo "==== 3. voices_original/ を raw_audio/ としてトップレベルへ移動 ===="

if [ -d "updated/voices_original" ]; then
  mv -v updated/voices_original raw_audio
else
  echo "  スキップ（updated/voices_original が見つかりません）"
fi

# ============================================================
# 4. 不要ファイル（実行ファイル・ログ・LaTeX中間生成物）の削除
# ============================================================
echo ""
echo "==== 4. 実行ファイル・ログ・LaTeX中間生成物を削除 ===="

# 実行ファイル（.exe, .o）はどこにあっても削除する
find . -type f \( -iname "*.exe" -o -iname "*.o" \) -print -delete

# ビルド・変換ログ
find . -type f \( \
    -iname "build_log.txt" -o \
    -iname "*_converted.log" -o \
    -iname "ffprobed_*.log" -o \
    -iname "all_wav_converted.log" \
  \) -print -delete

# LaTeXの中間生成物（main.pdf以外）
for ext in aux bbl bcf blg dvi log nav out run.xml snm toc fls fdb_latexmk synctex.gz; do
  find . -maxdepth 1 -type f -iname "main.${ext}" -print -delete
done

# ============================================================
# 5. ソース・ドキュメント・集計結果の振り分け
# ============================================================
echo ""
echo "==== 5. ソースコード・ドキュメント・集計結果を整理 ===="

mkdir -p src docs results

# --- ソースコード ---
if [ -d "updated/programs" ]; then
  find updated/programs -maxdepth 1 -iname "*.cpp" -exec mv -v {} src/ \;
  find updated/programs -maxdepth 1 -iname "*_usage.md" -exec mv -v {} docs/ \;
fi

# トップレベルに既に置かれている可能性のあるソース・ドキュメントも回収する
find . -maxdepth 1 -iname "*.cpp" -exec mv -v {} src/ \; 2>/dev/null || true
find . -maxdepth 1 -iname "*_usage.md" -exec mv -v {} docs/ \; 2>/dev/null || true

# --- 録音一覧CSV（スペース入りファイル名 → アンダースコアに統一） ---
if [ -f "updated/List of recordings.csv" ]; then
  mv -v "updated/List of recordings.csv" "docs/List_of_recordings.csv"
elif [ -f "updated/List_of_recordings.csv" ]; then
  mv -v "updated/List_of_recordings.csv" "docs/List_of_recordings.csv"
fi

# --- 集計・比較用CSV ---
for f in "updated/summarized.csv" "updated/summarized_chord.csv" "edited_chord.csv"; do
  if [ -f "$f" ]; then
    mv -v "$f" results/
  fi
done

# ============================================================
# 6. 空になった updated/ フォルダを削除
# ============================================================
echo ""
echo "==== 6. 空フォルダの後片付け ===="

if [ -d "updated/programs" ] && [ -z "$(ls -A updated/programs 2>/dev/null)" ]; then
  rmdir updated/programs
fi
if [ -d "updated" ] && [ -z "$(ls -A updated 2>/dev/null)" ]; then
  rmdir updated
  echo "  updated/ を削除しました（空になったため）"
else
  if [ -d "updated" ]; then
    echo "  [注意] updated/ にまだファイルが残っています。内容を確認してください:"
    find updated -maxdepth 2 -print
  fi
fi

# ============================================================
# 7. .gitignore の配置
# ============================================================
echo ""
echo "==== 7. .gitignore を配置 ===="

if [ ! -f ".gitignore" ]; then
  cat > .gitignore << 'EOF'
# LaTeX build artifacts
*.aux
*.bbl
*.bcf
*.blg
*.dvi
*.log
*.nav
*.out
*.run.xml
*.snm
*.toc
*.fls
*.fdb_latexmk
*.synctex.gz

# コンパイル済み実行ファイル（ソースがあれば十分なため）
*.exe
*.o

# 作業用の一時ログ
build_log.txt
*_converted.log
ffprobed_*.log
all_wav_converted.log

# OS/エディタ由来のゴミファイル
.DS_Store
Thumbs.db
*.swp

EOF
  echo "  .gitignore を新規作成しました。"
else
  echo "  .gitignore は既に存在するため上書きしません。"
fi

# ============================================================
# 8. 大きいファイル・音声ファイルの警告表示
# ============================================================
echo ""
echo "==== 8. 音声ファイル・大きいファイルの確認 ===="

WAV_COUNT=$(find . -iname "*.wav" -type f | wc -l | tr -d ' ')
M4A_COUNT=$(find . -iname "*.m4a" -type f | wc -l | tr -d ' ')
echo "  .wav ファイル数: $WAV_COUNT"
echo "  .m4a ファイル数: $M4A_COUNT"
if [ "$WAV_COUNT" -gt 0 ]; then
  echo "  [注意] .wav は非圧縮で容量が大きくなりがちです。"
  echo "         全て公開する必要が無ければ、代表的な数個だけ残すことを検討してください。"
fi

# 10MBを超えるファイルがあれば警告（GitHubは1ファイル100MBが上限）
echo "  10MBを超える大きいファイル:"
find . -type f -size +10M -not -path "./.git/*" -exec ls -lh {} \; | awk '{print "    " $9 " (" $5 ")"}'

# ============================================================
# 9. git 初期化
# ============================================================
echo ""
echo "==== 9. git リポジトリの初期化 ===="

if [ -d ".git" ]; then
  echo "  既に git リポジトリが初期化されています（.git が存在）。init はスキップします。"
else
  git init
  echo "  git init 完了。"
fi

# コミット用のユーザー情報が設定されているか確認
if ! git config user.name > /dev/null 2>&1; then
  echo "  [警告] git の user.name が設定されていません。"
  echo "         以下を実行してから再度スクリプトを実行するか、手動で commit してください。"
  echo "           git config --global user.name \"あなたの名前\""
  echo "           git config --global user.email \"you@example.com\""
fi

echo ""
echo "==== 最終的なディレクトリ構成 ===="
find . -maxdepth 2 -not -path "./.git*" | sort

echo ""
echo "==== 完了 ===="
echo "内容を確認し、問題なければ以下を実行してコミットしてください:"
echo "  git add ."
echo "  git commit -m \"Initial commit: 数学で読み解くピアノの音\""
echo ""
echo "GitHubにpushする場合は、先にGitHub上で空のリポジトリを作成した上で:"
echo "  git branch -M main"
echo "  git remote add origin https://github.com/<ユーザー名>/<リポジトリ名>.git"
echo "  git push -u origin main"
