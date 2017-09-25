// fastman92 IMG Console.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "IMG.h"

#include <cstring>
#include <iostream>
#include <regex>
#include <Shlobj.h>
#include <string>

#ifdef _DEBUG
#include <vld.h>
#endif

using namespace std;

// #define IMG_CONSOLE_DEBUG

#define IMG_ENTRY_NAME_SIZE _countof(((IMG::IMG_version2_tableItem*)0) -> Name)

const char ProjectTitle[] = "fastman92 IMG console";
const char ProjectVersion[] = "1.9";

unsigned int LineNum;	// only in script mode
IMG Archive;

wchar_t RootPath [MAX_PATH];

	const char LevelOne[] = "  ";
	const char LevelTwo[] = "    ";
	const char LevelThree[] = "      ";

void PrintThisProjectInfo()
{
	cout << endl << endl << ProjectTitle << " " << ProjectVersion << " " <<
	#ifdef _WIN64
	  "64-bit"
	#else
	  "32-bit"
	#endif

	<< ", " << __DATE__ << endl << "(fastman92@gmail.com)" << endl;
}

enum CommandMode{SCRIPT_MODE, INTERACTIVE_MODE};
CommandMode ActiveMode;

enum MessageTypes
{
	MESSAGE_ERROR,
	MESSAGE_WARNING
};

void OutputMessage(MessageTypes type, const wchar_t * format, ...)
{
	va_list vl;
	va_start(vl, format);

	wchar_t buffer[4096];

	if(ActiveMode == SCRIPT_MODE)
		wsprintfW(buffer, L"(%d): ", LineNum);
	else
		buffer[0] = NULL;

		switch (type)
		{
		case MESSAGE_ERROR:
			wcscat(buffer, L"Error: ");
			break;
		case MESSAGE_WARNING:
			wcscat(buffer, L"Warning: ");
			break;
		}

	size_t l = wcslen(buffer);
	_vsnwprintf_s(&buffer[l], sizeof(buffer)-l, sizeof(buffer)-l-1, format, vl);

	wcout << buffer << endl;
}

void RebuildArchive()
{
	printf("Starting of IMG archive rebuilding\n");

	if(Archive.RebuildArchive())
		cout << "Archive rebuilt successfully" << endl;
	else
		OutputMessage(MESSAGE_ERROR, L"Unable to rebuild IMG archive.");
}

// returns true if rebuilding was neccessary
bool RebuildIfArchiveWasModified()
{
	if(Archive.hasArchiveBeenModifiedAndNotRebuiltYet())
	{
		RebuildArchive();
		return true;
	}
	else
		return false;
}

bool RebuildIfMoreUselessSpaceThanValue(unsigned __int64 Treshold, unsigned __int64* UnusedSpace)
{
	unsigned __int64 xUnusedSpace = Archive.GetSizeOfUnusedSpace();

	if(UnusedSpace)
		*UnusedSpace = xUnusedSpace;

	if(xUnusedSpace >= Treshold)
	{
		RebuildArchive();
		return true;
	}
	else
		return false;
}

bool requireArchiveToBeOpened()
{
	bool isOpened = Archive.IsArchiveOpened();

	if(!isOpened)
		OutputMessage(MESSAGE_ERROR, L"This command requires archive to be opened, it isn't opened currently.");
	
	return isOpened;
}



const char* SkipWhiteCharactersInLine(const char* Line)
{
	while(*Line == '\t' || *Line == ' ')
		Line++;

	return Line;
}

bool ReadInt(const char*& Line, int& num)
{
	Line = SkipWhiteCharactersInLine(Line);

	if(*Line == '0' && *(Line+1) == 'x')
	{
		Line += 2;
		num = strtol(Line, (char**)&Line, 16);
	}
	else if(*Line >= '0' && *Line <= '9')
		num = strtol(Line, (char**)&Line, 10);		
	else
	{
		OutputMessage(MESSAGE_ERROR, L"Unable to read integer value.");
		return false;
	}

	Line = SkipWhiteCharactersInLine(Line);
	return true;
}

