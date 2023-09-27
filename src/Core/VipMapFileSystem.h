#ifndef VIP_MAP_FILESYSTEM_H
#define VIP_MAP_FILESYSTEM_H

#include "VipConfig.h"
#include "VipCore.h"
#include "VipUniqueId.h"

#include <QIcon>
#include <QVariant>
#include <QIODevice>
#include <QDir>
#include <QSharedPointer>
#include <QEnableSharedFromThis>

class VipProgress;
class VipMapFileSystem;
typedef QSharedPointer<VipMapFileSystem> VipMapFileSystemPtr;

/// \a VipPath represents a node in a hierarchical tree manipulated through #VipMapFileSystem.
/// Usually it would be a file or a directory, from the filesystem or through a different interface like FTP or HTTP.
///
/// \a VipPath contains the full node path with a slash separator for each sub-node, and a set of attributes (like the size, the creation date, etc.)
class VIP_CORE_EXPORT VipPath
{
public:
	///Default ctor
	VipPath();
	///Copy ctor
	VipPath(const VipPath & other);
	///Construct from a canonical path and a bollean telling if this node represents a directory.
	/// The canonical path if modified by replacing backslash by slash and by removing any trailing slash.
	VipPath(const QString & full_path,  bool is_dir = false);
	///Construct from a canonical path, an attribute list and a bollean telling if this node represents a directory.
	/// The canonical path if modified by replacing backslash by slash and by removing any trailing slash.
	VipPath(const QString & full_path, const QVariantMap & attributes , bool is_dir );

	VipPath & operator=(const VipPath & other);


	///Returns the full canonical path
	QString canonicalPath() const { return m_canonical_path; }
	QString lastPath() const;
	QString fileName() const;
	QString filePath() const;
	///Returns whether this path represents a directory (a node with children) or a file (final leaf)
	bool isDir() const { return m_dir; }
	bool isEmpty() const;

	VipPath parent() const;

	void setMapFileSystem(const VipMapFileSystemPtr &);
	VipMapFileSystemPtr mapFileSystem() const;

	void setAttributes(const QVariantMap & attrs) { m_attributes = attrs; }
	void setAttribute(const QString & name, const QVariant & value) { m_attributes[name] = value; }
	const QVariantMap & attributes() const { return m_attributes; }
	QVariant attribute(const QString & attr) const { return m_attributes[attr]; }
	bool hasAttribute(const QString & attr) const { return m_attributes.find(attr) != m_attributes.end(); }
	QStringList mergeAttributes(const QVariantMap & attrs);

private:
	VipMapFileSystemPtr m_map;
	QVariantMap		m_attributes;
	QString			m_canonical_path;
	bool			m_dir;
};


VIP_CORE_EXPORT bool operator==(const VipPath & p1, const VipPath & p2);
VIP_CORE_EXPORT bool operator!=(const VipPath & p1, const VipPath & p2);

inline uint qHash(const VipPath &key)
{
	return qHash(key.canonicalPath());
}

class VipPathList : public QList<VipPath>
{
public:
	VipPathList() : QList<VipPath>() {}
	VipPathList(const QList<VipPath> & other) : QList<VipPath>(other) {}
	VipPathList(const QStringList & lst, bool all_dirs) : QList<VipPath>() {
		for (int i = 0; i < lst.size(); ++i)
			append(VipPath(lst[i], all_dirs));
	}

	QStringList paths() const {
		QStringList res;
		for (int i = 0; i < size(); ++i)
			res << (*this)[i].canonicalPath();
		return res;
	}
};

Q_DECLARE_METATYPE(VipPath)
Q_DECLARE_METATYPE(VipPathList);

class SearchThread;

/// \a VipMapFileSystem is an abstract class representing a physical or virtual file storage.
/// It could be the physical hard drive, or any distant server like a FTP or HTTP one.
///
/// \a VipMapFileSystem provides all necessary functions to interact with the file system (list paths, create or remove a path, open it,...).
/// Paths are represented using the #VipPath class.
class VIP_CORE_EXPORT VipMapFileSystem: public QObject, public QEnableSharedFromThis<VipMapFileSystem>
{
	Q_OBJECT
	friend class SearchThread;
public:
	/// Operations supported by this VipMapFileSystem.
	/// Listing a path content or telling if a path exists are always valid operations.
	enum SupportedOperation
	{
		Create = 0x0001, //Create a path (file or directory)
		Remove = 0X0002, //remove a path (file or directory)
		Rename = 0X0004, //Rename a file or directory
		CopyFile = 0x0008,  //Copy a file
		OpenRead = 0x001, //Open a file in read-only mode
		OpenWrite = 0x002, //open a file in write-only mode
		OpenText = 0x004, //Open a file in text mode
		All = Create| Remove| Rename | CopyFile | OpenRead| OpenWrite| OpenText
	};

