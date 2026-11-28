#!/bin/zsh
# Copyright 2026
# Licensed under the same terms as Mozc.
#
# Mozlumi (1.3.4): makes "Mozlumiの記録を読む.app" in ~/Applications. The app
# turns an exported debug log (.mozlumilog) into text when the file is
# double-clicked or dropped on the app. It uses the private key at
# ~/.mozlumi-developer/debug_log_private_key.p256 (made by keygen.swift).
#
#   zsh make_reader_app.zsh [folder for the text]
#
# The text goes to the folder (default: /Volumes/San1T/Mozlumi/debug-logs), or
# next to the log file when the folder is not there. Run this again after
# decrypt.swift changes.

set -euo pipefail

here=${0:A:h}
key="$HOME/.mozlumi-developer/debug_log_private_key.p256"
out_dir="${1:-/Volumes/San1T/Mozlumi/debug-logs}"
app="$HOME/Applications/Mozlumiの記録を読む.app"
lsregister=/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister

if [[ ! -f "$key" ]]; then
  print -u2 -- "The private key is not found: $key"
  exit 1
fi

work=$(/usr/bin/mktemp -d)
trap '/bin/rm -rf "$work"' EXIT

/usr/bin/swiftc -O -o "$work/mozlumi-debug-decrypt" "$here/decrypt.swift"

cat > "$work/reader.applescript" <<'APPLESCRIPT'
on open theFiles
	set decryptPath to POSIX path of (path to resource "mozlumi-debug-decrypt")
	set keyPath to (POSIX path of (path to home folder)) & ".mozlumi-developer/debug_log_private_key.p256"
	set outDir to do shell script "/bin/cat " & quoted form of (POSIX path of (path to resource "output_folder.txt"))
	set madeFiles to {}
	repeat with theFile in theFiles
		set inPath to POSIX path of theFile
		set baseName to do shell script "/usr/bin/basename " & quoted form of inPath & " .mozlumilog"
		set targetDir to outDir
		try
			do shell script "/bin/test -d " & quoted form of outDir
		on error
			set targetDir to do shell script "/usr/bin/dirname " & quoted form of inPath
		end try
		set outPath to targetDir & "/" & baseName & ".txt"
		try
			do shell script quoted form of decryptPath & " " & quoted form of keyPath & " " & quoted form of inPath & " " & quoted form of outPath
			set end of madeFiles to outPath
		on error errorText
			display dialog "読めませんでした。" & return & return & inPath & return & return & errorText with title "Mozlumi" buttons {"OK"} default button "OK" with icon caution
		end try
	end repeat
	if (count of madeFiles) > 0 then
		set resultText to "読めるテキストにしました。" & return & return
		repeat with madeFile in madeFiles
			set resultText to resultText & madeFile & return
		end repeat
		set dialogResult to display dialog resultText with title "Mozlumi" buttons {"Finder で見る", "OK"} default button "OK"
		if button returned of dialogResult is "Finder で見る" then
			do shell script "/usr/bin/open -R " & quoted form of (item 1 of madeFiles)
		end if
	end if
end open

on run
	display dialog "読みたい .mozlumilog ファイルをダブルクリックするか、このアプリのアイコンにドラッグしてください。" with title "Mozlumi" buttons {"OK"} default button "OK"
end run
APPLESCRIPT

/bin/mkdir -p "$HOME/Applications"
/bin/rm -rf "$app"
/usr/bin/osacompile -o "$app" "$work/reader.applescript"
/bin/cp "$work/mozlumi-debug-decrypt" "$app/Contents/Resources/mozlumi-debug-decrypt"
print -r -- "$out_dir" > "$app/Contents/Resources/output_folder.txt"

# Open .mozlumilog files with this app (double-click).
plist="$app/Contents/Info.plist"
/usr/bin/plutil -replace CFBundleIdentifier \
  -string "io.github.curren-lumi-project.mozlumi.debug-log-reader" "$plist"
/usr/bin/plutil -replace CFBundleDocumentTypes -json \
  '[{"CFBundleTypeName":"Mozlumi debug log","CFBundleTypeRole":"Viewer","CFBundleTypeExtensions":["mozlumilog"],"LSHandlerRank":"Owner"}]' \
  "$plist"

# The Info.plist and the bundle changed: sign the app again (ad hoc, for this
# Mac only) and tell Launch Services about it.
/usr/bin/codesign --force --deep --sign - "$app"
"$lsregister" -f "$app"

print -r -- "made: $app"
print -r -- "text folder: $out_dir"