bool ReadString(const char*& Line, string& str)
{
	Line = SkipWhiteCharactersInLine(Line);
	
	if(*Line++ != '"')
	{
		OutputMessage(MESSAGE_ERROR, L"String not found.");
		return false;
	}

	while(*Line)
	{
		const char c = *Line;

		if(c == '"')
		{
			Line++;
			Line = SkipWhiteCharactersInLine(Line);
			return true;
		}		

		if(c == '\\')
		{
			const char n = *++Line;
			Line++;

			switch(n)
			{
			// Alert (bell)
			case 'a':
				str += '\a';
				break;
			// Backspace
			case 'b':				
				str += '\b';
				break;

			// Form feed
			case 'f':		
				str += '\f';
				break;
					
			// Newline (line feed)
			case 'n':
				str += '\n';
				break;

			// Carriage return
			case 'r':
				str += '\r';
				break;

			// Horizontal tab
			case 't':
				str += '\t';
				Line += 2;
				break;

			// Vertical tab
			case 'v':
				str += '\v';
				break;

			// Character with hexadecimal value hh
			case 'x':
				char Character;
				Character = (char)strtol(Line, (char**)&Line, 16);
				str += Character;			
				break;
			case '\\':
				str += '\\';
				break;
			default:
				OutputMessage(MESSAGE_WARNING, L"'%c' : unrecognized character escape sequence", n);
			}			
		}
		else
		{
			str += c;
			Line += 1;
		}
	}

	OutputMessage(MESSAGE_ERROR, L"Unterminated string");
	return false;
}

void* LoadFileIntoMemory(const char* filePath, size_t* filesize)
{
	if(FILE* fp = fopen(filePath, "rb"))
	{
		_fseeki64(fp, 0, SEEK_END);
		__int64 fSize = _ftelli64(fp);

		if(fSize > Archive.MAX_FILESIZE)
		{
			OutputMessage(MESSAGE_ERROR, L"Size of file \"%hs\" is too big to store in IMG archive.", filePath);
			fclose(fp);
			return false;
		}

		_fseeki64(fp, 0, SEEK_SET);

		void* fileContent;
		
		fileContent = new char[(unsigned int)fSize];

		fread(fileContent, 1, (size_t)fSize, fp);

		*filesize = (size_t)fSize;
		fclose(fp);		

		return fileContent;
	}

	return nullptr;
}



unsigned int OrdinalNum;
inline void InitProjectList()
{
	OrdinalNum = 1;
	cout << LevelOne << "fastman92 projects";
}

void PrintProjectInfo(const char* ProjectName, const char* ProjectDescription)
{
	cout
	<< endl << LevelOne << OrdinalNum++ << ". " << ProjectName
	<< endl << LevelTwo << ProjectDescription;
}

void OutputErrorFileDoesNotExist(const char* filename)
{
	OutputMessage(MESSAGE_ERROR, L"File %hs does not exist in archive.", filename);
}

void OutputErrorUnableToOpenDirectory(const char* dirname)
{
	OutputMessage(MESSAGE_ERROR, L"Unable to open directory \"%hs\".", dirname);
}

void OutputWarningFileAlreadyExistsInArchiveOverwritingIt(const char* fileName)
{
	OutputMessage(MESSAGE_WARNING, L"File %hs already exists in archive, overwriting it", fileName);
}

void OutputErrorUnableToReadFileA(const char* filename)
{
	OutputMessage(MESSAGE_ERROR, L"Unable to read file %hs", filename);
}

void ImportFile(const char* filePath)
{
	const void* fileContent;
	size_t filesize;
						
	if(fileContent = LoadFileIntoMemory(filePath, &filesize))
	{
		char* filename = PathFindFileNameA(filePath);

		if(Archive.FileExists(filename))
			OutputWarningFileAlreadyExistsInArchiveOverwritingIt(filename);

		if(Archive.AddOrReplaceFile(filename, fileContent, filesize) != Archive.end())
			cout << "Imported file: " << filename << endl;
		else
			OutputMessage(MESSAGE_ERROR, L"File read from disk, but cannot import file %hs", filename);
		
		delete fileContent;
	}
	else
		OutputErrorUnableToReadFileA(filePath);
}

