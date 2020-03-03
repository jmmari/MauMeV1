#include "SpifFsFunctions.hpp"
//----------------------------------------------------------------------------------------------------------------------------
int IRAM_ATTR fileExists(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirName){
  int ret = 0;
  #ifdef MAUME_DEBUG 
    Serial.println(" -> Checking if file exists..."); 
  #endif
  if(mutex){
    while(!xSemaphoreTakeRecursive(mutex, portMAX_DELAY));
      ret = fs.exists(dirName);
    xSemaphoreGiveRecursive(mutex);
  }else{
    Serial.println("                                                                            No mutex provided");
    ret = fs.exists(dirName);
  }
  return ret;
}
//----------------------------------------------------------------------------------------------------------------------------
/*int fileExists(fs::FS &fs, SemaphoreHandle_t mutex, String dirName){
  int ret = 0;
  if(mutex){
    xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
    ret = fs.exists(dirName.c_str());
    xSemaphoreGiveRecursive(mutex);
  }else{
    ret = fs.exists(dirName.c_str());
  }
  return ret;
}*/
//----------------------------------------------------------------------------------------------------------------------------
File IRAM_ATTR fileOpen(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirName){
  File ret;
  if(mutex){
    while(!xSemaphoreTakeRecursive(mutex, portMAX_DELAY));
    ret = fs.open(dirName);
    xSemaphoreGiveRecursive(mutex);
  }else{
    Serial.println("                                                                            No mutex provided");
    ret = fs.open(dirName);
  }
  return ret;
}
//----------------------------------------------------------------------------------------------------------------------------
File IRAM_ATTR fileOpenFor(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirName, const char* mod){
  File ret;
  if(mutex){
    while(!xSemaphoreTakeRecursive(mutex, portMAX_DELAY));
    ret = fs.open(dirName, mod);
    xSemaphoreGiveRecursive(mutex);
  }else{
    Serial.println("                                                                            No mutex provided");
    ret = fs.open(dirName, mod);
  }
  return ret;
}

