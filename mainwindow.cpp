#include <cmath>

#include <QFileDialog>
#include <QProgressDialog>
#include <QDateTime>
#include <QTest>
#include <QMessageBox>
#include <QPainter>
#include <QQueue>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "components.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->buttonLoad,     SIGNAL(clicked()), this, SLOT(openImages()));
    connect(ui->buttonClear,    SIGNAL(clicked()), this, SLOT(clearImageList()));
    connect(ui->buttonPlay,     SIGNAL(clicked()), this, SLOT(play()));
    connect(ui->buttonLearn,    SIGNAL(clicked()), this, SLOT(learn()));
    connect(ui->buttonRecognize,SIGNAL(clicked()), this, SLOT(recognize()));

    connect(ui->listItem,       SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(itemClicked(QListWidgetItem*)));

    connect(ui->spinSigmaMax,   SIGNAL(valueChanged(double)), this, SLOT(spinSigmaMinChanged(double)));

    sigmamin = 5;
}

MainWindow::~MainWindow()
{
    clearLists();
    delete ui;
}

void MainWindow::openImages()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter(tr("Images (*.png *.xpm *.jpg *.jpeg *.bmp)"));
    dialog.setViewMode(QFileDialog::List);

    QProgressDialog progress("Открытие файлов", "Остановить", 0, 1, this);
    progress.setWindowTitle("Открытие последовательности изображений");
    progress.setWindowModality(Qt::WindowModal);

    QStringList fileNames;
    if (dialog.exec())
    {
        fileNames = dialog.selectedFiles();
        progress.setMaximum(fileNames.size());
        imageList.reserve(fileNames.size());
        QString iter;
        int i = 0;
        foreach (iter, fileNames)
        {
            if (i % 10 == 0)
                progress.setValue(i);
            QImage* image = new QImage;
            if (image->load(iter))
            {
                //convertToGrayscale(*image);
                *image = image->convertToFormat(QImage::Format_RGB32);
                if (i == 0)
                    pixelCount = image->byteCount() / 4;

                QListWidgetItem *newItem = new QListWidgetItem;
                newItem->setText(QString("frame %1").arg(i));
                imageList << image;
                newItem->setIcon(QIcon(QPixmap::fromImage(image->scaled(100, 100))));
                ui->listItem->insertItem(i++, newItem);
            }

            if (progress.wasCanceled())
                break;
        }
    }
}

void MainWindow::clearImageList()
{
    ui->listItem->clear();
    clearLists();
}

void MainWindow::play()
{
    playImages(imageList);
}

void MainWindow::recognize()
{
    substractBackground();
    getCentresOfMass();
    applyBorders();
    foreach (QImage* iter, imageList)
    {
        delete iter;
    }
    imageList.clear();
    imageList = imagesWithMasks;
    imagesWithMasks.clear();
    playImages(imageList);
}

void MainWindow::itemClicked(QListWidgetItem *item)
{
    int index = ui->listItem->row(item);

    ui->imageView->setPixmap(QPixmap::fromImage(*(imageList.at(index))));
}

void MainWindow::spinSigmaMinChanged(double newValue)
{
    sigmamin = (float)newValue;
}

void MainWindow::playImages(QList<QImage*>& pixmap)
{
    QProgressDialog progress("воспроизведение", "Остановить", 0, pixmap.size(), this);
    //progress.setWindowModality(Qt::WindowModal);
    progress.setValue(0);
    progress.open();
    int i = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 waitTime;
    foreach (QImage *iter, pixmap)
    {
        now += fps;
        progress.setValue(i++);
        ui->imageView->setPixmap(QPixmap::fromImage(*iter));
        if (progress.wasCanceled())
            break;
        waitTime = now - QDateTime::currentMSecsSinceEpoch();
        QTest::qWait((waitTime > fps) ? 0 : waitTime);
    }
}

