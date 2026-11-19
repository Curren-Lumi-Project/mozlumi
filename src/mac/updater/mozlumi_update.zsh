#!/bin/zsh
# Copyright 2026 Curren Lumi Project.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Mozlumi updater.
#
# Checks the latest release of Curren-Lumi-Project/mozlumi on GitHub. When a
# newer version is found, asks the user, downloads the PKG, verifies its
# SHA-256 against the ".sha256" file of the same release, and runs the
# installer with the user's administrator approval. No logout is needed.
#
# Usage:
#   mozlumi_update.zsh --auto     Run by the launch agent. At most once a day,
#                                 silent unless a new version is found.
#   mozlumi_update.zsh --manual   Run from the input menu. Always reports.
#
# Release convention: tag "v<major>.<minor>.<patch>-build<N>" with assets
# "Mozlumi_v<version>_build<N>_macos_universal.pkg" and "<that name>.sha256".

emulate -L zsh
setopt no_unset pipe_fail

# The whole script is defined as functions and run from the last line, so that
# replacing this file during an update does not affect the running copy.

readonly REPO='Curren-Lumi-Project/mozlumi'
readonly API_URL="https://api.github.com/repos/${REPO}/releases/latest"
readonly APP='/Library/Input Methods/Mozlumi.app'
readonly PKG_ID='blog.curren-lumi.pkg.Mozlumi'
readonly STATE_DIR="${HOME}/Library/Application Support/Mozlumi"
readonly DISABLED_FILE="${STATE_DIR}/update_check_disabled"
readonly LAST_CHECK_FILE="${STATE_DIR}/update_last_check"
readonly SKIP_FILE="${STATE_DIR}/update_skipped_version"
readonly LOCK_DIR="${STATE_DIR}/update.lock"

typeset -g MODE='--manual'
typeset -g WORK_DIR=''
typeset -g PROGRESS_FILE=''
typeset -g PROGRESS_PID=''
typeset -g CURL_PID=''

cleanup() {
  [[ -n "${CURL_PID}" ]] && kill "${CURL_PID}" 2>/dev/null
  progress_stop
  [[ -n "${WORK_DIR}" && -d "${WORK_DIR}" ]] && /bin/rm -rf "${WORK_DIR}"
  /bin/rmdir "${LOCK_DIR}" 2>/dev/null
}

# dialog <message> <buttons...>: shows a dialog, prints the clicked button.
dialog() {
  local message=$1
  shift
  progress_stop
  /usr/bin/osascript - "${message}" "$@" <<'APPLESCRIPT' 2>/dev/null
on run argv
  set theMessage to item 1 of argv
  set theButtons to items 2 thru -1 of argv
  activate
  try
    set theResult to display dialog theMessage with title "Mozlumi" ¬
      buttons theButtons default button (last item of theButtons)
    return button returned of theResult
  on error number -128
    return ""
  end try
end run
APPLESCRIPT
}

# A small window with a progress bar, shown while updating. It is a separate
# osascript process that follows PROGRESS_FILE, whose content is
# "<fraction 0..1, or -1 for indeterminate><TAB><text>", or "quit".
progress_start() {
  PROGRESS_FILE="${WORK_DIR}/progress"
  print -r -- $'-1\t準備中…' > "${PROGRESS_FILE}"
  /usr/bin/osascript -l JavaScript - "${PROGRESS_FILE}" <<'JAVASCRIPT' >/dev/null 2>&1 &
ObjC.import('Cocoa');
function run(argv) {
  const path = argv[0];
  const app = $.NSApplication.sharedApplication;
  app.setActivationPolicy(1);  // Accessory: no Dock icon.
  const win = $.NSWindow.alloc.initWithContentRectStyleMaskBackingDefer(
      $.NSMakeRect(0, 0, 500, 96), 1 /* titled */, 2 /* buffered */, false);
  win.title = 'Mozlumi';
  win.level = 3;  // Floating.
  const label = $.NSTextField.labelWithString('準備中…');
  label.frame = $.NSMakeRect(20, 56, 460, 20);
  const bar = $.NSProgressIndicator.alloc.initWithFrame(
      $.NSMakeRect(20, 22, 460, 20));
  bar.minValue = 0;
  bar.maxValue = 1;
  bar.indeterminate = true;
  win.contentView.addSubview(label);
  win.contentView.addSubview(bar);
  win.center;
  // Above the center, so that the password prompt does not cover it.
  const frame = win.frame;
  win.setFrameOrigin($.NSMakePoint(frame.origin.x, frame.origin.y + 160));
  win.makeKeyAndOrderFront(null);
  app.activateIgnoringOtherApps(true);
  bar.startAnimation(null);
  let last = '';
  for (let i = 0; i < 36000; i++) {  // At most two hours.
    const content = $.NSString.stringWithContentsOfFileEncodingError(
        path, 4 /* UTF-8 */, null);
    const text = content.isNil() ? 'quit' : content.js.trim();
    if (text === 'quit') {
      break;
    }
    if (text !== last) {
      last = text;
      const tab = text.indexOf('\t');
      const fraction = parseFloat(text.slice(0, tab));
      label.stringValue = text.slice(tab + 1);
      if (fraction >= 0) {
        bar.stopAnimation(null);
        bar.indeterminate = false;
        bar.doubleValue = Math.min(fraction, 1);
      } else {
        bar.indeterminate = true;
        bar.startAnimation(null);
      }
    }
    $.NSRunLoop.currentRunLoop.runUntilDate(
        $.NSDate.dateWithTimeIntervalSinceNow(0.2));
  }
  win.close;
}
JAVASCRIPT
  PROGRESS_PID=$!
}

