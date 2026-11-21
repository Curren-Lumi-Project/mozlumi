// Copyright 2026 Curren Lumi Project.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Mozlumi: the windows of the Windows updater (win32/updater/
// mozlumi_update.ps1). The script runs
//   mozlumi_tool.exe --mode=update_dialog --update_dialog_type=<type> ...
// and this shows the window in the same look as the other Mozlumi windows.
//
//   question  Asks whether to update. Exits with kExitUpdate, kExitLater or
//             kExitSkip.
//   message   Shows a message with an OK button.
//   progress  Shows a progress window that follows --update_progress_file,
//             whose content is "<fraction 0..1, or -1><TAB><text>" or "quit",
//             until "quit", or until the process --update_parent_pid ends.
//
// In --update_dialog_text, "\n" (a backslash and "n") is a line break.

#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/file_util.h"
#include "gui/base/util.h"

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

ABSL_FLAG(std::string, update_dialog_type, "",
          "Mozlumi updater window: question, message or progress");
ABSL_FLAG(std::string, update_dialog_text, "",
          "Mozlumi updater window: the message (\\n is a line break)");
ABSL_FLAG(std::string, update_progress_file, "",
          "Mozlumi updater window: the file the progress window follows");
ABSL_FLAG(int32_t, update_parent_pid, 0,
          "Mozlumi updater window: closes the progress window when this "
          "process ends");

namespace mozc {
namespace gui {
namespace {

// Must match win32/updater/mozlumi_update.ps1.
constexpr int kExitUpdate = 10;
constexpr int kExitLater = 11;
constexpr int kExitSkip = 12;
constexpr int kExitError = 1;

constexpr int kLogoSize = 64;
constexpr int kTextWidth = 380;

QString FlagText() {
  return QString::fromStdString(absl::GetFlag(FLAGS_update_dialog_text))
      .replace(QStringLiteral("\\n"), QStringLiteral("\n"));
}

QDialog::DialogCode ToDialogCode(int code) {
  return static_cast<QDialog::DialogCode>(code);
}

// The logo and the message, side by side.
QHBoxLayout *NewMessageRow(const QString &text) {
  QHBoxLayout *row = new QHBoxLayout();
  row->setSpacing(16);
  QLabel *logo = new QLabel();
  logo->setPixmap(QPixmap(QStringLiteral(":/update_logo.png"))
                      .scaled(kLogoSize, kLogoSize, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation));
  logo->setAlignment(Qt::AlignTop);
  QLabel *label = new QLabel(text);
  label->setWordWrap(true);
  label->setFixedWidth(kTextWidth);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  row->addWidget(logo, 0, Qt::AlignTop);
  row->addWidget(label, 1);
  return row;
}

void PrepareDialog(QDialog *dialog) {
  dialog->setWindowTitle(QStringLiteral("Mozlumi"));
  dialog->setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint |
                         Qt::WindowStaysOnTopHint | Qt::WindowTitleHint |
                         Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
}

int RunQuestion() {
  QDialog dialog;
  PrepareDialog(&dialog);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(20, 20, 20, 16);
  layout->setSpacing(20);
  layout->addLayout(NewMessageRow(FlagText()));

  QHBoxLayout *buttons = new QHBoxLayout();
  QPushButton *skip =
      new QPushButton(QString::fromUtf8("このバージョンをスキップ"));
  QPushButton *later = new QPushButton(QString::fromUtf8("あとで"));
  QPushButton *update = new QPushButton(QString::fromUtf8("更新する"));
  update->setDefault(true);
  buttons->addWidget(skip);
  buttons->addStretch(1);
  buttons->addWidget(later);
  buttons->addWidget(update);
  layout->addLayout(buttons);

  QObject::connect(skip, &QPushButton::clicked,
                   [&dialog] { dialog.done(ToDialogCode(kExitSkip)); });
  QObject::connect(later, &QPushButton::clicked,
                   [&dialog] { dialog.done(ToDialogCode(kExitLater)); });
  QObject::connect(update, &QPushButton::clicked,
                   [&dialog] { dialog.done(ToDialogCode(kExitUpdate)); });

  dialog.adjustSize();
  dialog.raise();
  dialog.activateWindow();
  const int result = dialog.exec();
  // Closing the window (or Esc) means "later".
  return (result == kExitUpdate || result == kExitSkip) ? result : kExitLater;
}

int RunMessage() {
  QDialog dialog;
  PrepareDialog(&dialog);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(20, 20, 20, 16);
  layout->setSpacing(20);
  layout->addLayout(NewMessageRow(FlagText()));

  QHBoxLayout *buttons = new QHBoxLayout();
  QPushButton *ok = new QPushButton(QStringLiteral("OK"));
  ok->setDefault(true);
  buttons->addStretch(1);
  buttons->addWidget(ok);
  layout->addLayout(buttons);
  QObject::connect(ok, &QPushButton::clicked, [&dialog] { dialog.accept(); });

  dialog.adjustSize();
  dialog.raise();
  dialog.activateWindow();
  dialog.exec();
  return 0;
}

// A progress window that the user cannot close; the updater closes it.
class ProgressDialog : public QDialog {
 public:
  void Finish() {
    finishing_ = true;
    done(QDialog::Accepted);
  }

