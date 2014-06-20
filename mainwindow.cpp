#include <cmath>

#include <QFileDialog>
#include <QProgressDialog>
#include <QDateTime>
#include <QTest>
#include <QMessageBox>

#include "mainwindow.h"
#include "ui_mainwindow.h"

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
            progress.setValue(i);
            QImage* image = new QImage;
            if (image->load(iter))
            {
                convertToGrayscale(*image);
                if (i == 0)
                    pixelCount = image->byteCount();

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
    applyMasks();
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
    progress.setWindowModality(Qt::WindowModal);
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
        uchar* pixel = image->bits();
        for (int j = 0; j < pixelCount; j++, pixel++)
            backg[j]->addItem(*pixel);

        if (progress.wasCanceled())
            break;
    }
    for (int j = 0; j < pixelCount; j++)
        backg[j]->finalize();
}

void MainWindow::substractBackground()
{
    fillBackg();

    QImage* firstFrame = imageList.first();

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

    QImage* blackDisk = disk(3, 0xFF000000);
    QImage* whiteDisk = disk(3, 0xFF000000);

    /*
    for (int i = 0; i < pixelCount; i++)
    {
        backg[i]->sigmamin = ui->spinSigmaMax->value();
        backg[i]->finalize();
    }
    */

    for (; image != imageList.end(); image++)
    {
        progress.setValue(prog++);

        mask = new QImage(width, height, QImage::Format_Indexed8);
        mask->setColorTable(maskColorTable);

        uchar* imagePixel = (*image)->bits();
        uchar* maskPixel = mask->bits();

        for (int i = 0; i < pixelCount; i++, imagePixel++, maskPixel++)
            *maskPixel = (backg[i]->isBackground(*imagePixel)) ? 0 : 1;

        //dilation(mask, *blackDisk, 0xFF000000, 0xFFFFFFFF);
        //dilation(mask, *whiteDisk, 0xFFFFFFFF, 0xFF000000);

        masks << mask;

        if (progress.wasCanceled())
            break;
    }

    delete blackDisk;
    delete whiteDisk;
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
        *grayPixel = qGray(*origPixel);

    image = grayImage;
}

/////////////////////////////////////////////////////////////////////////////////
/// \brief Gaussian::Gaussian
///
/////////////////////////////////////////////////////////////////////////////////

Gaussian::Gaussian(float _sirmamin)
{
    sigmamin = _sirmamin;
    isNotFinalized = true;
}

__fastcall void Gaussian::addItem(uchar x)
{
    points << x;
    isNotFinalized = true;
}

__fastcall float Gaussian::p(uchar x)
{
    float expPower = (float)x - mu;
    expPower *= expPower * sigma_2;
    expPower = (float)exp((double)expPower);

    static float coef = 1. / sqrt(2* M_PI);

    return coef * expPower * sigma_sqrt_1;
}

__fastcall bool Gaussian::isBackground(uchar x)
{
    if (isNotFinalized)
        finalize();

    if (isNotFinalized)
    {
        return -1;
    }

    float res = (float)x - mu;

    return (res < sigma_k && res > -sigma_k) ? true : false;
}

void Gaussian::finalize()
{
    float sigmamin2 = sigmamin * sigmamin;

    unsigned int sumI = 0;
    int size = points.size();

    foreach (uchar iter, points) {
        sumI += iter;
    }
    mu = (float)sumI / size;

    float sumF = 0, tmp;
    foreach (uchar iter, points) {
        tmp = (float)iter - mu;
        sumF += tmp * tmp;
    }

    sigma = sumF / size;
    if (sigma < sigmamin2)
        sigma = sigmamin2;

    sigma_2 = -0.5 / sigma;  // коэффициент экспоненты
    sigma   = sqrt(sigma);

    sigma_k = k * sigma;

    isNotFinalized = false;
}
