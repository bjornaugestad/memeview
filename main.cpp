#include <QtWidgets>

static bool isImg(const QString &p)
{
    static const QSet<QString> exts = {"png", "jpg", "jpeg", "webp", "bmp", "gif"};
    return exts.contains(QFileInfo(p).suffix().toLower());
}

struct FileSet {
    QStringList files;
    int _icurr = 0;

    QString cur() const { return files.value(_icurr); }

    void dropCurrent()
    {
        files.removeAt(_icurr);
        if (_icurr >= files.size())
            _icurr = qMax(0, _icurr - 1);
    }

    bool empty() const { return files.isEmpty(); }
};

class View : public QLabel
{
    Q_OBJECT FileSet set;

public:
    explicit View(QStringList paths, QWidget *parent = nullptr) : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setFocusPolicy(Qt::StrongFocus);
        setFocus(Qt::OtherFocusReason);

        for (const QString &pth : paths) {
            QFileInfo fi(pth);
            if (fi.isFile() && isImg(fi.filePath()))
                set.files << fi.absoluteFilePath();
        }

        if (set.empty()) {
            fputs("No valid image files.\n", stderr);
            QTimer::singleShot(0, this, &QWidget::close);
            return;
        }

        showImg();
    }

    void showEvent(QShowEvent *ev) override
    {
        QLabel::showEvent(ev);
        QTimer::singleShot(0, this, [this] {
            activateWindow();
            raise();
            setFocus(Qt::OtherFocusReason);
        });
    }

    // ----- core -----
    void showImg()
    {
        QImageReader r(set.cur());
        r.setAutoTransform(true);
        const QImage img = r.read();
        if (img.isNull()) {
            setText("Failed: " + set.cur());
        }
        else {
            setPixmap(QPixmap::fromImage(img).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            setWindowTitle(QFileInfo(set.cur()).fileName());
        }

        setFocus(Qt::OtherFocusReason);
    }

    void next()
    {
        if (set._icurr < set.files.size() - 1) {
            ++set._icurr;
            showImg();
        }
    }

    void prev()
    {
        if (set._icurr > 0) {
            --set._icurr;
            showImg();
        }
    }

    void renameCurrent()
    {
        QFileInfo fi(set.cur());
        bool ok = false;
        const QString base = fi.completeBaseName();
        QString nn = QInputDialog::getText(this, "Rename", base, QLineEdit::Normal, base, &ok);
        if (!ok || nn.isEmpty()) {
            setFocus(Qt::OtherFocusReason);
            return;
        }

        // Append original extension hvis bruker ikke skrev en
        if (QFileInfo(nn).suffix().isEmpty() && !fi.suffix().isEmpty())
            nn += "." + fi.suffix();

        const QString np = fi.dir().filePath(nn);
        if (QFile::exists(np)) {
            if (QMessageBox::question(this, "Overwrite?", QFileInfo(np).fileName() + " exists. Overwrite?") != QMessageBox::Yes) {
                setFocus(Qt::OtherFocusReason);
                return;
            }

            QFile::remove(np);
        }

        if (QFile::rename(fi.absoluteFilePath(), np)) {
            set.files[set._icurr] = QFileInfo(np).absoluteFilePath();
            showImg();
        }
        else {
            QMessageBox::warning(this, "Rename failed", "Could not rename file.");
            setFocus(Qt::OtherFocusReason);
        }
    }

    void deleteCurrent()
    { // permanent delete
        const QString path = set.cur();
        if (!QFile::remove(path)) {
            QMessageBox::warning(this, "Delete failed", "Could not delete file.");
            setFocus(Qt::OtherFocusReason);
            return;
        }

        set.dropCurrent();
        if (!set.empty())
            showImg();
        else
            close();
    }

    void moveCurrent()
    {
        QFileInfo fi(set.cur());
        QDir parent = fi.dir();

        // make list of subdirs
        QStringList subdirs;
        for (const QFileInfo &d : parent.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
            subdirs << d.fileName();

        bool ok = false;
        QString choice = QInputDialog::getItem(this,
            "Move to directory", "Destination directory:",
            subdirs, 0, true, &ok);

        if (!ok || choice.trimmed().isEmpty()) {
            setFocus(Qt::OtherFocusReason);
            return;
        }

        QString destDir;
        if (QDir::isAbsolutePath(choice))
            destDir = QDir(choice).absolutePath();
        else
            destDir = parent.absoluteFilePath(choice);

        QDir().mkpath(destDir);
        const QString target = QDir(destDir).filePath(fi.fileName());

        if (QFile::exists(target)) {
            if (QMessageBox::question(this, "Overwrite?", QFileInfo(target).fileName() + " exists. Overwrite?")
            == QMessageBox::Yes) {
                QFile::remove(target);
            }
            else {
                setFocus(Qt::OtherFocusReason);
                return;
            }
        }

        if (QFile::rename(fi.absoluteFilePath(), target)) {
            set.dropCurrent();
            if (!set.empty())
                showImg();
            else
                close();
        }
        else {
            QMessageBox::warning(this, "Move failed", "Could not move file.");
            setFocus(Qt::OtherFocusReason);
        }
    }

    // ----- events -----
    void resizeEvent(QResizeEvent *) override
    {
        if (!set.empty())
            showImg();
    }

    void keyPressEvent(QKeyEvent *e) override
    {
        switch (e->key()) {
            case Qt::Key_Right:
            case Qt::Key_L:
            case Qt::Key_N:
                next();
                break;

            case Qt::Key_Left:
            case Qt::Key_H:
            case Qt::Key_P:
                prev();
                break;

            case Qt::Key_R:
                renameCurrent();
                break;

            case Qt::Key_M:
                moveCurrent();
                break;

            case Qt::Key_Delete:
                deleteCurrent();
                break;

            case Qt::Key_Q:
                close();
                break;

            default:
                QLabel::keyPressEvent(e);
        }
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QStringList args = app.arguments();
    args.removeFirst(); // drop program name

    if (args.isEmpty()) {
        fputs("USAGE: memeview FILE [FILE...]\n", stderr);
        return 2;
    }

    // Shell expands ~, but let's be explicit
    for (QString &a : args)
        if (a.startsWith("~"))
            a.replace(0, 1, QDir::homePath());

    View v(args);
    v.showMaximized();
    return app.exec();
}

#include "main.moc"