	enum Errors
	{
		UnsupportedOperation = -1
	};

	Q_DECLARE_FLAGS(SupportedOperations, SupportedOperation);

	///Constructor
	VipMapFileSystem(SupportedOperations operations);
	~VipMapFileSystem();

	QSharedPointer<VipMapFileSystem> sharedPointer() const;
	VipLazyPointer lazyPointer() const;

	///Returns an icon for this path type
	virtual QIcon iconPath(const VipPath &) const { return QIcon(); }

	virtual bool requirePassword() const {return false; }
	/// @brief Set optional password, call before open()
	virtual void setPassword(const QByteArray&) {}

	virtual bool open(const QByteArray & //address
) { return false; }
	virtual QByteArray address() const = 0;
	virtual bool isOpen() const = 0;

	///Open the the filesystem if not already done
	void openIfNecessary();

	///Returns all root paths
	VipPathList roots();
	///Returns the list of standard attributes supported by this class
	QStringList standardAttributes();
	///Tells if a given path exists
	bool exists(const VipPath & path);
	///Creates a new path (file or directory)
	bool create(const VipPath & path);
	///Remove a path (file or directory)
	bool remove(const VipPath & path);

	/// Rename a file or directory. \a src and\a dst must be of the same type (file or directory).
	/// If the destination file/directory already exists and \a overwrite is true, it will be removed first.
	bool rename(const VipPath & src, const VipPath & dst, bool overwrite = false);

	///Move a file or directory to \a dst file or directory.
	/// If \a src is a file and \a dst is a file that already exists in destination folder, the destination file will be overwrite if \a merge is true.
	/// If \a src is a directory, the behavior is the following:
	/// <ul>
	/// <li> If the source directory name does not exists in the destination folder, just rename the source directory.</li>
	/// <li> If the source directory name already exists in the destination folder and \a merge is false, return false.</li>
	/// <li> If the source directory name already exists in the destination folder and \a merge is true, merge the source folder in the destination one, and remove the source folder.</li>
	/// </ul>
	bool move(const VipPath & src, const VipPath & dst, bool merge = false, VipProgress * progress = NULL);

	/// Copy a files or directory to \a dst file or directory, specifying if the destination directory or file should be overwritten.
	/// If \a overwrite is true and \a src is folder, its content will be merged with the destination folder.
	bool copy(const VipPath & src, const VipPath & dst, bool merge = false, VipProgress * progress = NULL);

	///Lists the content of a directory
	VipPathList list(const VipPath & path, bool recursive = false);
	///Opens a file and returns the related QIOdevice
	QIODevice * open(const VipPath & path, QIODevice::OpenMode mode);

	///Starts a search in given directory based on a regular expression and a filter.
	/// This function will launch the searching in a separate thread and return immediately.
	/// Search results are retrieved with the signal #VipMapFileSystem::found() or the function #VipMapFileSystem::searchResults().
	/// The signal #VipMapFileSystem::searchEnded() is emitted when the search stops (automatically or manually).
	/// This function will stop any previous search.
	void search(const VipPath & in_path, const QList<QRegExp> & exps, bool exact_match = false, QDir::Filters filters = QDir::AllEntries);

	///Stops the current search (if any)
	void stopSearch();

	///Returns the last search results after a call to #VipMapFileSystem::search()
	VipPathList searchResults() const;

	///Returns all supported operations
	SupportedOperations supportedOperations() const;
	void setSupportedOperations(SupportedOperations ops) {  m_operations = ops; }

