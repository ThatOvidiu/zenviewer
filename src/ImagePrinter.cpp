#include "ImagePrinter.h"

#include "ImageLoader.h"

#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPrintPreviewDialog>
#include <QToolBar>

namespace {
// Enough for A3 at 600 dpi; larger pictures are decoded smaller, which saves memory
constexpr int MaxPrintSide = 8000;
const QMarginsF PageMargins(10, 10, 10, 10); // mm; printers may need wider ones
}

ImagePrinter::ImagePrinter()
    : m_printer(QPrinter::HighResolution)
{
    m_printer.setPageMargins(PageMargins, QPageLayout::Millimeter);
    m_printer.setCreator(QStringLiteral("ZenViewer"));
}

QString ImagePrinter::print(const QString &path, QWidget *parent)
{
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const QImage image = decodeImage(path, QSize(MaxPrintSide, MaxPrintSide), &error);
    QGuiApplication::restoreOverrideCursor();
    if (image.isNull())
        return tr("Could not print: %1").arg(error.isEmpty() ? tr("the image did not load") : error);

    const QString name = QFileInfo(path).fileName();
    m_printer.setDocName(name);
    // The paper turned the way the picture is; the preview can still turn it
    m_printer.setPageOrientation(image.width() > image.height() ? QPageLayout::Landscape : QPageLayout::Portrait);

    QPrintPreviewDialog dialog(&m_printer, parent);
    dialog.setWindowTitle(tr("Print Preview - %1").arg(name));
    if (parent)
        dialog.resize(parent->size() * 0.8);
    QObject::connect(&dialog, &QPrintPreviewDialog::paintRequested, &dialog,
                     [&image](QPrinter *printer) { paintPage(printer, image); });

    // The preview has no button of its own for a PDF, and the system's PDF
    // printer (if there is one) is a step further away, behind Print
    QString message;
    if (auto *toolBar = dialog.findChild<QToolBar *>()) {
        toolBar->addSeparator();
        QAction *pdfAct = toolBar->addAction(tr("Save as PDF..."));
        pdfAct->setToolTip(tr("Save the page as a PDF file"));
        QObject::connect(pdfAct, &QAction::triggered, &dialog, [&] {
            message = saveAsPdf(image, path, &dialog);
            if (!message.isEmpty())
                dialog.accept();
        });
    }

    if (dialog.exec() != QDialog::Accepted)
        return {};
    if (!message.isEmpty())
        return message;
    if (m_printer.outputFormat() == QPrinter::PdfFormat || !m_printer.outputFileName().isEmpty())
        return tr("Saved %1").arg(QDir::toNativeSeparators(m_printer.outputFileName()));
    return tr("Sent to %1").arg(m_printer.printerName());
}

QString ImagePrinter::saveAsPdf(const QImage &image, const QString &path, QWidget *parent)
{
    const QFileInfo info(path);
    // The folder of the last PDF in this run, else the image's
    const QString folder = m_pdfFolder.isEmpty() || !QFileInfo(m_pdfFolder).isDir() ? info.absolutePath() : m_pdfFolder;
    const QString suggested = QDir(folder).filePath(info.completeBaseName() + QStringLiteral(".pdf"));
    QString file = QFileDialog::getSaveFileName(parent, tr("Save as PDF"), suggested, tr("PDF files (*.pdf)"));
    if (file.isEmpty())
        return {};
    if (QFileInfo(file).suffix().isEmpty())
        file += QStringLiteral(".pdf");
    m_pdfFolder = QFileInfo(file).absolutePath();

    QPrinter pdf(QPrinter::HighResolution);
    pdf.setOutputFormat(QPrinter::PdfFormat);
    pdf.setOutputFileName(file);
    pdf.setCreator(m_printer.creator());
    pdf.setDocName(m_printer.docName());
    pdf.setPageLayout(m_printer.pageLayout()); // the paper, turn and margins the preview shows
    if (!paintPage(&pdf, image))
        return tr("Could not save %1").arg(QDir::toNativeSeparators(file));
    return tr("Saved %1").arg(QDir::toNativeSeparators(file));
}

bool ImagePrinter::paintPage(QPrinter *printer, const QImage &image)
{
    QPainter painter;
    if (!painter.begin(printer))
        return false;
    // The painter starts at the printable area's corner, inside the margins
    const QSizeF area = printer->pageRect(QPrinter::DevicePixel).size();
    const QSizeF size = QSizeF(image.size()).scaled(area, Qt::KeepAspectRatio);
    const QRectF target(QPointF((area.width() - size.width()) / 2, (area.height() - size.height()) / 2), size);

    // Scaled here rather than by the printer: smoother, and a PDF holds no more pixels than it shows
    const QSize pixels = target.size().toSize();
    const QImage page = image.width() > pixels.width() && pixels.width() > 0
        ? image.scaled(pixels, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        : image;
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target, page);
    return painter.end();
}
