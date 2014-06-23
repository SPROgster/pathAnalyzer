#ifndef COMPONENTS_H
#define COMPONENTS_H

struct xy
{
    int x, y;
};

void replaceColor(QImage *image, const QRgb colorToReplace, const QRgb newColor);
QImage* selectComponents(const QImage* origin, int& colorNumber);
xy *crop(QImage &object);

#endif // COMPONENTS_H
