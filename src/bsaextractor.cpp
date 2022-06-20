#include "bsaextractor.h"

#include "iplugingame.h"
#include "imodlist.h"
#include <versioninfo.h>
#include <imodinterface.h>
#include <questionboxmemory.h>
#include <report.h>

#include <QDir>
#include <QFileInfoList>
#include <QCoreApplication>
#include <QMessageBox>
#include <QtPlugin>

#include <functional>

#include <boost/bind.hpp>
#include <binary_io/binary_io.hpp>
#include <bsa/bsa.hpp>

using namespace MOBase;
namespace bindph = std::placeholders;


namespace
{
    binary_io::any_ostream openFile(const std::filesystem::path& path)
    {
        std::filesystem::create_directories(path.parent_path());
        return binary_io::any_ostream(std::in_place_type<binary_io::file_ostream>, path);
    }

    void unpackTES3(
        const std::filesystem::path& input,
        const std::filesystem::path& output)
    {
        bsa::tes3::archive bsa;
        bsa.read(input);

        for (const auto& [key, file] : bsa) {
            auto out = openFile(output / key.name());
            file.write(out);
        }
    }

    void unpackTES4(
        const std::filesystem::path& input,
        const std::filesystem::path& output)
    {
        bsa::tes4::archive bsa;
        const auto format = bsa.read(input);

        for (auto& dir : bsa) {
            for (auto& file : dir.second) {
                auto out = openFile(output / dir.first.name() / file.first.name());
                file.second.write(out, format);
            }
        }
    }

    void unpackFO4(
        const std::filesystem::path& input,
        const std::filesystem::path& output)
    {
        bsa::fo4::archive ba2;
        const auto format = ba2.read(input);

        for (auto& [key, file] : ba2) {
            auto out = openFile(output / key.name());
            file.write(out, format);
        }
    }
}


BsaExtractor::BsaExtractor()
  : m_Organizer(nullptr)
{
}

bool BsaExtractor::init(MOBase::IOrganizer *moInfo)
{
  m_Organizer = moInfo;
  moInfo->modList()->onModInstalled(std::bind(&BsaExtractor::modInstalledHandler, this, bindph::_1));
  return true;
}

QString BsaExtractor::name() const
{
  return "BSA Extractor";
}

QString BsaExtractor::localizedName() const
{
  return tr("BSA Extractor");
}


QString BsaExtractor::author() const
{
  return "Tannin";
}

QString BsaExtractor::description() const
{
  return tr("Offers a dialog during installation of a mod to unpack all its BSAs");
}

VersionInfo BsaExtractor::version() const
{
  return VersionInfo(1, 2, 0, VersionInfo::RELEASE_FINAL);
}

QList<PluginSetting> BsaExtractor::settings() const
{
  return {
    PluginSetting("only_alternate_source", "only trigger bsa extraction for alternate game sources", true)
  };
}


bool BsaExtractor::extractProgress(QProgressDialog &progress, int percentage, std::string fileName)
{
  progress.setLabelText(fileName.c_str());
  progress.setValue(percentage);
  qApp->processEvents();
  return !progress.wasCanceled();
}


void BsaExtractor::modInstalledHandler(IModInterface *mod)
{

  if (m_Organizer->pluginSetting(name(), "only_alternate_source").toBool() &&
      !(m_Organizer->modList()->state(mod->name()) & IModList::STATE_ALTERNATE)) {
    return;
  }

  if (QFileInfo(mod->absolutePath()) == QFileInfo(m_Organizer->managedGame()->dataDirectory().absolutePath())) {
    QMessageBox::information(nullptr, tr("invalid mod name"),
                             tr("BSA extraction doesn't work on mods that have the same name as a non-MO mod."
                                "Please remove the mod then reinstall with a different name."));
    return;
  }
  QDir dir(mod->absolutePath());

  QFileInfoList archives = dir.entryInfoList(QStringList({ "*.bsa", "*.ba2" }));
  if (archives.length() != 0 &&
      (QuestionBoxMemory::query(nullptr, "unpackBSA", tr("Extract BSA"),
                             tr("This mod contains at least one BSA. Do you want to unpack it?\n"
                                "(If you don't know about BSAs, just select no)"),
                             QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No) == QDialogButtonBox::Yes)) {

    bool removeBSAs = (QuestionBoxMemory::query(nullptr, "removeUnpackedBSA", tr("Remove extracted archives"),
                       tr("Do you wish to remove BSAs after extraction completed?\n"),
                       QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No) == QDialogButtonBox::Yes);
    foreach (QFileInfo archiveInfo, archives) {
      const auto archiveQPath = archiveInfo.absoluteFilePath();
      const auto archivePath = std::filesystem::path(
        reinterpret_cast<const char8_t*>(archiveInfo.absoluteFilePath().toUtf8().data()));
      const auto format = bsa::guess_file_format(archivePath);
      if (!format) {
        reportError(tr("file is not actually an archive: %1").arg(archiveQPath));
        return;
      }

      try {
        const auto modPath = std::filesystem::path(
          reinterpret_cast<const char8_t*>(mod->absolutePath().toUtf8().data()));
        switch (*format) {
        case bsa::file_format::tes3:
            unpackTES3(archivePath, modPath);
            break;
        case bsa::file_format::tes4:
            unpackTES4(archivePath, modPath);
            break;
        case bsa::file_format::fo4:
            unpackFO4(archivePath, modPath);
            break;
        }
      } catch(const bsa::exception& e) {
        reportError(tr("Encountered an error while unpacking the file: %1").arg(e.what()));
      }

      if (removeBSAs) {
        if (!QFile::remove(archiveInfo.absoluteFilePath())) {
          qCritical("failed to remove archive %s", archiveInfo.absoluteFilePath().toUtf8().constData());
        }
      }
    }
  }
}

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
Q_EXPORT_PLUGIN2(bsaextractor, BsaExtractor)
#endif
