#ifndef _HANDLE_FILE_H_
#define _HANDLE_FILE_H_

#include <vector>
#include <pthread.h>
#include <sys/types.h>
#include <string>
#include <cstring>

namespace BasicTest
{
#define SHARE_MEMORY_NAME   "shareMemory"
#define MMAP_FILE_PATH      "mmapFile"
#define PROJECT_ID          8
#define NUM 128

	typedef struct
	{
		pid_t					mainPid;
		int						shmId;
		char					word[NUM];
		int						totalNum;
	}ShmInfo;

	class FileHanle
	{
	public:
		/*
		* @brief
		*   Obtain the FileHanle object.
		* @return
		*   FileHanle*: FileHanle object.
		* @param
		*   None
		*/
		static FileHanle* getInstance();

		/*
		* @brief
		*   Initialize the lock of process.
		* @return
		*   bool: Whether the process lock was created successfully. true: Success, false: fail.
		* @param
		*   None
		*/
		bool initMutexPtr();

		/*
		* @brief
		*   Obtain shared memory information.
		* @return
		*   int: Whether the shared memory is obtained successfully. 1: Success, -1: fail.
		* @param
		*   [in]ShmInfo** ptr: Mapped segment memory pointer.
		*   [in]int* shmId: Segment memory identifier.
		*/
		int obtainSharedMemory(ShmInfo** ptr, int* shmId);

		/*
		* @brief
		*   Initialize the shared memory.
		* @return
		*   bool: Whether the shared memory is initialized successful, true: Success, false: failed
		* @param
		*   None
		*/
		bool sharedMemoryInit();

		/**
		* @brief
		*   Change the variable from int to string.
		* @return
		*   string.
		* @param
		*   int num
		*/
		std::string intToString(int num);

		/**
		* @brief
		*   Calculate the size of specified file.
		* @return
		*   int: The size of current file.
		* @param
		*   const char* fileName: Full path name of the file.
		*/
		int caculateFileSize(const char* fileName);

		/*
		* @brief
		*   Count the total number of the specified word appears in the input file.
		* @return
		*   void
		* @param
		*   [in]const char* inFile: Input file
		*   [in]int processNum: Current process index
		*   [in]int blockSize: The minimum size of block which current process should read from the file.
		*   [in]int isEnd: Whether the process is the last one.
		*   [in]char* word: Specified word which is input by user.
		*/
		void readFileFindWord(const char* inFile, int processNum, uint32_t blockSize, int isEnd, char* word);

		/**
		* @brief
		*   Output the word which is input by user and the number of word in the file.
		* @return
		*   void.
		* @param
		*   None
		*/
		void outputResult();

		/**
		* @brief
		*   Destroy the mutex and shared memory resource.
		* @return
		*   void.
		* @param
		*   None
		*/
		void destroyResource();

	private:
		FileHanle();
		~FileHanle();

		FileHanle(const FileHanle& info);
		FileHanle(const FileHanle&& info);
		FileHanle& operator=(const FileHanle&);

	private:
		ShmInfo* shmInfoPtr;
		pthread_mutex_t* mutexPtr;
	};
}

#endif // !_HANDLE_FILE_H_
