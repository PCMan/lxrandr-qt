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

#include <QStandardItemModel>
#include <QStandardItem>

#include "ui_monitor.h"

#include <glib.h>
#include <string.h>
#include <stdio.h>

#include "randr.h"

MonitorSettingsDialog::MonitorSettingsDialog():
  QDialog(NULL, 0),
  monitors(),
  LVDS(NULL) {
  setupUi();
}


MonitorSettingsDialog::~MonitorSettingsDialog() {
  while (!monitors.isEmpty())
    delete monitors.takeFirst();
}

QString MonitorSettingsDialog::humanReadableName(Monitor* monitor) {
  if(monitor == LVDS)
    return tr("Laptop LCD Monitor");
  else if( monitor->name.startsWith("VGA") || monitor->name.startsWith("Analog") )
    return tr(LVDS ? "External VGA Monitor" : "VGA Monitor");
  else if( monitor->name.startsWith("DVI") || monitor->name.startsWith("TMDS") || monitor->name.startsWith("Digital") || monitor->name.startsWith("LVDS") )
    return tr(LVDS ? "External DVI Monitor" : "DVI Monitor");
  else if( monitor->name.startsWith("TV") || monitor->name.startsWith("S-Video") )
    return tr("TV");
  else if( monitor->name=="default" )
    return tr("Default Monitor");

  return monitor->name;
}

bool MonitorSettingsDialog::getXRandRInfo() {
  monitors = get_monitors_info();
  
  // check if this is the built-in LCD of laptop
  for(int i=0; i<monitors.size(); i++)
  {
    Monitor *monitor = monitors[i];
    if( !LVDS && ( monitor->name.startsWith("LVSD") || monitor->name.startsWith("PANEL") ) )
    {
      LVDS = monitor;
      break;
    }
  }
  
  return true;
}

void MonitorSettingsDialog::onResolutionChanged(int index) {
  QComboBox* combo = static_cast<QComboBox*>(sender());
  Monitor* monitor = reinterpret_cast<Monitor*>(qVariantValue<void*>(combo->property("monitor")));
  char** rate;
  QStandardItemModel *model = (QStandardItemModel *) combo->model();
  int sel = model->item(combo->currentIndex())->data().toInt();
  //printf("MonitorSettingsDialog::onResolutionChanged sel = %d\n", sel);
  monitor->rateCombo->clear();
  monitor->rateCombo->addItem(tr("Auto"));
  if( sel >= 0 ) {
    ModeLine mode_line = monitor->modeLines[sel];
    //printf("scale = %s\n", mode_line.scale.toAscii().data());
    for (int i = 0; i < mode_line.rates.size(); ++i)
      monitor->rateCombo->addItem(mode_line.rates[i]);
  }
  monitor->rateCombo->setCurrentIndex(0);
  QString mode = combo->currentText();
  if(mode.endsWith("*"))
    monitor->panning->setEnabled(true);
  else
    monitor->panning->setEnabled(false);
}

void MonitorSettingsDialog::setXRandRInfo() {
  Monitor* l;
  QByteArray cmd = "xrandr";

  for(int i=0; i<monitors.size(); ++i) {
    Monitor* monitor = monitors[i];
    cmd += " --output ";
    cmd.append(monitor->name);
    cmd.append(' ');

    // if the monitor is turned on
    if(monitor->enable->isChecked()) {
      int sel_res = monitor->resolutionCombo->currentIndex(); // the fist item in the combo box is "Auto", indecis of resolutions are 1, 2, 3...
      int sel_rate = monitor->rateCombo->currentIndex();

      if(sel_res < 1)   // auto resolution
        cmd.append("--auto");
      else {
        QStandardItemModel *model = (QStandardItemModel *) monitor->resolutionCombo->model();
        ModeLine modeLine = monitor->modeLines[ model->item(sel_res)->data().toInt() ];
        QString mode = monitor->resolutionCombo->currentText();
        bool mode_virtual_ok = mode.endsWith("*");
        if(mode_virtual_ok)
          mode = mode.replace(QString("*"), QString());
        else
        {
          cmd.append("--mode ");
          cmd.append(mode);
        }
        
        cmd.append(" --panning ");
        cmd.append(mode);
        
        if(mode_virtual_ok)
        {
          cmd.append(" --fb ");
          cmd.append(mode);
        }
        
        cmd.append(" --scale ");
        if( mode_virtual_ok && monitor->panning->checkState()==Qt::Checked )
          cmd.append("1.0x1.0");
        else
          cmd.append(modeLine.scale);

        if(sel_rate >= 1) { // not auto refresh rate
          cmd.append(" --rate ");
          cmd.append(monitor->resolutionCombo->currentText());
        }
      }
      
      cmd.append(" --brightness ");
      cmd.append(QString().setNum((float)monitor->brightness_slider->value()/100.0));
    }
    else    // turn off
      cmd.append("--off");
  }
  
  QProcess process;
  process.start(cmd);
  process.waitForFinished();
  printf("%s\n", cmd.constData() );
}

