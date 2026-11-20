#pragma once

#include <QCoreApplication>
#include <QPrinter>
#include <QString>

class QImage;
class QWidget;

// Prints the current image: a print preview first, from which it goes to a
// printer or into a PDF file. Keeps the chosen printer and paper for the
// rest of the session.
class ImagePrinter
{
    Q_DECLARE_TR_FUNCTIONS(ImagePrinter)

public:
    ImagePrinter();

    // Shows the preview of the image at path. Returns a line saying what was
    // done, for a toast; empty if the user cancelled.
    QString print(const QString &path, QWidget *parent);

private:
    // False if the printer could not be opened (a PDF file that can't be written, say)
    static bool paintPage(QPrinter *printer, const QImage &image);
    QString saveAsPdf(const QImage &image, const QString &path, QWidget *parent);

    QPrinter m_printer;
    QString m_pdfFolder; // where the last PDF went, while the app runs
};
