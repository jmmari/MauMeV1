#include <FS.h>
#include <SPIFFS.h>
#include <sys/dir.h>

// #define MAUME_DEBUG 

typedef struct {
  char ** fileNames;
  time_t * lwTimes; // last write times
  byte nbFiles;
}NamesList;
int IRAM_ATTR fileExists(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirName);
//int fileExists(fs::FS &fs, SemaphoreHandle_t mutex, String dirName);
File IRAM_ATTR fileOpen(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirName);
File IRAM_ATTR fileOpenFor(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirName, const char* mod);
File IRAM_ATTR getNextFile(SemaphoreHandle_t mutex, File root);
int IRAM_ATTR writeBytes(SemaphoreHandle_t mutex, File aFile, const uint8_t * bytes, size_t len);
size_t IRAM_ATTR readSomeBytes(SemaphoreHandle_t mutex, File aFile, char * bytes, size_t len);
int deleteDirectoryFiles(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname);
String listDir2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, const char * bReturn);
//String listReceived2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname);
void sortFileList(NamesList * namesList);
NamesList * getDirList(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname);
void listDir(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, uint8_t levels);
int createDir(fs::FS &fs, const char * path);
int removeDir(fs::FS &fs, const char * path);
String IRAM_ATTR readFile(fs::FS &fs, SemaphoreHandle_t mutex, String path);
//int writeFile(fs::FS &fs, const char * path, const char * message);
int IRAM_ATTR writeBytesToFile(fs::FS &fs, SemaphoreHandle_t mutex, char * path, byte * bytes, int len);
int IRAM_ATTR appendFile(fs::FS &fs, SemaphoreHandle_t mutex, String path, String message);
int renameFile(fs::FS &fs, const char * path1, const char * path2);
int deleteFile(fs::FS &fs, SemaphoreHandle_t mutex, const char * path);