void MainWindow::learn()
{
    QList<QListWidgetItem *> selection = ui->listItem->selectedItems();
    int images = selection.size();
    if (images == 0)
    {
        QMessageBox(QMessageBox::Critical, "Ошибка обучения", "Не выбраны элементы для обучения").exec();
        return;
    }

    fillBackg(pixelCount);

    QProgressDialog progress("Обучение", "Остановить", 0, images, this);
    progress.setWindowTitle("Обучение фоновыми изображениями");
    progress.setWindowModality(Qt::WindowModal);

    int i = 0;
    foreach (QListWidgetItem *iter, selection)
    {
        progress.setValue(i++);

        QImage* image = imageList.at(ui->listItem->row(iter));

        // Добавление точек
        QRgb* pixel = (QRgb*)image->bits();
        for (int j = 0; j < pixelCount; j++, pixel++)
            backg[j]->addItem(*pixel);

        if (progress.wasCanceled())
            break;
    }
    for (int j = 0; j < pixelCount; j++)
        backg[j]->finalize();

    QMessageBox(QMessageBox::Information, "Обучение", "Обучение завершено").exec();
}

void MainWindow::substractBackground()
{
    if (backg.size() == 0)
    {
        return;
    }

    QImage* firstFrame = imageList.first();

    QVector<QRgb> maskColorTable;
    maskColorTable << 0xFF000000;
    maskColorTable << 0xFFFFFFFF;

    int width = firstFrame->width();
    int height= firstFrame->height();

    QImage* mask = new QImage(width, height, QImage::Format_Indexed8);
    mask->setColorTable(maskColorTable);
    mask->fill(0);

    foreach (QImage* iter, masks) {
        delete iter;
    }
    masks.clear();
    masks.reserve(imageList.size());

    QProgressDialog progress("Вычитание фона", "Остановить", 0, imageList.size(), this);
    progress.setWindowTitle("Распознование");
    progress.setWindowModality(Qt::WindowModal);
    progress.setValue(0);

    int prog = 1;
    QList<QImage*>::iterator image = imageList.begin();
    image++;

    masks << mask;

    QImage* blackDisk = disk(4, 0xFF000000);
    QImage* whiteDisk = disk(4, 0xFFFFFFFF);

    for (; image != imageList.end(); image++)
    {
        if (prog++ % 10 == 0)
        {
            progress.setValue(prog);
        }

        mask = new QImage(width, height, QImage::Format_Indexed8);
        mask->setColorTable(maskColorTable);

        QRgb* imagePixel = (QRgb*)(*image)->bits();
        uchar* maskPixel = mask->bits();

        for (int i = 0; i < pixelCount; i++, imagePixel++, maskPixel++)
        {
            *maskPixel = (backg[i]->isBackground(*imagePixel)) ? 0 : 1;
        }

        // Размыкание
        dilation(mask, *blackDisk, 0xFF000000, 0xFFFFFFFF);
        dilation(mask, *whiteDisk, 0xFFFFFFFF, 0xFF000000);

        //ui->imageView->setPixmap(QPixmap::fromImage(*mask));
        //QMessageBox(QMessageBox::NoIcon, "Отладка", QString("%1 %2").arg(j).arg(d_max)).exec();

        masks << mask;

        if (progress.wasCanceled())
            break;
    }

    delete blackDisk;
    delete whiteDisk;
}

