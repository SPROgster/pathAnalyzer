#include "morphology.h"

#include <QColor>
#include <QPainter>
// Структурные элементы

QImage* disk(int radius, QRgb color)
{
    int size = radius * 2 - 1;
    QImage *disk = new QImage(radius * 2 - 1, radius * 2 - 1, QImage::Format_ARGB32);
    disk->fill(Qt::transparent);

    QColor qcolor(color);

    QPainter painter;

    painter.begin(disk);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(qcolor);
    painter.setBrush(QBrush(qcolor));

    painter.drawEllipse(0, 0, size, size);

    painter.end();

    return disk;
}

void dilation(QImage* &origin, const QImage &element, const QRgb pixelColor, const QRgb background)
{
    int width       = origin->width(),
        height      = origin->height(),
        elementSizeX= element.width(),
        elementSizeY= element.height(),
        elementRadX = elementSizeX / 2,
        elementRadY = elementSizeY / 2;

    QImage::Format origFormat = origin->format();
    QVector<QRgb> colorTable;
    if (origFormat == QImage::Format_Indexed8)
        colorTable = origin->colorTable();

    QImage *delationResult = new QImage(width, height, QImage::Format_ARGB32_Premultiplied);
    *origin = origin->convertToFormat(QImage::Format_ARGB32_Premultiplied);
    delationResult->fill(QColor(background));

    QPainter painter;

    painter.begin(delationResult);

    QRgb* pixel = (QRgb*)origin->bits();

    for (int x = 0; x < width; x++)
        for (int y = 0; y < height; y++, pixel++)
        {
            // Проверяем, пиксель
            if (*pixel == pixelColor)
                painter.drawImage(x - elementRadX,
                                  y - elementRadY,
                                  element);
        }

    painter.end();

    if (origFormat == QImage::Format_Indexed8)
        *delationResult = delationResult->convertToFormat(origFormat, colorTable);
    else
        *delationResult = delationResult->convertToFormat(origFormat);

    delete origin;
    origin = delationResult;
}
