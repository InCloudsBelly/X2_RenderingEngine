#pragma once

#include "X2/Core/Buffer.h"

// NOTE(Peter): This is really just a workaround Microsoft being extremely dumb and thinking no one would ever use the name CreateDirectory, etc... for a method...
//				If this causes any issues in the future we'll just remove these. I thought about renaming the methods, but that doesn't really work for DeleteFile, MoveFile or CopyFile
//				Seriously, I don't like using undef, but it's just such an annoying situation.
#ifdef CreateDirectory
#undef CreateDirectory
#undef DeleteFile
#undef MoveFile
#undef CopyFile
#undef SetEnvironmentVariable
#undef GetEnvironmentVariable
#endif

#include <functional>
#include <filesystem>

namespace X2 {

	enum class FileSystemAction
	{
		Added, Rename, Modified, Delete
	};

	struct FileSystemChangedEvent
	{
		FileSystemAction Action;
		std::filesystem::path FilePath;
		bool IsDirectory;

		// If this is a rename event the new name will be in the FilePath
		std::string OldName = "";
	};

	class FileSystem
	{
	public:
		static bool CreateDirectory(const std::filesystem::path& directory);
		static bool CreateDirectory(const std::string& directory);
		static bool Exists(const std::filesystem::path& filepath);
		static bool Exists(const std::string& filepath);
		static bool DeleteFile(const std::filesystem::path& filepath);
		static bool MoveFile(const std::filesystem::path& filepath, const std::filesystem::path& dest);
		static bool CopyFile(const std::filesystem::path& filepath, const std::filesystem::path& dest);
		static bool IsDirectory(const std::filesystem::path& filepath);

		static bool IsNewer(const std::filesystem::path& fileA, const std::filesystem::path& fileB);

		static bool Move(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath);
		static bool Copy(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath);
		static bool Rename(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath);
		static bool RenameFilename(const std::filesystem::path& oldFilepath, const std::string& newName);

		static bool ShowFileInExplorer(const std::filesystem::path& path);
		static bool OpenDirectoryInExplorer(const std::filesystem::path& path);
		static bool OpenExternally(const std::filesystem::path& path);

		static bool WriteBytes(const std::filesystem::path& filepath, const Buffer& buffer);
		static Buffer ReadBytes(const std::filesystem::path& filepath);

		static std::filesystem::path GetUniqueFileName(const std::filesystem::path& filepath);

	public:
		using FileSystemChangedCallbackFn = std::function<void(const std::vector<FileSystemChangedEvent>&)>;

		static void AddFileSystemChangedCallback(const FileSystemChangedCallbackFn& callback);
		static void ClearFileSystemChangedCallbacks();
		static void StartWatching();
		static void StopWatching();

		static std::filesystem::path OpenFileDialog(const char* filter = "All\0*.*\0");
		static std::filesystem::path OpenFolderDialog(const char* initialFolder = "");
		static std::filesystem::path SaveFileDialog(const char* filter = "All\0*.*\0");

		static std::filesystem::path GetPersistentStoragePath();

		static void SkipNextFileSystemChange();

	public:
		static bool HasEnvironmentVariable(const std::string& key);
		static bool SetEnvironmentVariable(const std::string& key, const std::string& value);
		static std::string GetEnvironmentVariable(const std::string& key);

	private:
		static unsigned long Watch(void* param);

	private:
		static std::vector<FileSystemChangedCallbackFn> s_Callbacks;
	};
}
