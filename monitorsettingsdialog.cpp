/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2013  <copyright holder> <email>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "monitorsettingsdialog.h"
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QComboBox>
#include <QProcess>

#include <QGroupBox>
#include <QMessageBox>
#include "ui_monitor.h"

#include <glib.h>
#include <string.h>

struct Monitor {
  char* name;
  GSList* modeLines;
  short currentMode;
  short currentRate;
  short preferredMode;
  short preferredRate;

  QCheckBox* enable;
  QComboBox* resolutionCombo;
  QComboBox* rateCombo;
};

MonitorSettingsDialog::MonitorSettingsDialog():
  QDialog(NULL, 0),
  monitors(NULL),
  LVDS(NULL) {
  setupUi();
}

void monitor_free(Monitor* monitor) {
  g_free(monitor->name);
  g_slist_free(monitor->modeLines);
  g_free(monitor);
}

MonitorSettingsDialog::~MonitorSettingsDialog() {
  g_slist_foreach(monitors, (GFunc)monitor_free, NULL);
  g_slist_free(monitors);
}

QString MonitorSettingsDialog::humanReadableName(Monitor* monitor) {
  if(monitor == LVDS)
    return tr("Laptop LCD Monitor");
  else if(g_str_has_prefix(monitor->name, "VGA") || g_str_has_prefix(monitor->name, "Analog"))
    return tr(LVDS ? "External VGA Monitor" : "VGA Monitor");
  else if(g_str_has_prefix(monitor->name, "DVI") || g_str_has_prefix(monitor->name, "TMDS") || g_str_has_prefix(monitor->name, "Digital") || g_str_has_prefix(monitor->name, "LVDS"))
    return tr(LVDS ? "External DVI Monitor" : "DVI Monitor");
  else if(g_str_has_prefix(monitor->name, "TV") || g_str_has_prefix(monitor->name, "S-Video"))
    return tr("TV");
  else if(strcmp(monitor->name, "default") == 0)
    return tr("Default Monitor");

  return QString(monitor->name);
}

bool MonitorSettingsDialog::getXRandRInfo() {
  // execute xrandr command and read its output
  QProcess process;
  // set locale to "C" guarantee English output of xrandr
  process.processEnvironment().insert("LC_ALL", "c");
  process.start("xrandr");
  process.waitForFinished(-1);
  if(process.exitCode() != 0)
    return false;

  QByteArray output = process.readAllStandardOutput();

  GRegex* regex = g_regex_new("([a-zA-Z]+[-0-9]*) +connected .*((\n +[0-9]+x[0-9]+[^\n]+)+)",
                      GRegexCompileFlags(0), GRegexMatchFlags(0), NULL);
  GMatchInfo* match;
  if(g_regex_match(regex, output.constData(), GRegexMatchFlags(0), &match)) {
    do {
      Monitor* monitor = g_new0(Monitor, 1);
      char* modes = g_match_info_fetch(match, 2);
      char** lines, **line;
      int imode = 0;

      monitor->currentMode = monitor->currentRate = -1;
      monitor->preferredMode = monitor->preferredRate = -1;
      monitor->name = g_match_info_fetch(match, 1);

      // check if this is the built-in LCD of laptop
      if(! LVDS && (strcmp(monitor->name, "LVDS") == 0 || strcmp(monitor->name, "PANEL") == 0))
        LVDS = monitor;

      lines = g_strsplit(modes, "\n", -1);

      for(line = lines; *line; ++line) {
        char* str = strtok(*line, " ");
        int irate = 0;
        GPtrArray* strv;

        if(! str)
          continue;

        strv = g_ptr_array_sized_new(8);
        g_ptr_array_add(strv, g_strdup(str));

        while(str = strtok(NULL, " ")) {
          if(*str) {
            char* star, *plus;
            str = g_strdup(str);

            // sometimes, + goes after a space
            if(0 == strcmp(str, "+"))
              --irate;
            else
              g_ptr_array_add(strv, str);

            if(star = strchr(str, '*')) {
              monitor->currentMode = imode;
              monitor->currentRate = irate;
            }

            if(plus = strchr(str, '+')) {
              monitor->preferredMode = imode;
              monitor->preferredRate = irate;
            }

            if(star)
              *star = '\0';

            if(plus)
              *plus = '\0';

            ++irate;
          }
        }

        g_ptr_array_add(strv, NULL);
        monitor->modeLines = g_slist_append(monitor->modeLines, g_ptr_array_free(strv, FALSE));
        ++imode;
      }

      g_strfreev(lines);
      g_free(modes);
      monitors = g_slist_prepend(monitors, monitor);
    }
    while(g_match_info_next(match, NULL));

    g_match_info_free(match);
  }
  g_regex_unref(regex);
  return true;
}

void MonitorSettingsDialog::onResolutionChanged(int index) {
  QComboBox* combo = static_cast<QComboBox*>(sender());
  Monitor* monitor = reinterpret_cast<Monitor*>(qVariantValue<void*>(combo->property("monitor")));
  char** rate;
  int sel = combo->currentIndex();
  char** mode_line = reinterpret_cast<char**>(g_slist_nth_data(monitor->modeLines, sel - 1));
  monitor->rateCombo->clear();
  monitor->rateCombo->addItem(tr("Auto"));
  if(sel >= 0 && mode_line && *mode_line) {
    for(rate = mode_line + 1; *rate; ++rate)
      monitor->rateCombo->addItem(*rate);
  }
  monitor->rateCombo->setCurrentIndex(0);
}