# progress_set <fraction or -1> <text>
progress_set() {
  [[ -n "${PROGRESS_FILE}" ]] || return 0
  print -r -- "$1"$'\t'"$2" > "${PROGRESS_FILE}.tmp" && \
    /bin/mv -f "${PROGRESS_FILE}.tmp" "${PROGRESS_FILE}"
}

progress_stop() {
  [[ -n "${PROGRESS_FILE}" ]] || return 0
  print -r -- 'quit' > "${PROGRESS_FILE}.tmp" && \
    /bin/mv -f "${PROGRESS_FILE}.tmp" "${PROGRESS_FILE}"
  PROGRESS_FILE=''
  if [[ -n "${PROGRESS_PID}" ]]; then
    local i
    for (( i = 0; i < 10; i++ )); do
      kill -0 "${PROGRESS_PID}" 2>/dev/null || break
      /bin/sleep 0.1
    done
    kill "${PROGRESS_PID}" 2>/dev/null
    PROGRESS_PID=''
  fi
  return 0
}

# download <url> <output> <expected size or 0>: shows the progress meanwhile.
download() {
  local url=$1 output=$2 total=$3 size fraction
  /usr/bin/curl -fsSL --max-time 900 -o "${output}" "${url}" &
  CURL_PID=$!
  while kill -0 "${CURL_PID}" 2>/dev/null; do
    size=$(/usr/bin/stat -f%z "${output}" 2>/dev/null) || size=0
    [[ "${size}" == <-> ]] || size=0
    if (( total > 0 )); then
      fraction=$(printf '%.3f' $(( size * 1.0 / total )))
      progress_set "${fraction}" \
        "ダウンロード中… $(( size / 1048576 )) / $(( total / 1048576 )) MB"
    else
      progress_set -1 "ダウンロード中… $(( size / 1048576 )) MB"
    fi
    /bin/sleep 0.3
  done
  wait "${CURL_PID}"
  local result=$?
  CURL_PID=''
  return ${result}
}

# Reports to the user only in manual mode.
report() {
  [[ "${MODE}" == '--manual' ]] && dialog "$1" 'OK' >/dev/null
  return 0
}

plist_value() {
  /usr/libexec/PlistBuddy -c "Print :$2" "$1" 2>/dev/null
}

json_value() {
  /usr/bin/plutil -extract "$2" raw -o - "$1" 2>/dev/null
}