void MainWindow::substractBackground2()
{
    /*QImage* firstFrame = imageList.first();
    fillBackg(firstFrame->byteCount());

    // Начальное заполнение гауссианов
    uchar* pixel = firstFrame->bits();
    for(int i = 0; i < firstFrame->byteCount(); i++)
    {
        backg[i]->mu           = *pixel;
        backg[i]->sigma_2      = 100;
        backg[i]->sigma_sqrt_1 = 1. / 10.;
    }

    QVector<QRgb> maskColorTable;
    maskColorTable << 0xFF000000;
    maskColorTable << 0xFFFFFFFF;

    int width = firstFrame->width();
    int height= firstFrame->height();

    QImage* mask = new QImage(width, height, QImage::Format_Indexed8);
    mask->setColorTable(maskColorTable);
    mask->fill(0);

    foreach (QImage* iter, masks) {
        delete iter;
    }
    masks.clear();
    masks.reserve(imageList.size());

    QProgressDialog progress("Вычитание фона", "Остановить", 0, imageList.size(), this);
    progress.setWindowTitle("Распознование");
    progress.setWindowModality(Qt::WindowModal);
    progress.setValue(0);

    int prog = 1;
    QList<QImage*>::iterator image = imageList.begin();
    image++;

    masks << mask;

    float sigmamin2 = sigmamin * sigmamin;

    //*QImage* blackDisk = disk(3, 0xFF000000);
    QImage* whiteDisk = disk(3, 0xFF000000);///*

    ////*
    for (int i = 0; i < pixelCount; i++)
    {
        backg[i]->sigmamin = ui->spinSigmaMax->value();
        backg[i]->finalize();
    }
    ///*
    for (; image != imageList.end(); image++)
    {
        progress.setValue(prog++);

        mask = new QImage(width, height, QImage::Format_Indexed8);
        mask->setColorTable(maskColorTable);

        uchar* imagePixel = (*image)->bits();
        uchar* maskPixel = mask->bits();

        for (int i = 0; i < pixelCount; i++, imagePixel++, maskPixel++)
        {
            /*float d = (*imagePixel - backg[i]->mu);
            d *= d;

            if (d > k*(backg[i]->sigma_2))
            {
                // Передний план
                *maskPixel = 1;
            }
            else
            {
                // Фон
                *maskPixel = 0;
                backg[i]->mu      = rho * (*pixel) + (1 + rho) * backg[i]->mu;
                float tmp         = *pixel - backg[i]->mu;
                backg[i]->sigma_2 = rho * tmp*tmp  + (1 + rho) * backg[i]->sigma_2;
                if (backg[i]->sigma_2 < sigmamin2)
                    backg[i]->sigma_2 = sigmamin2;

                backg[i]->sigma_sqrt_1= 1. / sqrt(backg[i]->sigma_2);
            }//

            *maskPixel = (backg[i]->isBackground(*imagePixel)) ? 0 : 1;
        }

        //dilation(mask, *blackDisk, 0xFF000000, 0xFFFFFFFF);
        //dilation(mask, *whiteDisk, 0xFFFFFFFF, 0xFF000000);

        ui->imageView->setPixmap(QPixmap::fromImage(*mask));
        //QMessageBox(QMessageBox::NoIcon, "Отладка", QString("%1 %2").arg(j).arg(d_max)).exec();

        masks << mask;

        if (progress.wasCanceled())
            break;
    }

    return;

    //delete blackDisk;
    //delete whiteDisk;*/
}

void MainWindow::applyMasks()
{
    foreach (QImage* iter, imagesWithMasks)
    {
        delete iter;
    }
    imagesWithMasks.clear();
    imagesWithMasks.reserve(masks.size());

    for (int i = 0; i < masks.size(); i++)
    {
        QImage* image = new QImage(*imageList[i]);
        image->setAlphaChannel(*(masks[i]));
        imagesWithMasks << image;
    }
}

QList<QImage*>* MainWindow::createBorders()
{
    QList<QImage*>* borders = new QList<QImage*>;
    borders->reserve(masks.size());

    QImage* temp;
    QImage* firstFrame = masks[0];
    int imageWidth = firstFrame->width();
    int imageHeight= firstFrame->height();

    QVector<QRgb> borderColorMask;
    borderColorMask << 0x00000000;
    borderColorMask << 0xFFFF0000;

    foreach (QImage* iter, masks)
    {
        temp = new QImage(imageWidth, imageHeight, QImage::Format_Indexed8);
        temp->setColorTable(borderColorMask);
        temp->fill(0);

        if (xy* croped = crop(*iter))
        {
            uchar* pixel1 = temp->scanLine(croped[0].y) + croped[0].x;
            uchar* pixel2 = temp->scanLine(croped[1].y) + croped[0].x;

            for (int x = croped[0].x; x <= croped[1].x; x++, pixel1++, pixel2++)
            {
                *pixel1 = 1; *pixel2 = 1;
            }

            pixel1 = temp->scanLine(croped[0].y) + croped[0].x;
            pixel2 = temp->scanLine(croped[0].y) + croped[1].x;

            for (int y = croped[0].y; y <= croped[1].y; y++, pixel1 += imageWidth, pixel2 += imageWidth)
            {
                *pixel1 = 1; *pixel2 = 1;
            }

            delete [] croped;
        }

        *borders << temp;
    }
    return borders;
}

