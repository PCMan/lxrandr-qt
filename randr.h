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


#ifndef randr_H
#define randr_H

#include <QString>
#include <QList>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>

struct ModeLine {
  QString modeline;
  QString scale;
  QList<QString> rates;
};

struct Monitor {
  QString name;
  QList<ModeLine> modeLines;
  QHash<QString, QList<QString> > modes_hash;
  short currentMode;
  short currentRate;
  short preferredMode;
  short preferredRate;
  float brightness, red, blue, green;

  QCheckBox* enable;
  QComboBox* resolutionCombo;
  QComboBox* rateCombo;
  QCheckBox* panning;
  QSlider* brightness_slider;
};

QList<Monitor*> get_monitors_info();

#endif // randr_H