# "1.2.1" and "9" -> "001002001009" (comparable as a string).
version_key() {
  local -a parts
  parts=("${(@s:.:)1}")
  (( ${#parts} == 3 )) || return 1
  printf '%03d%03d%03d%03d' "${parts[1]}" "${parts[2]}" "${parts[3]}" "$2"
}

main() {
  MODE=${1:---manual}
  /bin/mkdir -p "${STATE_DIR}" || return 1

  if [[ "${MODE}" == '--auto' ]]; then
    [[ -e "${DISABLED_FILE}" ]] && return 0
    # At most once a day.
    if [[ -e "${LAST_CHECK_FILE}" ]] && \
       [[ -n "$(/usr/bin/find "${LAST_CHECK_FILE}" -mtime -1 2>/dev/null)" ]]; then
      return 0
    fi
  fi

  /bin/mkdir "${LOCK_DIR}" 2>/dev/null || {
    # Another check is running. A lock older than a day is stale.
    if [[ -n "$(/usr/bin/find "${LOCK_DIR}" -maxdepth 0 -mtime +0 2>/dev/null)" ]]; then
      /bin/rmdir "${LOCK_DIR}" 2>/dev/null
    fi
    return 0
  }
  trap cleanup EXIT INT TERM

  if [[ ! -d "${APP}" ]]; then
    report 'Mozlumi がインストールされていません。'
    return 1
  fi
  local current_version current_build
  current_version=$(plist_value "${APP}/Contents/Info.plist" CFBundleShortVersionString)
  current_build=$(plist_value "${APP}/Contents/Info.plist" MozlumiBuild)
  [[ "${current_build}" == <-> ]] || current_build=0
  local current_key
  current_key=$(version_key "${current_version}" "${current_build}") || {
    report 'インストール済みの Mozlumi のバージョンを読み取れませんでした。'
    return 1
  }

  WORK_DIR=$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/mozlumi-update.XXXXXXXX") || return 1
  local json="${WORK_DIR}/latest.json"
  if ! /usr/bin/curl -fsSL --max-time 30 \
      -H 'Accept: application/vnd.github+json' \
      -A 'Mozlumi-Updater' -o "${json}" "${API_URL}"; then
    report 'アップデートを確認できませんでした。ネットワークの接続を確認してください。'
    return 1
  fi
  /usr/bin/touch "${LAST_CHECK_FILE}"

  local tag
  tag=$(json_value "${json}" tag_name)
  if [[ ! "${tag}" =~ '^v([0-9]+)\.([0-9]+)\.([0-9]+)(-build([0-9]+))?$' ]]; then
    report 'リリース情報を読み取れませんでした。'
    return 1
  fi
  local new_version="${match[1]}.${match[2]}.${match[3]}"
  local new_build="${match[5]:-0}"
  local new_key
  new_key=$(version_key "${new_version}" "${new_build}") || return 1
  local current_label="${current_version} Build ${current_build}"
  local new_label="${new_version} Build ${new_build}"

  if [[ ! "${new_key}" > "${current_key}" ]]; then
    report "Mozlumi は最新です（${current_label}）。"
    return 0
  fi
  if [[ "${MODE}" == '--auto' && -e "${SKIP_FILE}" ]] && \
     [[ "$(<"${SKIP_FILE}")" == "${tag}" ]]; then
    return 0
  fi

  # Find the PKG and its checksum among the release assets.
  local pkg_name='' pkg_url='' sha_url='' i name
  local pkg_size=0
  local -a names urls sizes
  for (( i = 0; i < 50; i++ )); do
    name=$(json_value "${json}" "assets.${i}.name") || break
    names+=("${name}")
    urls+=("$(json_value "${json}" "assets.${i}.browser_download_url")")
    sizes+=("$(json_value "${json}" "assets.${i}.size")")
  done
  for (( i = 1; i <= ${#names}; i++ )); do
    if [[ "${names[i]}" =~ '^Mozlumi_v[0-9.]+_build[0-9]+_macos_universal\.pkg$' ]]; then
      pkg_name=${names[i]}
      pkg_url=${urls[i]}
      pkg_size=${sizes[i]}
    fi
  done
  for (( i = 1; i <= ${#names}; i++ )); do
    if [[ -n "${pkg_name}" && "${names[i]}" == "${pkg_name}.sha256" ]]; then
      sha_url=${urls[i]}
    fi
  done
  if [[ -z "${pkg_url}" || -z "${sha_url}" ]] || \
     [[ "${pkg_url}" != "https://github.com/${REPO}/releases/download/"* ]] || \
     [[ "${sha_url}" != "https://github.com/${REPO}/releases/download/"* ]]; then
    report "Mozlumi ${new_label} が公開されていますが、更新ファイルが見つかりませんでした。"
    return 1
  fi

  local answer
  answer=$(dialog "Mozlumi ${new_label} が公開されています（今は ${current_label}）。

更新しますか？ ダウンロード後、管理者のパスワードを求められます。ログアウトは不要です。" \
    'このバージョンをスキップ' 'あとで' '更新する')
  case "${answer}" in
    '更新する') ;;
    'このバージョンをスキップ')
      print -r -- "${tag}" > "${SKIP_FILE}"
      return 0
      ;;
    *) return 0 ;;
  esac

  local pkg="${WORK_DIR}/${pkg_name}" sha="${WORK_DIR}/${pkg_name}.sha256"
  [[ "${pkg_size}" == <-> ]] || pkg_size=0
  progress_start
  if ! download "${pkg_url}" "${pkg}" "${pkg_size}" || \
     ! /usr/bin/curl -fsSL --max-time 60 -o "${sha}" "${sha_url}"; then
    dialog 'ダウンロードに失敗しました。あとでもう一度お試しください。' 'OK' >/dev/null
    return 1
  fi
  progress_set -1 'ダウンロードしたファイルを確認しています…'
  local expected actual
  expected=$(/usr/bin/awk 'NR == 1 { print tolower($1) }' "${sha}")
  actual=$(/usr/bin/shasum -a 256 "${pkg}" | /usr/bin/awk '{ print $1 }')
  if [[ ! "${expected}" =~ '^[0-9a-f]{64}$' || "${expected}" != "${actual}" ]]; then
    dialog 'ダウンロードしたファイルの確認（SHA-256）に失敗したため、更新を中止しました。' 'OK' >/dev/null
    return 1
  fi
  # The package must be Mozlumi's.
  if ! /usr/sbin/pkgutil --expand "${pkg}" "${WORK_DIR}/expanded" >/dev/null 2>&1 || \
     ! /usr/bin/grep -q "identifier=\"${PKG_ID}\"" \
       "${WORK_DIR}/expanded/Mozc.pkg/PackageInfo" 2>/dev/null; then
    dialog 'ダウンロードしたファイルが Mozlumi の更新ではないため、更新を中止しました。' 'OK' >/dev/null
    return 1
  fi

  progress_set -1 'インストールしています…（管理者のパスワードを入力してください）'
  # Any process of this user can change the PKG in WORK_DIR until it is
  # installed. So, as root, copy it to a folder only root can write, check the
  # SHA-256 and the package identifier again there, and install that copy.
  local root_script
  root_script=$(/bin/cat <<'ROOTSCRIPT'
set -u
src=$1
expected=$2
id=$3
d=$(/usr/bin/mktemp -d /private/tmp/mozlumi-install.XXXXXXXX) || exit 70
trap '/bin/rm -rf "$d"' EXIT
/bin/cp "$src" "$d/Mozlumi.pkg" || exit 71
actual=$(/usr/bin/shasum -a 256 "$d/Mozlumi.pkg" | /usr/bin/awk '{ print $1 }')
[ "$actual" = "$expected" ] || exit 72
/usr/sbin/pkgutil --expand "$d/Mozlumi.pkg" "$d/expanded" >/dev/null 2>&1 || exit 73
/usr/bin/grep -q "identifier=\"$id\"" "$d/expanded/Mozc.pkg/PackageInfo" || exit 73
/usr/sbin/installer -pkg "$d/Mozlumi.pkg" -target /
ROOTSCRIPT
)
  local result
  result=$(/usr/bin/osascript - "${root_script}" "${pkg}" "${expected}" "${PKG_ID}" <<'APPLESCRIPT' 2>&1
on run argv
  do shell script "/bin/sh -c " & quoted form of (item 1 of argv) & ¬
    " mozlumi-install " & quoted form of (item 2 of argv) & " " & ¬
    quoted form of (item 3 of argv) & " " & quoted form of (item 4 of argv) ¬
    with prompt "Mozlumi を更新します。" with administrator privileges
end run
APPLESCRIPT
)
  local install_status=$?
  progress_stop
  if (( install_status != 0 )); then
    # -128: the user canceled the password prompt.
    [[ "${result}" == *'(-128)'* ]] && return 0
    # 72, 73: the PKG changed between the check and the installation.
    if [[ "${result}" == *'(72)'* || "${result}" == *'(73)'* ]]; then
      dialog 'インストールの直前に確かめたところ、更新ファイルが変わっていたため、更新を中止しました。' 'OK' >/dev/null
      return 1
    fi
    dialog "更新に失敗しました。

${result}" 'OK' >/dev/null
    return 1
  fi
  /bin/rm -f "${SKIP_FILE}"
  dialog "Mozlumi を ${new_label} に更新しました。次に文字を入力したときから新しい版になります。" 'OK' >/dev/null
  return 0
}

main "$@"; exit $?