void MainWindow::applyBorders()
{
    QList <xy> centres;
    centres.reserve(QueueLength);

    foreach (QImage* iter, imagesWithMasks)
    {
        delete iter;
    }
    imagesWithMasks.clear();
    imagesWithMasks.reserve(masks.size());

    QList<QImage*> *borders = createBorders();

    QPen trajPen(QColor(Qt::red));

    for (int i = 0; i < borders->size(); i++)
    {
        QImage* iter = borders->at(i);
        QImage* image = new QImage(*imageList[i]);
        xy centre = centresOfMass[i];

        if (centre.x > 0)
        {
            if (centres.size() == QueueLength)
            {
                centres.removeFirst();
            }
            centres << centre;
        }

        QPainter paint;
        paint.begin(image);
        paint.drawImage(0, 0, *iter);

        // Рисование траекторий
        paint.setPen(trajPen);
        if (centres.size() > 1)
            for (QList<xy>::iterator iter = centres.begin() + 1, iter1 = centres.begin();
                 iter != centres.end(); iter++, iter1++)
            {
                paint.drawLine(iter1->x, iter1->y, iter->x, iter->y);
                paint.drawRect(iter->x - 2, iter->y - 2, 4, 4);
            }

        paint.end();

        imagesWithMasks << image;

        delete iter;
    }
    borders->clear();
    delete borders;
}

void MainWindow::getCentresOfMass()
{
    centresOfMass.clear();
    centresOfMass.reserve(masks.size());

    QImage* firstFrame = masks[0];

    int imageWidth = firstFrame->width();
    int imageHeight= firstFrame->height();

    foreach (QImage* iter, masks)
    {
        xy center;
        center.x = 0;
        center.y = 0;

        uchar* pixel = iter->bits();
        int count = 0;
        for (int y = 0; y < imageHeight; y++)
            for (int x = 0; x < imageWidth; x++, pixel++)
            {
                if (*pixel)
                {
                    count++;
                    center.x += x;
                    center.y += y;
                }
            }

        if (count)
        {
            center.x /= count;
            center.y /= count;
        }
        else
        {
            center.x = -1;
            center.y = -1;
        }

        centresOfMass << center;
    }
}

void MainWindow::fillBackg(int n)
{
    clearBackg();
    backg.reserve(n);
    for (int i = 0; i < n; i++)
        backg << new Gaussian(sigmamin);
}

void MainWindow::clearBackg()
{
    foreach (Gaussian* iter, backg) {
        delete iter;
    }
    backg.clear();
}

