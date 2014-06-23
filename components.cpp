#include <QImage>
#include <QColor>

#include "components.h"

QImage* selectComponents(const QImage* origin, int& colorNumber)
{
    QImage bitmap(origin->createMaskFromColor(0xFF000000, Qt::MaskOutColor).convertToFormat(QImage::Format_RGB32));

    QImage* componentsMap = new QImage(bitmap.width(), bitmap.height(), QImage::Format_RGB32);
    componentsMap->fill(QColor(0, 0, 0, 0));

    QRgb currColor = 0xFF000000;

    // Берем 2ю по счету строку и 2й по счету элемент. Граница изображения игнорируется
    QRgb* y1 = (QRgb*)bitmap.scanLine(0);
    QRgb* y2 = (QRgb*)componentsMap->scanLine(0);

    // Список активных маркеров. В последствие, когда будем соединять несколько линий, которая правее будет заменяться
    //на ту, что левее и помечаться в списке неактивной
    QList<bool> componentsActive;

    unsigned int curveNum = 0xFF000000,
                  curveNum1 = 0xFF000000; // В curveNum будет хранится активный номер рядомстоящей точки

    if (*y1 & 0x1)
    {
        *y2 = ++currColor | 0xFF000000;
        componentsActive << true;

        curveNum = curveNum1 = currColor | 0xFF000000;
    }
    y1++;
    y2++;

    int width = bitmap.width();
    // Маркируем первую строку. Смотрим, стоит ли что слева, для этого curveNum пригодилась
    for (int x = 1; x < bitmap.width(); x++, y1++, y2++)
    {
        if (*y1 & 0x1)
        {
            if (curveNum & 0xFFFFFF)
                *y2 = curveNum | 0xFF000000;
            else
            {
                *y2 = ++currColor | 0xFF000000;
                componentsActive << true;

                curveNum = currColor | 0xFF000000;
            }
        }
        else
            curveNum = 0;
    }

    curveNum = curveNum1;

    // Маркируем первый столбец
    for (int y = 1; y < bitmap.height(); y++, y1 += width, y2 += width)
    {
        if (*(y1) & 0x1)
        {
            if (curveNum & 0xFFFFFF)
                *y2 = curveNum | 0xFF000000;
            else
            {
                *y2 = ++currColor | 0xFF000000;
                componentsActive << true;

                curveNum = currColor;
            }
        }
        else
            curveNum = 0;
    }

    // Теперь идем по внутренней части
    QRgb *bitmapPixel;
    bitmapPixel = (QRgb*)bitmap.scanLine(1);

    y1 = (QRgb *)componentsMap->scanLine(0);
    y2 = (QRgb *)componentsMap->scanLine(1);

    for (int y = 1; y < bitmap.height(); y++)
    {
        bitmapPixel++;
        y1++; y2++; // + 1 лишний раз будет делать for по x

        // Смотрим связные элементы по строке
        for (int x = 1; x < width; x++, y1++, y2++, bitmapPixel++)
            // Если пиксель не пустой, проверяем его окружение и относим к какой нибудь из областей
            if ( *bitmapPixel & 0xFF)
            {
                curveNum = 0;

                //Если мы тут, то под нами элемент. Осталось определить, к какой группе его определить

                // _______
                // |*| | |
                // _______
                // | |?| |
                /*curveNum1 = *(y1 - 1) & 0xFFFFFF;
if (curveNum1)
// Если выбранный пиксель пронумерован, то проверяемый соединяем с ним
curveNum = curveNum1;*/

                // _______
                // | |*| |
                // _______
                // | |?| |
                curveNum1 = *y1 & 0xFFFFFF;
                if (curveNum1)
                {
                    /*if (curveNum != curveNum1 && curveNum)
{ // TODO сверху 2 ячейки занумерованы. но теоритически, этот if всегда -
componentsActive.replace(curveNum1 - 1, false);
replaceColor(componentsMap, 0xFF000000 | curveNum1, 0xFF000000 | curveNum);
}
else*/
                        curveNum = curveNum1;
                }

                // _______
                // | | | |
                // _______
                // |*|?| |
                //
                // TODO: теоритически, на это можно наложить ограничение, если сверху слева пусто и еще прямо сверху
                curveNum1 = *(y2 - 1) & 0xFFFFFF;
                if (curveNum1)
                {
                    if (curveNum != curveNum1 && curveNum)
                    { // TODO сверху 2 ячейки занумерованы. но теоритически, этот if всегда -
                        componentsActive.replace(curveNum1 - 1, false);
                        replaceColor(componentsMap, 0xFF000000 | curveNum1, 0xFF000000 | curveNum);
                    }
                    else
                        curveNum = curveNum1;
                }

                // этот элемент может быть более новым, чем выше. чтобы не плодить новые
                // _______
                // | | |*|
                // _______
                // | |?| |
                /*if (x < bitmap.width() - 2)
{
curveNum1 = *(y1 + 1) & 0xFFFFFF;
if (curveNum1)
{
if (curveNum != curveNum1 && curveNum)
{ // TODO сверху 2 ячейки занумерованы. но теоритически, этот if всегда -
componentsActive.replace(curveNum1 - 1, false);
replaceColor(componentsMap, 0xFF000000 | curveNum1, 0xFF000000 | curveNum);
}
else
curveNum = curveNum1;
}
}*/

                // Если сосед есть
                if (curveNum)
                    *y2 = curveNum | 0xFF000000;
                else
                { // Если соседа нету
                    *y2 = ++currColor | 0xFF000000;
                    componentsActive << true;
                }
            }
    }

    colorNumber = 1;

    for (int i = 0; i < componentsActive.size(); i++)
        if (componentsActive[i])
            replaceColor(componentsMap, 0xFF000000 + i + 1, 0xFF000000 + colorNumber++);

    componentsActive.clear();

    for (int i = 0; i < colorNumber; i++)
        componentsActive << true;

    return componentsMap;
}


void replaceColor(QImage *image, const QRgb colorToReplace, const QRgb newColor)
{
    QRgb* xLine = (QRgb*)image->scanLine(0);
    for (int y = 0; y < image->height(); y++)
        for (int x = 0; x < image->width(); x++, xLine++)
            if (*xLine == colorToReplace)
                *xLine = newColor;
}


xy* crop(QImage &object)
{
    int imageWidth = object.width();
    int imageHeight= object.height();

    xy* res = new xy[2];
    res[0].x = imageWidth + 1;
    res[0].y = imageHeight+ 1;
    res[1].x = 0;
    res[1].y = 0;

    uchar* pixel = object.bits();

    for (int y = 0; y < imageHeight; y++)
        for (int x = 0; x < imageWidth; x++, pixel++)
        {
            if (*pixel)
            {
                // Смотрим левый верхний край
                if (y < res[0].y)
                    res[0].y = y;
                if (x < res[0].x)
                    res[0].x = x;

                // Смотрим правый нижний край
                if (y > res[1].y)
                    res[1].y = y;
                if (x > res[1].x)
                    res[1].x = x;
            }
        }

    if (res->x == -1)
    {
        delete [] res;
        return 0;
    }

    return res;
}
