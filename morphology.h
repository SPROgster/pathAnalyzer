#ifndef MORPHOLOGY_H
#define MORPHOLOGY_H
// Структурные элементы
#include <QImage>

QImage *disk(int radius, QRgb color);
void dilation(QImage* &origin, const QImage &element, const QRgb pixelColor, const QRgb background);
#endif // MORPHOLOGY_H