QImage* MainWindow::maskGradient(QImage *origin)
{
    // Результат
    int imageWidth = origin->width();
    int imageHeight= origin->height();
    QImage* mask = new QImage(imageWidth, imageHeight, QImage::Format_Indexed8);
    QVector<QRgb> maskColorTable;
    maskColorTable << 0xFF000000;
    maskColorTable << 0xFFFFFFFF;
    mask->setColorTable(maskColorTable);

    //////////////////////////////////////////////////////////////
    /// Вычисление градиента
    ///
    {
    /// Первая строчка
    uchar* pixel = mask->bits();
    uchar* y1;
    uchar* y2 = origin->bits();
    uchar* y3 = origin->scanLine(1);

    // Левый верхний угол
    int gradient = abs(-(*y2 + *(y2 + 1)) + (*y3 + *(y3 + 1)))
                 + abs(-(*y2 + *y3) + *(y2 + 1) + *(y3 + 1));
    *pixel = (gradient > 0) ? 1 : 0;
    pixel++; y2++; y3++;
    // Элементы первой строки, кроме крайних
    for (int x = 1; x < imageWidth - 1; x++,
                                        y2++, y3++,
                                        pixel++)
    {
        // Вычисляем приближенное значение градиента
        gradient = abs(-((*(y2 - 1)) + 2 * (*y2) + (*(y2 + 1)))
                       + (*(y3 - 1)) + 2 * (*y3) + (*(y3 + 1)))
                 + abs(-((*(y2 - 1)) + (*(y3 - 1)))
                       + (*(y2 + 1)) + (*(y3 + 1)));
        *pixel = (gradient > 0) ? 1 : 0;

    }
    // Верхний правый элемент
    gradient = abs(-(*(y2 - 1) + *y2 ) + *(y3 - 1) + *y3)
             + abs(-(*(y2 - 1) + *(y3 - 1)) + *y2 + *y3);
    *pixel = (gradient > 0) ? 1 : 0;

    /// Строки со второй, кроме последней
    y1 = origin->bits();
    pixel++; y2++; y3++;

    for (int y = 1; y < imageHeight - 1; y++)
    {
        // Первая точка в строке
        gradient = abs(-(*y1 + *(y1 + 1)) + (*y3 + *(y3 + 1)))
                 + abs(-(*y1 + *y3) + *(y1 + 1) + *(y3 + 1));
        *pixel = (gradient > 0) ? 1 : 0;

        y1++; y2++; y3++; pixel++;

        // Внутренние точки
        for (int x = 1; x < imageWidth - 1; x++,
                                            y1++, y2++, y3++,
                                            pixel++)
        {
            gradient = abs(-(*(y1 - 1) + 2 * (*y1) + *(y1 + 1))
                           + *(y3 - 1) + 2 * (*y3) + *(y3 + 1) )
                     + abs(-(*(y1 - 1) + 2 * (*(y2 - 1)) + *(y3 - 1))
                           + *(y1 + 1) + 2 * (*(y2 + 1)) + *(y3 + 1));
            *pixel = (gradient > 0) ? 1 : 0;
        }

        // Последняя точка в строке
        gradient = abs(-(*(y1 - 1) + *y1 ) + *(y3 - 1) + *y3)
                 + abs(-(*(y1 - 1) + *(y3 - 1)) + *y1 + *y3);
        *pixel = (gradient > 0) ? 1 : 0;

        y1++; y2++; y3++; pixel++;
    }

    /// Последняя строка
    // Левый нижний угол
    gradient = abs(-(*y1 + *(y1 + 1)) + (*y2 + *(y2 + 1)))
             + abs(-(*y1 + *y2) + *(y1 + 1) + *(y2 + 1));
    *pixel = (gradient > 0) ? 1 : 0;
    pixel++; y1++; y2++;
    // Элементы первой строки, кроме крайних
    for (int x = 1; x < imageWidth - 1; x++,
                                        y1++, y2++,
                                        pixel++)
    {
        // Вычисляем приближенное значение градиента
        gradient = abs(-((*(y1 - 1)) + 2 * (*y1) + (*(y1 + 1)))
                       + (*(y2 - 1)) + 2 * (*y2) + (*(y2 + 1)))
                 + abs(-((*(y1 - 1)) + (*(y2 - 1)))
                       + (*(y1 + 1)) + (*(y2 + 1)));
        *pixel = (gradient > 0) ? 1 : 0;

    }
    // Правый нижний элемент
    gradient = abs(-(*(y1 - 1) + *y1 ) + *(y2 - 1) + *y2)
             + abs(-(*(y1 - 1) + *(y2 - 1)) + *y1 + *y2);
    *pixel = (gradient > 0) ? 1 : 0;
    }
    ///
    //////////////////////////////////////////////////////////////

    return mask;
}

void MainWindow::clearLists()
{
    foreach (QImage* iter, imageList)
    {
        delete iter;
    }
    imageList.clear();

    foreach (QImage* iter, imagesWithMasks)
    {
        delete iter;
    }
    imagesWithMasks.clear();

    clearBackg();
}

void MainWindow::convertToGrayscale(QImage &image)
{
    QVector<QRgb> ColorTable;
    for (unsigned int i = 0; i < 256; i++)
    {
        ColorTable << ((0xFF000000) + (i << 16) + (i << 8) + i);
    }

    QImage grayImage(image.width(), image.height(), QImage::Format_Indexed8);
    grayImage.setColorTable(ColorTable);

    QRgb*  origPixel = (QRgb*)image.bits();
    uchar* grayPixel =        grayImage.bits();

    for (int i = 0; i < grayImage.byteCount(); i++, origPixel++, grayPixel++)
    {
        int R, G, B;
        R = G = B = *origPixel;
        R >>= 16; G >>= 8;
        R = R & 0xFF; R = R & 0xFF; R = R & 0xFF;
    //    *grayPixel = qGray(*origPixel);
    }

    image = grayImage;
}