// Opens directory if exists or creates and opens directory if it doesn't initially exists
bool OpenOrCreateDirectory(const char* dirname)
{
	if (SetCurrentDirectoryA(dirname) || (!SHCreateDirectoryExA(NULL, dirname, NULL) && SetCurrentDirectoryA(dirname)))
		return true;
	else
	{
		char AbsoluteDirname[MAX_PATH];
		GetFullPathNameA(dirname, _countof(AbsoluteDirname), AbsoluteDirname, NULL);

		if(!SHCreateDirectoryExA(NULL, AbsoluteDirname, NULL) && SetCurrentDirectoryA(dirname))
			return true;
		else
		{
			OutputMessage(MESSAGE_ERROR, L"Unable to open or create directory \"%hs\".", dirname);
			return false;
		}
	}
}

bool ProcessCommand(const char* Line)
{
	Line = SkipWhiteCharactersInLine(Line);
	
	if(*Line == '-')
		Line++;
	else
	{		
		if(*Line != NULL && *Line != '\n' && !(*Line == '/' && *(Line+1) == '/'))
			OutputMessage(MESSAGE_ERROR, L"'-' expected, but '%c' found.", *Line);

		return false;
	}

	char CommandName[64];
	unsigned int CommandNameLen = 0;

	while(char c = *Line)
	{
		if(CommandNameLen < (_countof(CommandName)-1))
		{
			if(isalnum(c))
				CommandName[CommandNameLen++] = tolower(c);
			else
				break;

			Line++;
		}
		else
		{
			OutputMessage(MESSAGE_ERROR, L"Command name may not be longer than %d characters.", _countof(CommandName)-1);
			return false;
		}
	}
	CommandName[CommandNameLen] = NULL;

#define IsCommandNameEqual(Name) (CommandNameLen == _countof(Name)-1 && !strcmp(CommandName, Name))

	if(!CommandNameLen)
		cout << "Command name not found." << endl;
	else if (IsCommandNameEqual("exit"))
	{
		Archive.CloseArchive();
		return true;
	}
	else if (IsCommandNameEqual("close"))
	{
		if(Archive.IsArchiveOpened())
		{
			Archive.CloseArchive();
			cout << "Archive closed" << endl;
		}
		else
			OutputMessage(MESSAGE_ERROR, L"Archive isn't opened, can't close");
	}
	else if(IsCommandNameEqual("help"))
	{
		cout << LevelOne << "Commands (Script and Interactive)"

		<< endl << LevelTwo << "-open \"path\\\\to\\\\filename.img\""
		<< endl << LevelThree << "Opens IMG archive"

		<< endl << LevelTwo << "-create \"path\\to\\filename.img\" \"2\""
		<< endl << LevelThree << "Creates IMG archive. The second parameter is archive version."

		<< endl << LevelTwo << "-openOrCreate \"path\\\\to\\\\filename.img\" \"2\""
		<< endl << LevelThree << "Opens IMG archive if exists or creates archive if it does not exist. The second parameter is archive version."

		<< endl << LevelTwo << "-rebuild"
		<< endl << LevelThree << "Rebuilds IMG archive always when executed."

		<< endl << LevelTwo << "-rebuildIfMoreUselessSpaceThan 31457280"
		<< endl << LevelThree << "Rebuilds IMG archive only when amount of useless space is greater than specified amount of bytes. More than 30 MB in example."

		<< endl << LevelTwo << "-rebuildIfArchiveWasModified"		
		<< endl << LevelThree << "Rebuilds IMG archive only if archive was modified in any way. Archive was modified if e.g some files were imported/removed/renamed."
		
		<< endl << LevelTwo << "-rebuildIfMoreUselessSpaceThanValueOrArchiveWasModified 31457280"
		<< endl << LevelThree << "Rebuilds IMG archive only if archive was modified in any way / if there is more useless space specified value."

		<< endl << LevelTwo << "-add \"path\\\\to\\\\filename.dff\""
		<< endl << LevelThree << "Adds a file, it will always add a file, even though it may already exists."
		" It won't replace existing file."

		<< endl << LevelTwo << "-import \"path\\\\to\\\\filename.dff\""
		<< endl << LevelThree << "Imports file - file will be added if not exists or replaced if it exists."

		<< endl << LevelTwo << "-importFromDirectory \"path\\\\to\\\\directory\\\\\""
		<< endl << LevelThree << "Imports files from specified directory - files will be added if they don't exist or replaced if they exist."

		<< endl << LevelTwo << "-export \"filename.dff\" \"path\\\\to\\\\export\\\\\""
		<< endl << LevelThree << "Exports file."

		<< endl << LevelTwo << "-exportAll \"path\\\\to\\\\export\\\\\""
		<< endl << LevelThree << "Exports all files to specified directory."

		<< endl << LevelTwo << "-delete \"filename.dff\""
		<< endl << LevelThree << "Deletes file from archive"

		<< endl << LevelTwo << "-rename \"oldname.dff\"" << " \"newname.dff\""
		<< endl << LevelThree << "Renames file."

		<< endl << LevelTwo << "-getAmountOfUnusedSpace"
		<< endl << LevelThree << "Returns amount of unused space in IMG archive"

		<< endl << LevelTwo << "-printString \"message to print\""
		<< endl << LevelThree << "Prints message, useful for script mode."

		<< endl << LevelTwo << "-close"
		<< endl << LevelThree << "Closes handle of IMG archive"

		<< endl << LevelTwo << "-exit"
		<< endl << LevelThree << "Quits fastman92 IMG console, process is terminated."

		<< endl << endl << LevelOne << "IMG versions:"
		<< endl << LevelTwo << "\"1\" - GTA III, GTA VC, GTA III Mobile"
		<< endl << LevelTwo << "\"2\" - GTA SA"

		<< endl << endl << LevelOne << "String escape characters:"
		<< endl << LevelTwo << "\\a - Bell (alert)"
		<< endl << LevelTwo << "\\b - Backspace"
		<< endl << LevelTwo << "\\f - Formfeed"
		<< endl << LevelTwo << "\\n - New line"
		<< endl << LevelTwo << "\\r - Carriage return"
		<< endl << LevelTwo << "\\t - Horizontal tab"
		<< endl << LevelTwo << "\\v - Vertical tab"
		<< endl << LevelTwo << "\\\" - Double quotation mark"
		<< endl << LevelTwo << "\\\\ - Backslash"
		<< endl << LevelTwo << "\\xhh - ASCII character in hexadecimal notation, where hh is the number of character"

		<< endl;
	}
#ifdef IMG_CONSOLE_DEBUG
	else if(IsCommandNameEqual("debug"))
	{
		if(requireArchiveToBeOpened())
		{
			// Archive.DebugListOfEntries();
			// Archive.FindFirstEmptySpaceForFile(8192);
			
			vector<DWORD> Matches;

			// Archive.GetIDsOfAssociatedFiles(Archive.begin() + Archive.GetFileIndexByName("law_stream1.ipl"), Matches);	
			
			// x++;

			// for(auto i = Matches.begin(); i < Matches.end(); i++)
			{
				// cout << "File index: " << *i << endl;
			}
			cout << endl;
			
			// cout << endl << "Good position: " << Archive.FindFirstEmptySpaceForFile(8192) << endl;
		}
	}
#endif
	else if(IsCommandNameEqual("open"))
	{
		string archivePath;

		if(ReadString(Line, archivePath))
		{
			SetCurrentDirectoryW(RootPath);

			if(Archive.OpenArchive(archivePath.c_str()))
				cout << "Archive \"" << archivePath << "\" opened." << endl;
			else
				OutputMessage(MESSAGE_ERROR, L"Unable to open IMG archive named \"%hs\"", archivePath.c_str());
		}
	}
	else if(IsCommandNameEqual("rebuild"))
	{
		if(requireArchiveToBeOpened())
			RebuildArchive();
	}
	else if(IsCommandNameEqual("rebuildifarchivewasmodified"))
	{
		if(requireArchiveToBeOpened())
		{
			if(!RebuildIfArchiveWasModified())
				printf("No need to rebuild archive - it was unmodified or rebuilt recently.\n");
		}
	}
	else if(IsCommandNameEqual("rebuildifmoreuselessspacethan"))
	{
		if(requireArchiveToBeOpened())
		{
			int Treshold;

			if(ReadInt(Line, Treshold))
			{
				unsigned __int64 UnusedSpace;

				if(RebuildIfMoreUselessSpaceThanValue(Treshold, &UnusedSpace))
					printf("No need to rebuild this IMG archive.\nAmount of useless space is equal to %lld bytes.\n", UnusedSpace);		
			}
		}
	}
	else if(IsCommandNameEqual("rebuildifmoreuselessspacethanvalueorarchivewasmodified"))
	{
		if(requireArchiveToBeOpened())
		{
			int Treshold;

			if(ReadInt(Line, Treshold))
			{
				if(!RebuildIfArchiveWasModified() && !RebuildIfMoreUselessSpaceThanValue(Treshold, nullptr))
					printf("No need to rebuild this IMG archive.\n");
			}
		}
	}
	else if(IsCommandNameEqual("getamountofunusedspace"))
	{
		if(requireArchiveToBeOpened())
		{
			unsigned __int64 UnusedSpace = Archive.GetSizeOfUnusedSpace();

			printf("Amount of unused space: %d bytes", UnusedSpace);
		}
	}
	else if(IsCommandNameEqual("add"))
	{
		string filePath;
		const void* fileContent;
		size_t filesize;		

		if(ReadString(Line, filePath) && requireArchiveToBeOpened())
		{
			SetCurrentDirectoryW(RootPath);

			if(fileContent = LoadFileIntoMemory(filePath.c_str(), &filesize))
			{
				char* filename = PathFindFileNameA(filePath.c_str());

				Archive.AddFile(filename, fileContent, filesize);

				cout << "Added new file: " << filename << endl;

				delete fileContent;
			}
			else			
				OutputMessage(MESSAGE_ERROR, L"Unable to read file %hs", filePath.c_str());			
		}		
	}

	else if(IsCommandNameEqual("export"))
	{
		string fileName;
		string exportPath;

		if(ReadString(Line, fileName) && ReadString(Line, exportPath) && requireArchiveToBeOpened())
		{
			IMG::tIMGEntryIterator it;

			if((it = Archive.GetFileIteratorByName(fileName.c_str())) != Archive.end())
			{
				auto& file = *it;
				size_t fileSize = file.GetFilesize();				

				FILE * fp;

				SetCurrentDirectoryW(RootPath);

				if(OpenOrCreateDirectory(exportPath.c_str()))
				{
					if(fp = fopen(fileName.c_str(), "wb"))
					{					
						char* fileContent = new char[file.GetFilesize()];

						file.ReadEntireFile(fileContent);
						fwrite(fileContent, fileSize, 1, fp);
						fclose(fp);

						delete fileContent;

						cout << "File " << fileName << " exported successfully" << endl;
					}
					else
						OutputMessage(MESSAGE_ERROR, L"Unable to create \"%hs\"", fileName.c_str());
				}
			}
			else
				OutputErrorFileDoesNotExist(fileName.c_str());
		}
	}
	else if(IsCommandNameEqual("exportall"))
	// -exportAll  "path\\to\\export\\"
	{
		string exportPath;

		if(ReadString(Line, exportPath) && requireArchiveToBeOpened())
		{
			SetCurrentDirectoryW(RootPath);

			if(OpenOrCreateDirectory(exportPath.c_str()))
			{
				for(auto& file = Archive.begin(); file != Archive.end(); file++)
				{
					DWORD fileSize = file -> GetFilesize();
					char* fileContent = new char[fileSize];
					const char* fileName = file -> GetFilename();

					file -> ReadEntireFile(fileContent);

					if(FILE* fp = fopen(fileName, "wb"))
					{
						fwrite(fileContent, fileSize, 1, fp);
						fclose(fp);
						cout << "File " << fileName << " exported successfully." << endl;
					}
					else
						OutputMessage(MESSAGE_ERROR, L"Unable to create \"%hs\"", fileName);

					delete fileContent;					
				}				
			}				
		}
	}
	else if(IsCommandNameEqual("importfromdirectory"))
	{
		string dirPath;

		if(ReadString(Line, dirPath) && requireArchiveToBeOpened())
		{
			SetCurrentDirectoryW(RootPath);

			if(SetCurrentDirectoryA(dirPath.c_str()))
			{
				_WIN32_FIND_DATAA fd;

				HANDLE File = FindFirstFileA ("*", &fd);

				if (File != INVALID_HANDLE_VALUE)
				{			
					const void* fileContent = NULL;

					do {
						if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
							// avoid standard library functions to reduce dll size

							ImportFile(fd.cFileName);
						}

					} while (FindNextFileA (File, &fd));
					FindClose (File);					


					return false;
				}

				SetCurrentDirectoryW(RootPath);
			}
			else		
				OutputErrorUnableToOpenDirectory(dirPath.c_str());			
		}
	}	
	else if(IsCommandNameEqual("import"))
	{
		string filePath;	

		if(ReadString(Line, filePath) && requireArchiveToBeOpened())
		{
			SetCurrentDirectoryW(RootPath);

			ImportFile(filePath.c_str());
		}
	}
	else if(IsCommandNameEqual("rename"))
	{
		string oldName;
		string newName;

		if(ReadString(Line, oldName) && ReadString(Line, newName) && requireArchiveToBeOpened())
		{
			IMG::tIMGEntryIterator it;

			if((it = Archive.GetFileIteratorByName(oldName.c_str())) != Archive.end())
			{
				Archive.RenameFile(it, newName.c_str());
				cout << "Renamed file " << oldName << " to " << it -> GetFilename() << endl;
			}
			else
				OutputErrorFileDoesNotExist(oldName.c_str());
		}
	}
	else if(IsCommandNameEqual("delete"))
	{
		string FileName;

		if(ReadString(Line, FileName) && requireArchiveToBeOpened())
		{
			IMG::tIMGEntryIterator it;

			if((it = Archive.GetFileIteratorByName(FileName.c_str())) != Archive.end())
			{
				Archive.RemoveFile(it);
				cout << "Removed file " << FileName.c_str() << endl;
			}
			else
				OutputErrorFileDoesNotExist(FileName.c_str());
		}
	}
	else if(IsCommandNameEqual("save"))
		OutputMessage(MESSAGE_WARNING, L"-save command isn't currently implemented to fastman92 IMG Console, it may be implemented later");
	else if(IsCommandNameEqual("fastman92"))
	{
		InitProjectList();
		PrintProjectInfo("Analog clock", "Analog clock near the HUD.");
		PrintProjectInfo("Business Mod", "Assets make money everyday, BANK, paying in and out.");
		PrintProjectInfo("Car spawner", "Includes two sorting algorithms, ranges, categories, full trailers, config in .ini");
		PrintProjectInfo("Cheat Strings Loader", "Modify the cheat strings and share new .dat files!");
		PrintProjectInfo("fastman92 IMG Console", "Creates/edits IMG archives in interactive mode and script mode.");
		PrintProjectInfo("Free ID List Generator", "Useful for people who add new objects, peds, vehicles, anything.");
		PrintProjectInfo("GTA IV Window Crash", "Throws away player if he has hit anything heavily.");
		PrintProjectInfo("GTA San Andreas Cursor Pack", "New cursors, Windows Seven Aero cursor is available.");
		PrintProjectInfo("IMG Organizer", "Splits gta3.img & gta_int.img to many smaller IMG archives with structure inspired by GTA IV.");
		PrintProjectInfo("IMG & Stream Limit Adjuster", "Adjusts number of IMG archives possible to load (defined in default.dat / gta.dat or IMGLIST file).");
		PrintProjectInfo("In-game Timecyc Editor", "Edit weather apperance (timecyc.dat) directly in game.");
		PrintProjectInfo("Language Loader", "It adjusts number of languages, up to 11 languages now");
		PrintProjectInfo("Native IMG Builder", "Builds IMG archives in native way, like Rockstar Games may have done it");
		PrintProjectInfo("Remote Lock Car", "Control remotely your car. engine, lights, doors, load, siren.");
		PrintProjectInfo("Screen Ratio Fix", "Fixes screen ratio, result is visible on irregular resolutions.");
		PrintProjectInfo("Stream Memory Fix", "High-res textures don't disappear anymore.");
		PrintProjectInfo("Taxi IDs Loader", "Loads IDs used by 0602 opcode.");
		PrintProjectInfo("Traffic Switcher", "Enable traffic by \"NOTRAFFIC\" and \"TRAFFIC\" codes.");
		PrintProjectInfo("Vehicle Audio Loader", "Lets you edit audio properties e.g engine sound ID.");
		PrintProjectInfo("Waterworld", "Go or drive underwater, water is still visible.");

		cout << endl;	 
	}
	else if(IsCommandNameEqual("hell"))
	{
		SetConsoleTextAttribute( GetStdHandle( STD_OUTPUT_HANDLE ), FOREGROUND_RED | FOREGROUND_INTENSITY);
		cout << "You won't get out of here alive. It's hell." << endl;

		const int triangle_size = 30;

		// Draw triangles
		for(int y = 0; y < triangle_size; y++)
		{
			for(int x = 0; x < triangle_size - y; x++)
				cout << " ";

			for(int x = 0; x < y*2; x++)
				cout << "*";

			cout << endl;
		}

		for(int y = triangle_size; y > 0; y--)
		{
			for(int x = 0; x < triangle_size - y; x++)
				cout << " ";

			for(int x = 0; x < y*2; x++)
				cout << "*";

			cout << endl;
		}
	}
	else if(IsCommandNameEqual("printstring"))
	{
		string message;

		if(ReadString(Line, message))
			cout << message << endl;
	}
	else
	{				 
		string archivePath;
		string Version;

		BYTE command;

		if(IsCommandNameEqual("create"))
			command = 1;
		else if(IsCommandNameEqual("openorcreate"))
			command = 2;
		else
			command = false;		

		if(command)
		{
			if(ReadString(Line, archivePath) && ReadString(Line, Version))
			{
				eIMG_version archiveVersion;

				if(!_stricmp(Version.c_str(), "1"))
					archiveVersion = IMG_VERSION_1;
				else if(!_stricmp(Version.c_str(), "2"))
					archiveVersion = IMG_VERSION_2;
				else
				{
					OutputMessage(MESSAGE_ERROR, L"Unknown version: \"%hs\"", Version.c_str());
					return false;
				}

				SetCurrentDirectoryW(RootPath);
			
				if(command == 1)
				{
					if(Archive.CreateArchive(archivePath.c_str(), archiveVersion))
						cout << "Archive \"" << archivePath << "\" created." << endl;
					else
						OutputMessage(MESSAGE_ERROR, L"Unable to create IMG archive named \"%hs\"", archivePath.c_str());
				}
				else
				{
					if(Archive.OpenOrCreateArchive(archivePath.c_str(), archiveVersion))
						cout << "Archive \"" << archivePath << "\" opened/created." << endl;
					else
						OutputMessage(MESSAGE_ERROR, L"Unable to open/create IMG archive named \"%hs\"", archivePath.c_str());
				}
			}		
		}
		else
			OutputMessage(MESSAGE_ERROR, L"Unknown command: -%hs", CommandName);			
	}

	return false;
}

