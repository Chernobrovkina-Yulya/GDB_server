#pragma once

// библиотеки для работы с сокетами
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>

#include <string>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <cstdint>
// библиотека для многопоточности
#include <pthread.h>

#include "ktest.h"

#define MAX_BUF_SIZE 0x8000
#define NUM_OF_CP0_REGS 35
#define NUM_OF_CP1_REGS 32

// это функции, экспортируемые из библиотеки эмулятора
// они предоставляют API для работы с эмулятором
extern "C" uint32_t uemu_init(void);
extern "C" uint32_t uemu_dsp (uint32_t num, ...);

// результаты методов, запускающих исполнение
enum class EmulatorState
{
    INTERRUPTED = 0,
    RUNNING = 1,
    FINISHED = 2,
    FAILED = 3, 
};

// структура, представляющая RSP-пакет
struct Packet
{
    char data[MAX_BUF_SIZE];
    size_t len;
    uint8_t CheckSum();
};

class GDBServer
{
public:
    // методы, устанавливающие и закрывающие соединение между сервером и GDB
    void StartServer(int port, std::string &elf_name, load_elf_str &ed);
    void StopServer();

    // методы для обработки запросов GDB
    void ThreadsHandler();
    void HandleRequest();
    void RequestLoop();

private:
    // методы обрывания соединения с GDB
    void KillProcess();
    void Detach();

    void QueryPacket();         
    void vPacket();             
    void vContQuery();          
    void vCont();               
    void qSupported();         
    void qOffsets();            
    void qSymbol();             
    void vMustReplyEmpty();     
    void EmptyResp();          
    void qAttached();           

    // методы для работы с регистрами
    void ReadAllReg();
    void ReadReg();
    void WriteAllReg();
    void WriteReg();

    // методы для работы с памятью
    void ReadMem();
    void WriteMem();
    void WriteMemBin();

    // методы для запуска исполнения
    void StepInstr();
    void Continue();

    void UpdateEmState(int);

    // функции для работы с точками останова
    void InsertBp();
    void RemoveBp();

    // функции для работы с пакетами
    void PackStr(std::string &str);
    int TryGetChar(int, unsigned int);
    int GetChar();
    void PutChar(char ch);
    void PutPkt(std::string str);
    bool GetPkt();
    void SendStopReply(int sig);   

    // сокеты сервера и GDB
    int server_sock_fd;
    int client_sock_fd;
    pthread_mutex_t client_sock_m;

    Packet pack_in;                      // буфер для обмена пакетами с GDB
    Packet pack_out;
    bool is_connection_open = false; // при получении команды kill, detach становится false
    EmulatorState em_state; // состояние эмулятора

    reg_str r;              // структура, хранящая регистры эмулятора
    data_block d;           // структура, хранящая область памяти эмулятора
    exec_str ex;            // структура для информации о исполнении команд
    bp_str bp;              // структура для параметров точки останова
    // определения этих структур лежат в файле ktest.h, необходимы для работы с API эмулятора
};

// вспомогательные функции
char HexToChar(uint8_t d);
uint8_t CharToHex(int ch);
std::string ValToHex(uint64_t val, size_t numbytes);
uint64_t HexToVal(const char * s, size_t numbytes);
std::string RegNumToStr(int num);
int Unescape(char *data, size_t len);
void print_elf_data(load_elf_str const &ed);