	///Tells if an error occured during the last operation
	bool hasError() const;
	///Returns the error string for the last operation
	QString errorString() const;
	///Returns the error code for the last operation (or 0 if no error occured)
	int errorCode() const;

protected:
	///Returns the standard attributes for file or directory
	virtual QStringList standardFileSystemAttributes() = 0;
	///Returns the root directories for this VipMapFileSystem
	virtual VipPathList rootPaths() = 0;
	///Returns true if given file or directory exists
	virtual bool pathExists(const VipPath & path)  = 0;
	///Create a file or directory. #VipPath::isDir is used to determine which one should be created.
	/// This function should fail if given path already exists.
	virtual bool createPath(const VipPath & path);
	///Remove a file or directory
	virtual bool removePath(const VipPath & path);
	///Rename a file or a directory. VipMapFileSystem will ensure that \a src and \a dst are both directories or files.
	/// This function should fail if destiation file or folder already exists.
	virtual bool renamePath(const VipPath & src, const VipPath & dst);
	///Copy a source file into a destination file. VipMapFileSystem will ensure that \a src and \a dst are both files.
	/// This function should fail if destination file already exists.
	virtual bool copyPath(const VipPath & src, const VipPath & dst);
	///List the content (both files and directories) of a directory. VipMapFileSystem will ensure that \a path is a directory.
	/// The listing must not be recursive.
	virtual VipPathList listPathContent(const VipPath & path) = 0;
	/// Open a file in given \a mode and returns a QIODevice to manipulate this file.
	/// Returns a NULL QIODevice on failure.
	virtual QIODevice * openPath(const VipPath & path, QIODevice::OpenMode modes);

	void setError(const QString & error_str, int error_code = -2);
	void resetError();


Q_SIGNALS:
	void found(const VipPath &);
	void searchStarted();
	void searchEnterPath(const VipPath &);
	void searchEnded();

private:
	void listPathHelper(VipPathList & out, const VipPath & path);
	bool copyDirContentHelper(const VipPath & src_dir, const VipPath & dst_dir, bool overwrite, VipProgress * progress = NULL);

	int m_error_code;
	QString m_error_string;
	SupportedOperations m_operations;
	VipPathList m_found;
	SearchThread *m_search;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipMapFileSystem::SupportedOperations)
VIP_REGISTER_QOBJECT_METATYPE(VipMapFileSystem*)



/// \a VipAbstractFileSystem that represents the physical hard drive
class VIP_CORE_EXPORT VipPhysicalFileSystem : public VipMapFileSystem
{
	Q_OBJECT
public:
	VipPhysicalFileSystem();
	virtual bool open(const QByteArray & ) { return true; }
	virtual QByteArray address() const { return QByteArray(); }
	virtual bool isOpen() const { return true; }

	static bool exists_timeout(const QString& path, int milli_timeout = -1, bool * timed_out = NULL);
	static bool has_network_issues();
protected:
	virtual QStringList standardFileSystemAttributes();
	virtual VipPathList rootPaths();
	virtual bool pathExists(const VipPath & path);
	virtual bool createPath(const VipPath & path);
	virtual bool removePath(const VipPath & path);
	virtual bool renamePath(const VipPath & src, const VipPath & dst);
	virtual bool copyPath(const VipPath & src, const VipPath & dst);
	virtual VipPathList listPathContent(const VipPath & path);
	virtual QIODevice * openPath(const VipPath & path, QIODevice::OpenMode modes);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPhysicalFileSystem*)


#ifdef _WIN32

class VIP_CORE_EXPORT VipSFTPFileSystem : public VipMapFileSystem
{
	Q_OBJECT
public:
	VipSFTPFileSystem();
	~VipSFTPFileSystem();

	virtual bool open(const QByteArray&);
	virtual QByteArray address() const;
	virtual bool isOpen() const;
	virtual void setPassword(const QByteArray& pwd);
	virtual bool requirePassword() const { return true; }

protected:
	virtual QStringList standardFileSystemAttributes();
	virtual VipPathList rootPaths();
	virtual bool pathExists(const VipPath& path);
	virtual bool createPath(const VipPath& path);
	virtual bool removePath(const VipPath& path);
	virtual bool renamePath(const VipPath& src, const VipPath& dst);
	virtual bool copyPath(const VipPath& src, const VipPath& dst);
	virtual VipPathList listPathContent(const VipPath& path);
	virtual QIODevice* openPath(const VipPath& path, QIODevice::OpenMode modes);

	class PrivateData;
	PrivateData* d_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipSFTPFileSystem*)

#endif

#endif