//----------------------------------------------------------------------------------------------------------------------------
File IRAM_ATTR getNextFile(SemaphoreHandle_t mutex, File root){
  File ret;
  if(mutex){
    while(!xSemaphoreTakeRecursive(mutex, portMAX_DELAY));
    ret = root.openNextFile();
    xSemaphoreGiveRecursive(mutex);
  }else{
    Serial.println("                                                                            No mutex provided");
    ret = root.openNextFile();
  }
  return ret;
}
//----------------------------------------------------------------------------------------------------------------------------
int IRAM_ATTR writeBytes(SemaphoreHandle_t mutex, File aFile, const uint8_t * bytes, size_t len){
  int ret = 0;
  if(mutex){
    while(!xSemaphoreTakeRecursive(mutex, portMAX_DELAY));
    ret = (int)aFile.write(bytes, len);
    xSemaphoreGiveRecursive(mutex);
  }else{
    Serial.println("                                                                            No mutex provided");
    ret = (int)aFile.write(bytes, len);
  }
  return ret;
}
//----------------------------------------------------------------------------------------------------------------------------
// size_t nbRd = file.readBytes((char *)lrPkt, (size_t)sizeof(LoRa_PKT));
size_t IRAM_ATTR readSomeBytes(SemaphoreHandle_t mutex, File aFile, char * bytes, size_t len){
  size_t ret = 0;
  if(mutex){
    while(!xSemaphoreTakeRecursive(mutex, portMAX_DELAY));
    //ret = (int)aFile.write(bytes, len);
    ret = aFile.readBytes(bytes, len);
    xSemaphoreGiveRecursive(mutex);
  }else{
    Serial.println("                                                                            No mutex provided");
    ret = aFile.readBytes(bytes, len);
  }
  return ret;
}
//----------------------------------------------------------------------------------------------------------------------------
int IRAM_ATTR deleteFile(fs::FS &fs, SemaphoreHandle_t mutex, const char * path){
    int ret = 0;
  if(mutex){
    while(!xSemaphoreTakeRecursive(mutex, portMAX_DELAY));
    ret = fs.remove(path);
    xSemaphoreGiveRecursive(mutex);
  }else{
    Serial.println("                                                                            No mutex provided");
    ret = fs.remove(path);
  }
  if(!ret){Serial.println("Delete failed");}
  return ret;
}
//----------------------------------------------------------------------------------------------------------------------------
int deleteDirectoryFiles(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname) {
  int did = 0;
  File root = fileOpen(fs, mutex, dirname);
  if(!root){
     Serial.println(" -< Failed to open directory");
     return did;
  }
  if(!root.isDirectory()){
     Serial.println(" -< Not a directory");
     return did;
  }
  File file = getNextFile(mutex, root);
  while (file) {
    if(deleteFile(fs, mutex, file.name())){
      did++;
    }
    file = getNextFile(mutex, root);
  }
  root.close();
  file.close();
  return did;
}
//----------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR listDir2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, const char * bReturn) {
  String str = "";
  File root = fileOpen(fs, mutex, dirname);
  if(!root){
     Serial.println(" -< Failed to open directory !");
     return " -< Failed to list directory.";
  }
  if(!root.isDirectory()){
     Serial.println(" -< Not a directory !");
     return " -< Not a directory.";
  }
  File file = getNextFile(mutex, root);
  while (file) {
    if(strlen(file.name())>0){
      str += file.name();
      str += bReturn;
    }
    file = getNextFile(mutex, root);
  }
  //Serial.print(str);
  root.close();
  file.close();
  return str;
}
//----------------------------------------------------------------------------------------------------------------------------
void sortFileList(NamesList * namesList){
  if(namesList->nbFiles <= 0){
    return;
  }
  time_t temp;
  char* str = NULL;
  for (int i = 0; i < namesList->nbFiles; i++){
    for (int j = 0; j < (namesList->nbFiles - i - 1); j++){
      if (namesList->lwTimes[j] > namesList->lwTimes[j + 1]){
        temp = namesList->lwTimes[j];
        str = namesList->fileNames[j];
        namesList->lwTimes[j] = namesList->lwTimes[j + 1];
        namesList->fileNames[j] = namesList->fileNames[j + 1];
        namesList->lwTimes[j + 1] = temp;
        namesList->fileNames[j + 1] = str;
      }
    }
  }
}
//----------------------------------------------------------------------------------------------------------------------------
NamesList * getDirList(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname){
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" * Listing directory : "+String(dirname));
  #endif
  NamesList * namesList = (NamesList *)malloc(sizeof(NamesList));
  namesList->fileNames = NULL;
  namesList->lwTimes = NULL;
  namesList->nbFiles = 0;
  
  File root = fileOpen(fs, mutex, dirname);
  if(!root){
    free(namesList);namesList = NULL;
    Serial.println(" -< Failed to open directory");
    return NULL;
  }
  if(!root.isDirectory()){
    free(namesList);namesList = NULL;
    Serial.println(" -< Not a directory");
    return NULL;
  }
  int fileCount = 0;
  File file = getNextFile(mutex, root);
  while(file){
      if(!file.isDirectory()){
          fileCount++;
      }
      file = getNextFile(mutex, root);
  }
  root.rewindDirectory();
  // Serial.println("   > Allocating...");
  namesList->fileNames = (char**)malloc(fileCount*sizeof(char*));
  namesList->lwTimes = (time_t*)malloc(fileCount*sizeof(time_t*));
  file = getNextFile(mutex, root);
  while(file){
      if(!file.isDirectory()){
        namesList->fileNames[namesList->nbFiles] = (char*)malloc(32);
        namesList->fileNames[namesList->nbFiles][0] = '\0';
        strcpy(namesList->fileNames[namesList->nbFiles], file.name());
        namesList->lwTimes[namesList->nbFiles] = file.getLastWrite();
        namesList->nbFiles ++;
      }
      file = getNextFile(mutex, root);
  }
  root.close();
  file.close();
  return namesList;
}
//----------------------------------------------------------------------------------------------------------------------------
void listDir(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, uint8_t levels){
  Serial.printf(" * Listing directory: %s\n", dirname);
  File root = fileOpen(fs, mutex, dirname);
  if(!root){Serial.println(" -< Failed to open directory");return;}
  if(!root.isDirectory()){Serial.println(" -< Not a directory");return;}
  File file = getNextFile(mutex, root);
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.print (file.name());
      time_t t= file.getLastWrite();
      struct tm * tmstruct = localtime(&t);
      Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
      if(levels){
        listDir(fs, mutex, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.print(String(file.size()));
      time_t t= file.getLastWrite();
      struct tm * tmstruct = localtime(&t);
      Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
    }
    file = getNextFile(mutex, root);
  }
  root.close();
  file.close();
}
//----------------------------------------------------------------------------------------------------------------------------
int createDir(fs::FS &fs, const char * path){
    //Serial.printf("Creating Dir: %s\n", path);
    int ret = fs.mkdir(path);
    if(!ret){
        /*Serial.println("Dir created");
    } else {*/
        Serial.println(" -< mkdir failed");
    }
    return ret;
}
//----------------------------------------------------------------------------------------------------------------------------
int removeDir(fs::FS &fs, const char * path){
    //Serial.printf("Removing Dir: %s\n", path);
    int ret = fs.rmdir(path);
    if(!ret){
        /*Serial.println("Dir removed");
    } else {*/
        Serial.println(" -< rmdir failed !!!");
    }
    return ret;
}
//----------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR readFile(fs::FS &fs, SemaphoreHandle_t mutex, String path){
  #ifdef MAUME_DEBUG  // #endif
    Serial.println(" * Reading file: "+path+".\n");
  #endif
  while(!xSemaphoreTakeRecursive(mutex, portMAX_DELAY));
    File file = fs.open(path.c_str(), "r");
    if(!file){
      xSemaphoreGiveRecursive(mutex);
      Serial.println(" -< Failed to open file for reading !!!");
      return " NO DATA ";
    }
    String str = "";
    while(file.available()){
        str += char(file.read());
    }
    file.close();
  xSemaphoreGiveRecursive(mutex);
  #ifdef MAUME_DEBUG  // #endif
    Serial.println(" * File content : "+str+".\n");
  #endif
  return str;
}
//----------------------------------------------------------------------------------------------------------------
/*int writeFile(fs::FS &fs, const char * path, const char * message){
    //Serial.printf("Writing file: %s\n", path);
    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return 0;
    }
    int ret = file.print(message);
    if(!ret){
        
        Serial.println("Write failed");
    }
    file.close();
    return ret;
}*/
//-----------------------------------------------------------------------------------------------------
int IRAM_ATTR writeBytesToFile(fs::FS &fs, SemaphoreHandle_t mutex, char * path, byte * bytes, int len){
  #ifdef MAUME_DEBUG // #endif
    Serial.printf(" -> Writing bytes to file : %s\n", path);
    Serial.printf(" -> Writing bytes         : %s\n",(char*)bytes);
  #endif
  int ret = 0;
  while(!xSemaphoreTakeRecursive(mutex, portMAX_DELAY));
    File file = fs.open(path, FILE_WRITE);
    if(!file){
      Serial.println(" -< Failed to open file for writing");
    }else{
      ret = (int)file.write((const uint8_t *)bytes, (size_t)len);
      if(!ret){
        Serial.println(" -< Write failed.");
      }
      file.close();
    }
  xSemaphoreGiveRecursive(mutex);
  return ret;
}
//-----------------------------------------------------------------------------------------------------
int IRAM_ATTR appendFile(fs::FS &fs, SemaphoreHandle_t mutex, String path, String message){
  //
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" -> Appending bytes to file : "+path+".");
    Serial.println(" -> Appending string        : "+message+".");
  #endif
  int ret = 0;
  while(!xSemaphoreTakeRecursive(mutex, portMAX_DELAY));
    if(fs.exists(path.c_str())){
      File file = fs.open(path.c_str(), FILE_APPEND);
      if(!file){
        xSemaphoreGiveRecursive(mutex);
        Serial.println(" -< Failed to open file for appending !!!");
        return ret;
      }
      ret = (int)file.print(message);    
      file.close();
    }else{
      File file = fs.open(path.c_str(), FILE_WRITE);
      if(!file){
        xSemaphoreGiveRecursive(mutex);
        Serial.println(" -< Failed to open file for append/writing !!!");
        return ret;
      }
      ret = (int)file.print(message);       
      file.close();
    }    
  xSemaphoreGiveRecursive(mutex);
  if(ret){
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" -> Message appended");
    #endif
  } else {
      Serial.println(" -< Append failed");
  }  
  return ret;
}
//----------------------------------------------------------------------------------------------------------------------------
int renameFile(fs::FS &fs, const char * path1, const char * path2){
    //Serial.printf("Renaming file %s to %s\n", path1, path2);
    int ret = fs.rename(path1, path2);
    if (!ret) {
        /*Serial.println("File renamed");
    } else {*/
        Serial.println(" -< Rename failed");
    }
    return ret;
}

//----------------------------------------------------------------------------------------------------------------------------
