#pragma once

#include <QStringList>

// Raw files from the same shot: the same name in the same folder with a camera
// raw extension (demo.jpg -> demo.NEF). Full paths, sorted; usually none or one.
QStringList rawCompanions(const QString &path);

// The files in folder that copying or moving files there would replace
QStringList existingTargets(const QStringList &files, const QString &folder);

// Copies or moves files into folder, replacing files of the same name. Stops at
// the first failure and returns the name of the file that failed (empty if all went).
QString transferFiles(const QStringList &files, const QString &folder, bool move);