 protected:
  void reject() override {}  // Esc does nothing.
  void closeEvent(QCloseEvent *event) override {
    if (!finishing_) {
      event->ignore();
    }
  }

 private:
  bool finishing_ = false;
};

int RunProgress() {
  const std::string path = absl::GetFlag(FLAGS_update_progress_file);
  if (path.empty()) {
    return kExitError;
  }
#ifdef _WIN32
  HANDLE parent = nullptr;
  if (absl::GetFlag(FLAGS_update_parent_pid) > 0) {
    parent = ::OpenProcess(
        SYNCHRONIZE, FALSE,
        static_cast<DWORD>(absl::GetFlag(FLAGS_update_parent_pid)));
  }
#endif  // _WIN32

  ProgressDialog dialog;
  dialog.setWindowTitle(QStringLiteral("Mozlumi"));
  dialog.setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint |
                        Qt::WindowStaysOnTopHint | Qt::WindowTitleHint |
                        Qt::CustomizeWindowHint);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(20, 20, 20, 20);
  layout->setSpacing(12);
  QHBoxLayout *row = new QHBoxLayout();
  row->setSpacing(16);
  QLabel *logo = new QLabel();
  logo->setPixmap(QPixmap(QStringLiteral(":/update_logo.png"))
                      .scaled(48, 48, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation));
  QVBoxLayout *right = new QVBoxLayout();
  QLabel *label = new QLabel(QString::fromUtf8("準備中…"));
  label->setFixedWidth(kTextWidth);
  QProgressBar *bar = new QProgressBar();
  bar->setFixedWidth(kTextWidth);
  bar->setTextVisible(false);
  bar->setRange(0, 0);  // Busy until the first fraction.
  right->addWidget(label);
  right->addWidget(bar);
  row->addWidget(logo, 0, Qt::AlignTop);
  row->addLayout(right, 1);
  layout->addLayout(row);

  QTimer timer;
  QObject::connect(&timer, &QTimer::timeout, [&] {
#ifdef _WIN32
    if (parent != nullptr && ::WaitForSingleObject(parent, 0) == WAIT_OBJECT_0) {
      dialog.Finish();
      return;
    }
#endif  // _WIN32
    // The updater replaces the file as a whole; a failed read is retried.
    const absl::StatusOr<std::string> content = FileUtil::GetContents(path);
    if (!content.ok()) {
      return;
    }
    const absl::string_view line =
        absl::StripTrailingAsciiWhitespace(*content);
    if (line == "quit") {
      dialog.Finish();
      return;
    }
    const std::pair<absl::string_view, absl::string_view> parts =
        absl::StrSplit(line, absl::MaxSplits('\t', 1));
    double fraction = -1;
    if (!absl::SimpleAtod(parts.first, &fraction)) {
      return;
    }
    label->setText(QString::fromUtf8(parts.second.data(),
                                     static_cast<int>(parts.second.size())));
    if (fraction < 0) {
      bar->setRange(0, 0);
    } else {
      bar->setRange(0, 1000);
      bar->setValue(static_cast<int>((fraction > 1 ? 1 : fraction) * 1000));
    }
  });
  timer.start(200);

  dialog.adjustSize();
  dialog.raise();
  dialog.exec();
#ifdef _WIN32
  if (parent != nullptr) {
    ::CloseHandle(parent);
  }
#endif  // _WIN32
  return 0;
}

}  // namespace
}  // namespace gui
}  // namespace mozc

int RunUpdateDialog(int argc, char *argv[]) {
  Q_INIT_RESOURCE(qrc_update_dialog);
  auto app = mozc::gui::GuiUtil::InitQt(argc, argv);
  const std::string type = absl::GetFlag(FLAGS_update_dialog_type);
  if (type == "question") {
    return mozc::gui::RunQuestion();
  }
  if (type == "message") {
    return mozc::gui::RunMessage();
  }
  if (type == "progress") {
    return mozc::gui::RunProgress();
  }
  return mozc::gui::kExitError;
}
