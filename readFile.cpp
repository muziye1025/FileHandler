#include "readFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream> 
#include <unistd.h>
#include <sys/sem.h>
#include <sys/ipc.h> 
#include <sstream>
#include <sys/shm.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/wait.h>

using namespace std;

#define	DEFAULT_PROCESS_NUMBER	8
namespace BasicTest
{
	FileHanle* FileHanle::getInstance()
	{
		static FileHanle m_instance;
		return &m_instance;
	}

	FileHanle::FileHanle()
	{
		shmInfoPtr = nullptr;
		mutexPtr = nullptr;
	}

	FileHanle::~FileHanle()
	{
	}

	bool FileHanle::initMutexPtr()
	{
		bool ret = false;
		int fd = open(MMAP_FILE_PATH, O_RDWR | O_CREAT, 0744);
		if (0 < fd)
		{
			// Reset file's size with pthread_mutex_t
			ftruncate(fd, sizeof(pthread_mutex_t));
			mutexPtr = (pthread_mutex_t*)mmap(0, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			close(fd);
		}
		else
		{
			cout << __FUNCTION__ << ": " << __LINE__ << " Error" << ": open err" << endl;
		}
		if (NULL != mutexPtr)
		{
			// Initialize the mutex object.
			pthread_mutexattr_t attr;
			pthread_mutexattr_init(&attr);
			pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
			pthread_mutex_init(mutexPtr, &attr);
			cout << __FUNCTION__ << ": " << __LINE__ << ": Init mutex successed!" << endl;
			ret = true;
		}
		return ret;
	}

	int FileHanle::obtainSharedMemory(ShmInfo** ptr, int* shmId)
	{
		key_t key = ftok(SHARE_MEMORY_NAME, PROJECT_ID);
		if (key < 0)
		{
			cout << __FUNCTION__ << ": " << __LINE__ << " Error" << ": ftok failed!" << endl;
			return -1;
		}

		/*
			Obtain the shared memory according to the Key value, create and obtain if it does not exist,
			and obtain it if it exists. The return value is the shared memory segment identifier
		*/
		if ((*shmId = shmget(key, sizeof(ShmInfo), 0666 | IPC_CREAT | IPC_EXCL)) < 0)
		{
			// Check if the file exists
			if (EEXIST == errno)
			{
				/*
					Obtain the shared memory segment identifier according to the Key value,
					create and connect if it does not exist, and connect if it exists.
					The return value is the shared memory segment identifier
				*/
				*shmId = shmget(key, sizeof(ShmInfo), 0666);
				/*
					According to the shared memory segment identifier, the shared memory
					is mapped to the process space, and the mapped segment address is returned.
				*/
				*ptr = (ShmInfo*)shmat(*shmId, nullptr, 0);
				cout << __FUNCTION__ << ": " << __LINE__ << ": Get share memory succeed!" << endl;
			}
			else
			{
				cout << " Error" << ": get share memory failed!" << endl;
				return -1;
			}
		}
		else
		{
			/*
				According to the shared memory segment identifier, the shared memory
				is mapped to the process space, and the mapped segment address is returned.
			*/
			*ptr = (ShmInfo*)shmat(*shmId, nullptr, 0);
			cout << __FUNCTION__ << ": " << __LINE__ << ": Get share memory succeed!" << endl;
		}
		cout << __FUNCTION__ << ": " << __LINE__ << ": key = " << key << ", shmId = " << *shmId << ", addr = " << *(unsigned long*)ptr << endl;
		return 1;
	}

	bool FileHanle::sharedMemoryInit()
	{
		ShmInfo* shmPtr = nullptr;
		int ret = 0;
		int shmId = 0;
		bool result = false;
		int fd = open(SHARE_MEMORY_NAME, O_RDWR | O_CREAT, 0744);
		if (0 < fd)
		{
			close(fd);
		}
		else
		{
			cout << __FUNCTION__ << ": " << __LINE__ << " Error: " << SHARE_MEMORY_NAME << " is not found!" << endl;
			return result;
		}

		ret = obtainSharedMemory(&shmPtr, &shmId);
		cout << __FUNCTION__ << ": " << __LINE__ << ": ret = " << ret << endl;
		if (ret > 0 && nullptr != shmPtr)
		{
			shmInfoPtr = shmPtr;
			shmInfoPtr->shmId = shmId;
			shmInfoPtr->mainPid = getpid();
			shmInfoPtr->totalNum = 0;
			memset(shmInfoPtr->word, 0x00, sizeof(shmInfoPtr->word));
			result = true;
		}
		return result;
	}

	string FileHanle::intToString(int num)
	{
		ostringstream oss;
		string strNum = "";
		oss << num;
		strNum = oss.str();
		return strNum;
	}

	int FileHanle::caculateFileSize(const char* fileName)
	{
		if (nullptr == fileName)
		{
			cout << __FUNCTION__ << ": " << __LINE__ << " Error" << ": no file is found in current folder!" << endl;
			return 0;
		}
		int fileSize = 0;
		fstream file;
		file.open(fileName, ios::in);
		file.seekg(0, ios::end);
		fileSize = file.tellg();
		file.close();
		return fileSize;
	}

	void FileHanle::readFileFindWord(const char* inFile, int processNum, uint32_t blockSize, int isEnd, char* word)
	{
		/* fielOutlog is used to save the block of the file which is divided in current process. The Log_* are used to debug the program. */
		//fstream fielOutlog;
		//string outLog = "Log_" + intToString(processNum);
		//fielOutlog.open(outLog.c_str(), ios::out);

		/* fileIn is used to read the source file. */
		ifstream fileIn;
		fileIn.open(inFile, ios::in);
		int countRead = 0;				// Mark the number of the lines in current block of file.
		int wordHittedCount = 0;		// Mark the hit count of the word.
		int allWordCount = 0;			// Mark the total number of the current block, the variable is actually useless. ^_^
		char tempWord;					// Stores the first character of current block.
		bool neglectFirstLine = false;	// Mark Whether the first line should be read, it may be read in the previous block.
		/* Locate the current pointer position with the process's index and average block size. */
		fileIn.seekg(processNum * blockSize, ios::cur);
		fileIn.get(tempWord);
		/* If the first character of current block is newline sign, the first line should be handled. */
		if (0 != processNum && 0 != strcmp(&tempWord, "\n"))
		{
			neglectFirstLine = true;
		}
		cout << __FUNCTION__ << ": " << __LINE__ << ": processNum = " << processNum << ": blockSize = " << blockSize << ": neglectFirstLine = " << neglectFirstLine << endl;
		cout << __FUNCTION__ << ": " << __LINE__ << ": processNum * blockSize = " << processNum * blockSize << endl;

		cout << __FUNCTION__ << ": " << __LINE__ << ": word = " << word << endl;
		while (!fileIn.eof())
		{
			string strTemp = "";
			getline(fileIn, strTemp);
			if (!neglectFirstLine)
			{
				// fielOutlog is used to debug the program.
				//fielOutlog << strTemp.c_str() << endl;
				const char delimiter[] = " \t\n";
				size_t length = strlen(strTemp.c_str());
				char* strBackup = (char*)strTemp.c_str();
				char* token = nullptr;
				char* buffer;
				/* 
					Here is a patch, I cann't split the string by "\n", the ASCII value of end of the strBackup is 13, it means the character is "\n",
					then I change the 13 to 32 or 0(32 means the character is space, 0 means "\0").
				*/
				if (strBackup[strlen(strBackup) - 1] == 13)
				{
					strBackup[strlen(strBackup) - 1] = 0;
				}

				/* Split the string according to the given delimiter, the "\n" is changed to " ", so only use the delimiter as "\t ". */
				while (nullptr != (token = strtok_r(strBackup, "\t ", &buffer)))
				{
					strBackup = nullptr;
					if (0 == strcmp(token, word))
					{
						++wordHittedCount;
					}
					++allWordCount;
				}
			}
			else
			{
				neglectFirstLine = false;
			}
			++countRead;
			uint32_t fileSize = fileIn.tellg();
			if (0 < processNum)
			{
				fileSize -= processNum * blockSize;
			}
			/* The last process should handle all of the remaining content. */
			if (0 == isEnd && fileSize >= blockSize)
			{
				cout << __FUNCTION__ << ": " << __LINE__ << ": Current block has been scanned over!" << endl;
				break;
			}
		}
		fileIn.close();
		//fielOutlog.close();

		int ret = 0;
		int shmId = 0;
		ShmInfo* shmPtr = nullptr;
		/* Write the result to shared memory, process lock is required, otherwise the result will be wrong. */
		int lockRet = pthread_mutex_lock(mutexPtr);
		cout << __FUNCTION__ << ": " << __LINE__ << ": lockRet = " << lockRet << endl;
		if (0 == lockRet)
		{
			cout << __FUNCTION__ << ": " << __LINE__ << ": mutex lock successed!" << endl;
			ret = obtainSharedMemory(&shmPtr, &shmId);
			cout << __FUNCTION__ << ": " << __LINE__ << ": ret = " << ret << endl;
			if (ret > 0 && nullptr != shmPtr)
			{
				shmInfoPtr = shmPtr;
				shmInfoPtr->shmId = shmId;
				shmInfoPtr->totalNum += wordHittedCount;
				strncpy(shmInfoPtr->word, word, min(strlen(word), sizeof(shmInfoPtr->word)));
				cout << __FUNCTION__ << ": " << __LINE__ << ": The process [" << processNum << "]'s "  << " wordHittedCount = " << wordHittedCount << endl;
			}
		}
		else
		{
			cout << __FUNCTION__ << ": " << __LINE__ << " Error" << ": mutex lock failed: " << lockRet << endl;
		}

		if (0 == lockRet && nullptr != mutexPtr)
		{
			pthread_mutex_unlock(mutexPtr);
			cout << __FUNCTION__ << ": " << __LINE__ << ": mutex unlock!" << endl;
		}
	}

	void FileHanle::outputResult()
	{
		cout << __FUNCTION__ << ": " << __LINE__ << ": The total number of the keyword " << "'" << shmInfoPtr->word << "'" << " is " << shmInfoPtr->totalNum << "." << endl;
	}

	void FileHanle::destroyResource()
	{
		remove(SHARE_MEMORY_NAME);
		remove(MMAP_FILE_PATH);
		// Unmap shared memory
		shmdt(&shmInfoPtr);
		// Destroy shared memory
		shmctl(shmInfoPtr->shmId, IPC_RMID, NULL);

		// Destroy mutexPtr
		pthread_mutex_destroy(mutexPtr);
		shmInfoPtr = nullptr;
		mutexPtr = nullptr;
	}
}

int main(int argc, char* argv[])
{
	BasicTest::FileHanle* ins = BasicTest::FileHanle::getInstance();
	pid_t pid = 0;
	/* Caculate the total size of the file, divide files by the number of processes. */
	if (nullptr == argv[1])
	{
		cout << __FUNCTION__ << ": " << __LINE__ << " Error" << ": File name should be input after the application! " << endl;
		return -1;
	}

	if (nullptr == argv[2])
	{
		cout << __FUNCTION__ << ": " << __LINE__ << " Error" << ": Please input the word which you want to find int the file, the word can be checked after the file's name!" << endl;
		return -1;
	}

	int fileSize = ins->caculateFileSize(argv[1]);
	if (0 == fileSize)
	{
		cout << __FUNCTION__ << ": " << __LINE__ << " Error" << ": the file's size is 0, please check the file!" << endl;
		return -1;
	}
	/* If user doesn't input the number of process, the default value is 8. */
	int maxTthread = DEFAULT_PROCESS_NUMBER;
	if (argc > 3 && atoi(argv[3]) > 0)
	{
		maxTthread = min(atoi(argv[3]), DEFAULT_PROCESS_NUMBER);
	}
	cout << __FUNCTION__ << ": " << __LINE__ << ": maxTthread = " << maxTthread << endl;
	/* Initialize the mutex lock. */
	bool ret = ins->initMutexPtr();
	if (!ret)
	{
		cout << __FUNCTION__ << ": " << __LINE__ << " Error" << ": the initialization of lock occurred error, the program will run in unsafe environment!" << endl;
	}
	/* Initialize the shared memory. */
	bool initRet = ins->sharedMemoryInit();
	if (!initRet)
	{
		cout << __FUNCTION__ << ": " << __LINE__ << " Error" << ": the initialization of shared memory occurred error, exit!" << endl;
		return -1;
	}
	cout << __FUNCTION__ << ": " << __LINE__ << ": pid = " << getpid() << endl;
	vector<pid_t> processId;

	for (int idx = 0; idx < maxTthread; ++idx)
	{
		/* Create multi processes by fork() function. */
		pid = fork();
		if (-1 == pid)
		{
			cout << __FUNCTION__ << ": " << __LINE__ << ": create process failed!" << endl;
		}
		cout << __FUNCTION__ << ": " << __LINE__ << ": pid = " << pid << ", idx = " << idx << endl;
		/*  
			Because the number of processes is determined by the user, only use the child process to handle the file. 
			Parent process do nothing but wait the child process to quit. 
		*/
		if (pid == 0)
		{
			int isEnd = 0;	// Mark for the last process.
			if (idx == maxTthread - 1)
			{
				isEnd = 1;
			}
			ins->readFileFindWord(argv[1], idx, fileSize / maxTthread, isEnd, argv[2]);
			cout << __FUNCTION__ << ": " << __LINE__ << ": fileSize = " << fileSize << endl;
			exit(0);
		}
		else
		{
			processId.push_back(pid);
			int status = 0;
			int retWait = waitpid(-1, &status, 0);
			cout << __FUNCTION__ << ": " << __LINE__ << ": pid = " << pid << " is parent process." << endl;
			cout << __FUNCTION__ << ": " << __LINE__ << ": " << "child process " << WTERMSIG(status) << " exit!" << endl;
		}
	}
	vector<pid_t>::iterator iter = processId.begin();
	while (iter != processId.end())
	{
		int status = 0;
		cout << __FUNCTION__ << ": " << __LINE__ << ": pid = " << *iter << " has exited." << endl;
		waitpid(*iter, &status, 0);
		iter = processId.erase(iter);
	}
	/* Output the number of the word which is input by user. */
	ins->outputResult();
	/* Release process locks and shared memory resources */
	ins->destroyResource();
	return 0;
}