/////////////////////////////////////////////////////////////////////////////////
/// \brief Gaussian::Gaussian
///
/////////////////////////////////////////////////////////////////////////////////

Gaussian::Gaussian(float _sirmamin, bool fullMatrix, bool hsv) :
    usingRealTime(false), usingFullMastrix(fullMatrix), usingHsv(hsv)
{
    sigmamin = _sirmamin;
    isNotFinalized = true;
}

void Gaussian::addItem(QRgb x_)
{
    RgbColor x;
    if (usingHsv)
    {
        QColor hsv(x_);

        x.R = hsv.hsvHue();
        x.G = hsv.hsvSaturation();
        x.B = hsv.value();
    }
    else
    {
        x.B = x_ & 0xFF;
        x_ >>= 8;
        x.G = x_ & 0xFF;
        x_ >>= 8;
        x.R = x_ & 0xFF;
    }
    points << x;
    isNotFinalized = true;
}
/*
void Gaussian::addItemMix(QRgb x_)
{
    // Реализация для смешивания
    RgbColor x;
    if (usingHsv)
    {
        QColor hsv(x_);

        x.R = hsv.hsvHue();
        x.G = hsv.hsvSaturation();
        x.B = hsv.value();
    }
    else
    {
        x.B = x_ & 0xFF;
        x_ >>= 8;
        x.G = x_ & 0xFF;
        x_ >>= 8;
        x.R = x_ & 0xFF;
    }

    if (!usingRealTime)
    {
        usingRealTime = true;
        mu.R = x.R;
        mu.G = x.G;
        mu.B = x.B;

        memset(&sigma, 0, sizeof(sigma));
        memset(&inver, 0, sizeof(inver));


    }



}*/

float Gaussian::p(uchar x)
{
    /*float expPower = (float)x - mu;
    expPower *= expPower * sigma_2;
    expPower = (float)exp((double)expPower);

    static float coef = 1. / sqrt(2* M_PI);

    return coef * expPower * sigma_sqrt_1;*/
    return 0;
}

bool Gaussian::isBackground(QRgb x_)
{
    if (isNotFinalized)
        finalize();

    if (isNotFinalized)
    {
        return -1;
    }

    RgbColor x;
    if (usingHsv)
    {
        QColor hsv(x_);
        x.R = hsv.hslHue();
        x.G = hsv.hslSaturation();
        x.B = hsv.value();
    }
    else
    {
        x.B = x_ & 0xFF;
        x_ >>= 8;
        x.G = x_ & 0xFF;
        x_ >>= 8;
        x.R = x_ & 0xFF;
    }

    float x_mu[3];
    x_mu[0] = ((float)x.R - mu.Rf);
    if (x_mu[0] < 0)
        x_mu[0] = -x_mu[0];

    x_mu[1] = ((float)x.G - mu.Gf);
    if (x_mu[1] < 0)
        x_mu[1] = -x_mu[1];

    x_mu[2] = ((float)x.B - mu.Bf);
    if (x_mu[2] < 0)
        x_mu[2] = -x_mu[2];

    double expPower;
    if (usingFullMastrix)
    {
        expPower = x_mu[0] * (inver[0][0] * x_mu[0] + inver[0][1] * x_mu[1] + inver[0][2] * x_mu[2])
                 + x_mu[1] * (inver[1][0] * x_mu[0] + inver[1][1] * x_mu[1] + inver[1][2] * x_mu[2])
                 + x_mu[2] * (inver[2][0] * x_mu[0] + inver[2][1] * x_mu[1] + inver[2][2] * x_mu[2]);
                    //(x - mu) * inv * (x - mu) * 0.5;
        if (expPower < 0)
            expPower = -expPower;

        expPower = sqrt(expPower);
    }
    else
    {
        expPower = x_mu[0] + x_mu[1] + x_mu[2];
        expPower /= detSqrt;
    }
    return (expPower < k * k * k) ? true : false;
}