void MonitorSettingsDialog::setXRandRInfo() {
  GSList* l;
  QByteArray cmd = "xrandr";

  for(l = monitors; l; l = l->next) {
    Monitor* monitor = (Monitor*)l->data;
    cmd += " --output ";
    cmd.append(monitor->name);
    cmd.append(' ');

    // if the monitor is turned on
    if(monitor->enable->isChecked()) {
      int sel_res = monitor->resolutionCombo->currentIndex();
      int sel_rate = monitor->rateCombo->currentIndex();

      if(sel_res < 1)   // auto resolution
        cmd.append("--auto");
      else {
        cmd.append("--mode ");
        ++sel_res;  // the fist item in the combo box is "Auto", indecis of resolutions are 1, 2, 3...
        cmd.append(monitor->resolutionCombo->currentText());

        if(sel_rate >= 1) { // not auto refresh rate
          cmd.append(" --rate ");
          cmd.append(monitor->resolutionCombo->currentText());
        }
      }
    }
    else    // turn off
      cmd.append("--off");
  }
  
  QProcess process;
  process.start(cmd);
  process.waitForFinished();
}

void MonitorSettingsDialog::chooseMaxResolution(Monitor* monitor) {
  if(monitor->resolutionCombo->count() > 1)
    monitor->resolutionCombo->setCurrentIndex(1);
}

// turn on both laptop LCD and the external monitor
void MonitorSettingsDialog::onUseBoth() {
  for(GSList* l = monitors; l; l = l->next) {
    Monitor* monitor = (Monitor*)l->data;
    chooseMaxResolution(monitor);
    monitor->enable->setChecked(true);
  }
  accept();
}

// external monitor only
void MonitorSettingsDialog::onExternalOnly() {
  for(GSList* l = monitors; l; l = l->next) {
    Monitor* monitor = (Monitor*)l->data;
    chooseMaxResolution(monitor);
    monitor->enable->setChecked(monitor != LVDS);
  }
  accept();
}

// laptop panel - LVDS only
void MonitorSettingsDialog::onLaptopOnly() {
  for(GSList* l = monitors; l; l = l->next) {
    Monitor* monitor = (Monitor*)l->data;
    chooseMaxResolution(monitor);
    monitor->enable->setChecked(monitor == LVDS);
  }
  accept();
}

void MonitorSettingsDialog::setupUi() {
  ui.setupUi(this);
  connect(ui.useBoth, SIGNAL(clicked(bool)), SLOT(onUseBoth()));
  connect(ui.externalOnly, SIGNAL(clicked(bool)), SLOT(onExternalOnly()));
  connect(ui.laptopOnly, SIGNAL(clicked(bool)), SLOT(onLaptopOnly()));

  connect(ui.buttonBox, SIGNAL(clicked(QAbstractButton*)), SLOT(onDialogButtonClicked(QAbstractButton*)));
  aboutButton = new QPushButton(ui.buttonBox);
  aboutButton->setText(tr("About"));
  ui.buttonBox->addButton(aboutButton, QDialogButtonBox::HelpRole);

  getXRandRInfo();

  // If this is a laptop and there is an external monitor, offer quick options
  if(LVDS && g_slist_length(monitors) == 2)
    ui.tabWidget->setCurrentIndex(0);
  else {
    ui.tabWidget->removeTab(0);
  }

  int i = 0;
  GSList* l;
  for(l = monitors; l; l = l->next) {
    Monitor* monitor = (Monitor*)l->data;
    GSList* mode_line;

    QGroupBox* box = new QGroupBox(this);
    QString title = QString("Monitor %1: %2 (%3)")
        .arg(i + 1)
        .arg(QString::fromUtf8(monitor->name))
        .arg(humanReadableName(monitor));

    box->setTitle(title);
    Ui::MonitorWidget mui = Ui::MonitorWidget();
    mui.setupUi(box);
    ui.monitorLayout->insertWidget(ui.monitorLayout->count() - 1, box);
    ui.monitorLayout->setStretchFactor(box, 0);

    monitor->enable = mui.enabled;
    monitor->resolutionCombo = mui.resolution;
    monitor->resolutionCombo->setProperty("monitor", qVariantFromValue<void*>(monitor));
    monitor->rateCombo = mui.rate;

    // turn off screen is not allowed since there should be at least one monitor available.
    if(g_slist_length(monitors) == 1)
      monitor->enable->setEnabled(false);

    if(monitor->currentMode >= 0)
      monitor->enable->setChecked(true);

    connect(monitor->resolutionCombo, SIGNAL(currentIndexChanged(int)), SLOT(onResolutionChanged(int)));
    monitor->resolutionCombo->addItem(tr("Auto"));

    for(mode_line = monitor->modeLines; mode_line; mode_line = mode_line->next) {
      char** strv = (char**)mode_line->data;
      monitor->resolutionCombo->addItem(strv[0]);
    }
    monitor->resolutionCombo->setCurrentIndex(monitor->currentMode + 1);
    monitor->rateCombo->setCurrentIndex(monitor->currentRate + 1);
    ++i;
  }
}

void MonitorSettingsDialog::accept() {
  setXRandRInfo();
  QDialog::accept();
}

void MonitorSettingsDialog::onDialogButtonClicked(QAbstractButton* button) {
  if(ui.buttonBox->buttonRole(button) == QDialogButtonBox::ApplyRole)
    setXRandRInfo();
  else if(button == aboutButton) {
    // about dialog
    QMessageBox::about(this, tr("About"), tr("LXRandR-Qt\n\nMonitor configuration tool for LXDE."));
  }
}

