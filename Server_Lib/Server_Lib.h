#ifdef SERVER_LIB_EXPORTS
#define SERVER_LIB_API __declspec(dllexport)
#else
#define SERVER_LIB_API __declspec(dllimport)
#endif

//DCS Error Codes
#define NO_DCS_ERROR 0
#define FRAME_CHECKSUM_ERROR -1
#define FRAME_VERSION_ERROR -2
#define FRAME_INVALID_DATA -3
#define FRAME_DATA_CORRUPTION -4
#define MEMORY_ALLOCATION_ERROR -5
#define NETWORK_NOT_READY -6

//Thread Error Codes
#define THREAD_START_ERROR -7
#define THREAD_ALREADY_EXISTS -8
#define NETWORK_INIT_ERROR -9
#define NETWORK_ERROR -10

SERVER_LIB_API int Start_Server(const char* port);

SERVER_LIB_API int Stop_Server(void);