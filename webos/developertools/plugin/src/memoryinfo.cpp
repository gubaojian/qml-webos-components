// Copyright (c) 2014-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "memoryinfo.h"

#include <QTextStream>
#include <QFile>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

#include <QDebug>

MemoryInfo::MemoryInfo(QObject* parent)
    : QObject(parent)
    , m_totals(new QQmlPropertyMap)
{
    m_categories << "Size"
        << "Rss"
        << "Pss"
        << "Shared_Clean"
        << "Shared_Dirty"
        << "Private_Clean"
        << "Private_Dirty"
        << "Referenced"
        << "Anonymous"
        << "AnonHugePages"
        << "Swap"
        << "KernelPageSize"
        << "MMUPageSize"
        << "Locked";
    reset();
}

void MemoryInfo::setPid(const QString& pid)
{
    if (m_pid != pid) {
        m_pid = pid;
        emit pidChanged();
    }
}

QQmlListProperty<MemoryInfo::Mapping> MemoryInfo::mappings()
{
    return QQmlListProperty<MemoryInfo::Mapping>(this, &m_requestedMappings, &mappingAppend, &mappingCount, &mappingAt, &mappingClear);
}

void MemoryInfo::reset()
{
    foreach (QString c, m_categories) {
        m_totals->insert(c, 0);
    }
    foreach (Mapping* m, m_requestedMappings) {
        m->resetMap(this);
    }
}

void MemoryInfo::updateTotals()
{
    foreach (QString c, m_categories) {
        foreach (Mapping* m, m_requestedMappings) {
            int ov = m_totals->value(c).toInt();
            int nv = m->m_map->value(c).toInt();

            m_totals->insert(c, ov + nv);
            emit m_totals->valueChanged(c, ov + nv);
        }
    }
}

void MemoryInfo::update()
{
    QString path = "/proc/" + (m_pid.isEmpty() ? "self" : m_pid) + "/smaps";
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Cannot open file %s, for memory reading", qPrintable(file.fileName()));
        return;
    }

    if (!file.isReadable()) {
        qWarning("Unable to read file %s", qPrintable(file.fileName()));
        return;
    }

    QByteArray contents = file.readAll();
    QTextStream in(&contents);

    reset();
    // Read in the new data
    while (!in.atEnd()) {
        QString line = in.readLine();
        foreach (Mapping* m, m_requestedMappings) {
            if (m->match(line)) {
                m->read(this, &in);
                break;
            }
        }
    }
    file.close();
    updateTotals();
}


MemoryInfo::Mapping::Mapping(QObject* parent)
    : QObject(parent)
    , m_memoryInfo(NULL)
    , m_name("")
    , m_map(new QQmlPropertyMap)
{
}

MemoryInfo::Mapping::~Mapping()
{
    delete m_map;
}

void MemoryInfo::Mapping::setName(const QString& name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged();
    }
}

void MemoryInfo::Mapping::read(MemoryInfo* info, QTextStream* in)
{
    for (int i = 0; i < info->m_categories.count() && !in->atEnd(); i++) {
        QString line = in->readLine();
        if (line.endsWith("kB")) {
            QString splitMe = line.left(line.indexOf("kB"));
            QStringList split = splitMe.split(":", QString::SkipEmptyParts);

            QString category = split.at(0).trimmed();
            int value = split.at(1).trimmed().toInt();
            if (info->m_categories.contains(category)) {
                int old = m_map->value(category).toInt();
                m_map->insert(category, old + value);
                emit m_map->valueChanged(category, old + value);
            }
        }
    }
}


bool MemoryInfo::Mapping::match(const QString& line)
{
    QRegularExpression re(m_name);
    QRegularExpressionMatch m = re.match(line);
    return m.isValid() && m.hasMatch();
}

void MemoryInfo::Mapping::resetMap(MemoryInfo* info)
{
    // reset the contents of the map so that the values
    // do not accumulate
    foreach (QString c, info->m_categories) {
        m_map->insert(c, 0);
    }
}