int wmain(int argc, const wchar_t* argv[])
{
	SetConsoleTitleA(ProjectTitle);

	PrintThisProjectInfo();

	if(argc > 1)
	{
		if(!wcscmp(argv[1], L"-script"))
		{
			if(argc > 2)
			{
				ActiveMode = SCRIPT_MODE;

				if(FILE* script_fp = _wfopen(argv[2], L"r"))
				{					
					wchar_t* filePart;

					GetFullPathNameW(argv[2], _countof(RootPath), RootPath, &filePart);

					*filePart = NULL;

					char CommandLine[256];

					LineNum = 0;

					while(fgets(CommandLine, sizeof(CommandLine), script_fp))
					{
						LineNum++;
						ProcessCommand(CommandLine);
					}
				}
				else
					wcout << endl << endl << "Error: Unable to open script file " << argv[2];
			}
			else
				cout << endl << "No argument (path) for -script found";
		}
	}
	else
	{
		GetCurrentDirectoryW(_countof(RootPath), RootPath);

		cout << endl << "Interactive Mode. For help, type in -help. Exit with -exit" << endl;	

		ActiveMode = INTERACTIVE_MODE;

		string CommandLine;

		// ProcessCommand("-open \"LAWn.img\"");
		// ProcessCommand("-getAmountOfUnusedSpace");
		// ProcessCommand("-importFromDirectory \"goodpeds\"");

		bool isExit;

		do
		{
			cout << ">> ";
			getline (cin, CommandLine);

			isExit = ProcessCommand(CommandLine.c_str());
		}
		while(!isExit);
	}
	return 0;
}