void MonitorSettingsDialog::chooseMaxResolution(Monitor* monitor) {
  if(monitor->resolutionCombo->count() > 1)
    monitor->resolutionCombo->setCurrentIndex(1);
}

// turn on both laptop LCD and the external monitor
void MonitorSettingsDialog::onUseBoth() {
  for(int i=0; i<monitors.size(); ++i) {
    Monitor* monitor = monitors[i];
    chooseMaxResolution(monitor);
    monitor->enable->setChecked(true);
  }
  accept();
}

// external monitor only
void MonitorSettingsDialog::onExternalOnly() {
  for(int i=0; i<monitors.size(); ++i) {
    Monitor* monitor = monitors[i];
    chooseMaxResolution(monitor);
    monitor->enable->setChecked(monitor != LVDS);
  }
  accept();
}

// laptop panel - LVDS only
void MonitorSettingsDialog::onLaptopOnly() {
  for(int i=0; i<monitors.size(); ++i) {
    Monitor* monitor = monitors[i];
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
  if(LVDS && monitors.size() == 2)
    ui.tabWidget->setCurrentIndex(0);
  else {
    ui.tabWidget->removeTab(0);
  }

  for(int i=0; i<monitors.size(); ++i) {
    Monitor* monitor = monitors[i];

    QGroupBox* box = new QGroupBox(this);
    QString title = QString("Monitor %1: %2 (%3)")
        .arg(i + 1)
        .arg(monitor->name)
        .arg(humanReadableName(monitor));
    printf("%s\n", title.toAscii().data());
    box->setTitle(title);
    Ui::MonitorWidget mui = Ui::MonitorWidget();
    mui.setupUi(box);
    ui.monitorLayout->insertWidget(ui.monitorLayout->count() - 1, box);
    ui.monitorLayout->setStretchFactor(box, 0);

    monitor->enable = mui.enabled;
    monitor->resolutionCombo = mui.resolution;
    monitor->resolutionCombo->setProperty("monitor", qVariantFromValue<void*>(monitor));
    monitor->rateCombo = mui.rate;
    monitor->panning = mui.panning;
    monitor->brightness_slider = mui.brightness;

    // turn off screen is not allowed since there should be at least one monitor available.
    if(monitors.size() == 1)
      monitor->enable->setEnabled(false);

    if(monitor->currentMode >= 0)
      monitor->enable->setChecked(true);

    connect(monitor->resolutionCombo, SIGNAL(currentIndexChanged(int)), SLOT(onResolutionChanged(int)));
    
    QStandardItemModel *model=new QStandardItemModel(monitor->modeLines.size()+1,1);
    QStandardItem *item = new QStandardItem(QString(tr("Auto")));
    item->setData(QVariant(-1));
    model->setItem(0, 0, item);
    for(int j=0; j<monitor->modeLines.size(); j++)
    {
      item = new QStandardItem(monitor->modeLines[j].modeline);
      item->setData(QVariant(j)); //Stores index of modeLine
      model->setItem(j+1, 0, item);
    }
    monitor->resolutionCombo->setModel(model);
    
    monitor->brightness_slider->setValue((int)(monitor->brightness*100.0));
    
    /*
    monitor->resolutionCombo->addItem(tr("Auto"));

    for(int j=0; j<monitor->modeLines.size(); j++) {
      QString strv = monitor->modeLines[j].modeline;
      monitor->resolutionCombo->addItem(strv);
    }
    */
    
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