void Gaussian::finalize()
{
    memset(&mu, 0, sizeof(mu));
    memset(&sigma, 0, sizeof(sigma));

    int size = points.size();

    foreach (RgbColor iter, points) {
        mu.Rf += iter.R;
        mu.Gf += iter.G;
        mu.Bf += iter.B;
    }
    mu.Rf /= size; mu.Gf /= size; mu.Bf /= size;

    if (usingFullMastrix)
    {
        foreach (RgbColor iter, points) {
            sigma[0][0] += (iter.R - mu.Rf) * (iter.R - mu.Rf);
            sigma[0][1] += (iter.R - mu.Rf) * (iter.G - mu.Gf);
            sigma[0][2] += (iter.R - mu.Rf) * (iter.B - mu.Bf);

            sigma[1][1] += (iter.G - mu.Gf) * (iter.G - mu.Gf);
            sigma[1][2] += (iter.G - mu.Gf) * (iter.B - mu.Bf);

            sigma[2][2] += (iter.B - mu.Bf) * (iter.B - mu.Bf);
        }
        sigma[0][0] /= size; sigma[0][1] /= size; sigma[0][2] /= size;
                             sigma[1][1] /= size; sigma[1][2] /= size;
                                                  sigma[2][2] /= size;

        sigma[1][0] = sigma[0][1];
        sigma[2][0] = sigma[0][2]; sigma[2][1] = sigma[1][2];
    }
    else
    {
        foreach (RgbColor iter, points) {
            sigma[0][0] += (iter.R - mu.Rf) * (iter.R - mu.Rf);
            sigma[1][1] += (iter.G - mu.Gf) * (iter.G - mu.Gf);
            sigma[2][2] += (iter.B - mu.Bf) * (iter.B - mu.Bf);
        }
        sigma[0][0] /= size; sigma[1][1] /= size; sigma[2][2] /= size;
    }

    if (sigma[0][0] < sigmamin)
        sigma[0][0] = sigmamin;
    if (sigma[1][1] < sigmamin)
        sigma[1][1] = sigmamin;
    if (sigma[2][2] < sigmamin)
        sigma[2][2] = sigmamin;

    if (usingFullMastrix)
    {
        inver[0][0] = sigma[1][1] * sigma[2][2] - sigma[1][2] * sigma[2][1];
        inver[1][0] = sigma[1][2] * sigma[2][0] - sigma[1][0] * sigma[2][2];
        inver[2][0] = sigma[1][0] * sigma[2][1] - sigma[1][1] * sigma[2][0];

        inver[0][1] = inver[1][0];
        inver[1][1] = sigma[0][0] * sigma[2][2] - sigma[0][2] * sigma[2][0];
        inver[2][1] = sigma[0][1] * sigma[2][0] - sigma[0][0] * sigma[2][1];

        inver[0][2] = inver[2][0];
        inver[1][2] = inver[2][1];
        inver[2][2] = sigma[0][0] * sigma[1][1] - sigma[0][1] * sigma[1][0];

        det = sigma[0][0] * inver[0][0] + sigma[0][1] * inver[0][1]
            + sigma[0][2] * inver[0][2];

        if (det < 0)
            detSqrt = sqrt(-det);
        else
            detSqrt = sqrt(det);

        inver[0][0] /= det;        inver[0][1] /= det;        inver[0][2] /= det;
        inver[1][0] = inver[0][1]; inver[1][1] /= det;        inver[1][2] /= det;
        inver[2][0] = inver[0][2]; inver[2][1] = inver[1][2]; inver[2][2] /= det;
    }
    else
    {
        inver[0][0] = 1. / sigma[0][0]; inver[1][1] = 1. / sigma[1][1]; inver[2][2] = 1. / sigma[2][2];

        det = sigma[0][0] + sigma[1][1] + sigma[2][2];
        detSqrt = sqrt(det);
    }

    isNotFinalized = false;
}
