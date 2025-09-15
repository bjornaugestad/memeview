#include <QtWidgets>

static bool isImg(const QString &p)
{
    static const QSet<QString> exts = {"png", "jpg", "jpeg", "webp", "bmp", "gif"};
    return exts.contains(QFileInfo(p).suffix().toLower());
}

struct State
{
    QStringList files;
    int i = 0;
    QString cur() const { return files.value(i); }
    void dropCurrent()
    {
        files.removeAt(i);
        if (i >= files.size())
            i = qMax(0, i - 1);
    }
    bool empty() const { return files.isEmpty(); }
};

class View : public QLabel
{
    Q_OBJECT
    State s;

  public:
    explicit View(QStringList paths, QWidget *parent = nullptr) : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setFocusPolicy(Qt::StrongFocus);
        setFocus(Qt::OtherFocusReason);

        // Ta kun gyldige bildefiler (args er filer, ikke kataloger)
        for (const QString &pth : paths) {
            QFileInfo fi(pth);
            if (fi.isFile() && isImg(fi.filePath()))
                s.files << fi.absoluteFilePath();
        }
        if (s.empty()) {
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
        QImageReader r(s.cur());
        r.setAutoTransform(true);
        const QImage img = r.read();
        if (img.isNull()) {
            setText("Failed: " + s.cur());
        }
        else {
            setPixmap(QPixmap::fromImage(img).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            setWindowTitle(QFileInfo(s.cur()).fileName());
        }
        setFocus(Qt::OtherFocusReason);
    }
    void next()
    {
        if (s.i < s.files.size() - 1) {
            ++s.i;
            showImg();
        }
    }
    void prev()
    {
        if (s.i > 0) {
            --s.i;
            showImg();
        }
    }

    void renameCurrent()
    {
        QFileInfo fi(s.cur());
        bool ok = false;
        const QString base = fi.completeBaseName();
        QString nn = QInputDialog::getText(this, "Rename", base, QLineEdit::Normal, base, &ok);
        if (!(ok && !nn.isEmpty())) {
            setFocus(Qt::OtherFocusReason);
            return;
        }

        // Append original extension hvis bruker ikke skrev en
        if (QFileInfo(nn).suffix().isEmpty() && !fi.suffix().isEmpty())
            nn += "." + fi.suffix();

        const QString np = fi.dir().filePath(nn);
        if (QFile::exists(np)) {
            if (QMessageBox::question(this, "Overwrite?", QFileInfo(np).fileName() + " exists. Overwrite?")
            != QMessageBox::Yes) {
                setFocus(Qt::OtherFocusReason);
                return;
            }
            QFile::remove(np);
        }
        if (QFile::rename(fi.absoluteFilePath(), np)) {
            s.files[s.i] = QFileInfo(np).absoluteFilePath();
            showImg();
        }
        else {
            QMessageBox::warning(this, "Rename failed", "Could not rename file.");
            setFocus(Qt::OtherFocusReason);
        }
    }

    void deleteCurrent()
    { // permanent delete
        const QString path = s.cur();
        if (!QFile::remove(path)) {
            QMessageBox::warning(this, "Delete failed", "Could not delete file.");
            setFocus(Qt::OtherFocusReason);
            return;
        }
        s.dropCurrent();
        if (!s.empty())
            showImg();
        else
            close();
    }

    void moveCurrent()
    {
        QFileInfo fi(s.cur());
        QDir parent = fi.dir();

        // Lag liste over undermapper i nåværende mappe
        QStringList subdirs;
        for (const QFileInfo &d : parent.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
            subdirs << d.fileName();

        bool ok = false;
        // Redigerbar liste: bruk relativt navn for søskenmappe, eller absolutt sti
        QString choice = QInputDialog::getItem(this,
        "Move to directory",
        "Destination directory:",
        subdirs,
        0,
        /*editable*/ true,
        &ok);
        if (!(ok && !choice.trimmed().isEmpty())) {
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
            != QMessageBox::Yes) {
                setFocus(Qt::OtherFocusReason);
                return;
            }
            QFile::remove(target);
        }
        if (QFile::rename(fi.absoluteFilePath(), target)) {
            s.dropCurrent();
            if (!s.empty())
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
        if (!s.empty())
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
    args.removeFirst(); // drop programnavn
    if (args.isEmpty()) {
        fputs("USAGE: memeview FILE [FILE...]\n", stderr);
        return 2;
    }
    // Shell ekspanderer ~, men la oss være hyggelige hvis noen siterer det
    for (QString &a : args)
        if (a.startsWith("~"))
            a.replace(0, 1, QDir::homePath());

    View v(args);
    v.showMaximized();
    return app.exec();
}

#include "main.moc"
