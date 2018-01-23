#include "propagateuploadencrypted.h"
#include "clientsideencryptionjobs.h"
#include "networkjobs.h"
#include "clientsideencryption.h"
#include "account.h"

#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QFile>
#include <QTemporaryFile>
#include <QLoggingCategory>
#include <QMimeDatabase>

namespace OCC {

Q_LOGGING_CATEGORY(lcPropagateUploadEncrypted, "nextcloud.sync.propagator.upload.encrypted", QtInfoMsg)

PropagateUploadEncrypted::PropagateUploadEncrypted(OwncloudPropagator *propagator, SyncFileItemPtr item)
: _propagator(propagator),
 _item(item),
 _metadata(nullptr)
{
}

void PropagateUploadEncrypted::start()
{
  /* If the file is in a encrypted-enabled nextcloud instance, we need to
      * do the long road: Fetch the folder status of the encrypted bit,
      * if it's encrypted, find the ID of the folder.
      * lock the folder using it's id.
      * download the metadata
      * update the metadata
      * upload the file
      * upload the metadata
      * unlock the folder.
      *
      * If the folder is unencrypted we just follow the old way.
      */
      qCDebug(lcPropagateUploadEncrypted) << "Starting to send an encrypted file!";
      QFileInfo info(_item->_file);
      auto getEncryptedStatus = new GetFolderEncryptStatusJob(_propagator->account(),
                                                           info.path());

      connect(getEncryptedStatus, &GetFolderEncryptStatusJob::encryptStatusFolderReceived,
              this, &PropagateUploadEncrypted::slotFolderEncryptedStatusFetched);
      connect(getEncryptedStatus, &GetFolderEncryptStatusJob::encryptStatusError,
             this, &PropagateUploadEncrypted::slotFolderEncryptedStatusError);
      getEncryptedStatus->start();
}

void PropagateUploadEncrypted::slotFolderEncryptedStatusFetched(const QString &folder, bool isEncrypted)
{
  qCDebug(lcPropagateUploadEncrypted) << "Encrypted Status Fetched" << folder << isEncrypted;

  /* We are inside an encrypted folder, we need to find it's Id. */
  if (isEncrypted) {
      qCDebug(lcPropagateUploadEncrypted) << "Folder is encrypted, let's get the Id from it.";
      auto job = new LsColJob(_propagator->account(), folder, this);
      job->setProperties({"resourcetype", "http://owncloud.org/ns:fileid"});
      connect(job, &LsColJob::directoryListingSubfolders, this, &PropagateUploadEncrypted::slotFolderEncryptedIdReceived);
      connect(job, &LsColJob::finishedWithError, this, &PropagateUploadEncrypted::slotFolderEncryptedIdError);
      job->start();
  } else {
    qCDebug(lcPropagateUploadEncrypted) << "Folder is not encrypted, getting back to default.";
    emit folerNotEncrypted();
  }
}



/* We try to lock a folder, if it's locked we try again in one second.
 * if it's still locked we try again in one second. looping untill one minute.
 *                                                                      -> fail.
 * the 'loop':                                                         /
 *    slotFolderEncryptedIdReceived -> slotTryLock -> lockError -> stillTime? -> slotTryLock
 *                                        \
 *                                         -> success.
 */

void PropagateUploadEncrypted::slotFolderEncryptedIdReceived(const QStringList &list)
{
  qCDebug(lcPropagateUploadEncrypted) << "Received id of folder, trying to lock it so we can prepare the metadata";
  auto job = qobject_cast<LsColJob *>(sender());
  const auto& folderInfo = job->_folderInfos.value(list.first());
  _folderLockFirstTry.start();
  slotTryLock(folderInfo.fileId);
}

void PropagateUploadEncrypted::slotTryLock(const QByteArray& fileId)
{
  auto *lockJob = new LockEncryptFolderApiJob(_propagator->account(), fileId, this);
  connect(lockJob, &LockEncryptFolderApiJob::success, this, &PropagateUploadEncrypted::slotFolderLockedSuccessfully);
  connect(lockJob, &LockEncryptFolderApiJob::error, this, &PropagateUploadEncrypted::slotFolderLockedError);
  lockJob->start();
}

void PropagateUploadEncrypted::slotFolderLockedSuccessfully(const QByteArray& fileId, const QByteArray& token)
{
  qCDebug(lcPropagateUploadEncrypted) << "Folder" << fileId << "Locked Successfully for Upload, Fetching Metadata";
  // Should I use a mutex here?
  _currentLockingInProgress = true;
  _folderToken = token;
  _folderId = fileId;

  auto job = new GetMetadataApiJob(_propagator->account(), _folderId);
  connect(job, &GetMetadataApiJob::jsonReceived,
          this, &PropagateUploadEncrypted::slotFolderEncriptedMetadataReceived);
  job->start();
}

void PropagateUploadEncrypted::slotFolderEncriptedMetadataReceived(const QJsonDocument &json, int statusCode)
{
  qCDebug(lcPropagateUploadEncrypted) << "Metadata Received, Preparing it for the new file." << json.toVariant();

  // Encrypt File!
  _metadata = new FolderMetadata(_propagator->account(), json.toJson(QJsonDocument::Compact));

  QFileInfo info(_propagator->_localDir + QDir::separator() + _item->_file);

  // Find existing metadata for this file
  EncryptedFile encryptedFile;
  QVector<EncryptedFile> files = _metadata->files();
  for(EncryptedFile &file : files) {
    if (file.originalFilename == _item->File) {
      encryptedFile = file;
    }
  }

  encryptedFile.encryptionKey = EncryptionHelper::generateRandom(16);
  encryptedFile.initializationVector = EncryptionHelper::generateRandom(16);

  // New encrypted file so set it all up!
  if (encryptedFile.encryptedFilename.isEmpty()) {
      encryptedFile.encryptedFilename = EncryptionHelper::generateRandomString(20);
      encryptedFile.fileVersion = 1;
      encryptedFile.metadataKey = 1;
      encryptedFile.originalFilename = info.fileName();
      QMimeDatabase mdb;
      encryptedFile.mimetype = mdb.mimeTypeForFile(info).name().toLocal8Bit();
  }


  qCDebug(lcPropagateUploadEncrypted) << "Creating the encrypted file.";

  auto *input = new QFile(info.absoluteFilePath());
  auto *output = new QFile(QDir::tempPath() + QDir::separator() + encryptedFile.encryptedFilename);

  QByteArray tag;
  EncryptionHelper::fileEncryption(encryptedFile.encryptionKey, encryptedFile.initializationVector, input, output, tag);

  input->deleteLater();
  output->deleteLater();

  _completeFileName = output->fileName();

  qCDebug(lcPropagateUploadEncrypted) << "Creating the metadata for the encrypted file.";

  encryptedFile.authenticationTag = tag;

  _metadata->addEncryptedFile(encryptedFile);
  _encryptedFile = encryptedFile;

  qCDebug(lcPropagateUploadEncrypted) << "Metadata created, sending to the server.";
  auto job = new UpdateMetadataApiJob(_propagator->account(),
                                      _folderId,
                                      _metadata->encryptedMetadata(),
                                      _folderToken);

  connect(job, &UpdateMetadataApiJob::success, this, &PropagateUploadEncrypted::slotUpdateMetadataSuccess);
  connect(job, &UpdateMetadataApiJob::error, this, &PropagateUploadEncrypted::slotUpdateMetadataError);
  job->start();
}

void PropagateUploadEncrypted::slotUpdateMetadataSuccess(const QByteArray& fileId)
{
  qCDebug(lcPropagateUploadEncrypted) << "Uploading of the metadata success, Encrypting the file";
  QFileInfo outputInfo(_completeFileName);

  qCDebug(lcPropagateUploadEncrypted) << "Encrypted Info:" << outputInfo.path() << outputInfo.fileName() << outputInfo.size();
  qCDebug(lcPropagateUploadEncrypted) << "Finalizing the upload part, now the actuall uploader will take over";
  emit finalized(outputInfo.path() + QLatin1Char('/') + outputInfo.fileName(),
                 _item->_file.section(QLatin1Char('/'), 0, -2) + QLatin1Char('/') + outputInfo.fileName(),
                 outputInfo.size());
}

void PropagateUploadEncrypted::slotUpdateMetadataError(const QByteArray& fileId, int httpErrorResponse)
{
  qCDebug(lcPropagateUploadEncrypted) << "Update metadata error for folder" << fileId << "with error" << httpErrorResponse;
}

void PropagateUploadEncrypted::slotUnlockEncryptedFolderSuccess(const QByteArray& fileId)
{
    qCDebug(lcPropagateUploadEncrypted) << "Unlock Job worked for folder " << fileId;
}

void PropagateUploadEncrypted::slotUnlockEncryptedFolderError(const QByteArray& fileId, int httpStatusCode)
{
  qCDebug(lcPropagateUploadEncrypted) << "There was an error unlocking " << fileId << httpStatusCode;
}

void PropagateUploadEncrypted::slotFolderLockedError(const QByteArray& fileId, int httpErrorCode)
{
  /* try to call the lock from 5 to 5 seconds
    and fail if it's more than 5 minutes. */
  QTimer::singleShot(5000, this, [this, fileId]{
    if (!_currentLockingInProgress) {
      qCDebug(lcPropagateUploadEncrypted) << "Error locking the folder while no other update is locking it up.";
      qCDebug(lcPropagateUploadEncrypted) << "Perhaps another client locked it.";
      qCDebug(lcPropagateUploadEncrypted) << "Abort";
      return;
    }

    // Perhaps I should remove the elapsed timer if the lock is from this client?
    if (_folderLockFirstTry.elapsed() > /* five minutes */ 1000 * 60 * 5 ) {
      qCDebug(lcPropagateUploadEncrypted) << "One minute passed, ignoring more attemps to lock the folder.";
      return;
    }
    slotTryLock(fileId);
  });

  qCDebug(lcPropagateUploadEncrypted) << "Folder" << fileId << "Coundn't be locked.";
}

void PropagateUploadEncrypted::slotFolderEncryptedIdError(QNetworkReply *r)
{
  qCDebug(lcPropagateUploadEncrypted) << "Error retrieving the Id of the encrypted folder.";
}

void PropagateUploadEncrypted::slotFolderEncryptedStatusError(int error)
{
    qCDebug(lcPropagateUploadEncrypted) << "Failed to retrieve the status of the folders." << error;
}

} // namespace OCC
