#include "server.hpp"

#define BP_ENCOUNTERED 110

// функция для потока, выполняющего обработку запросов
void GDBServer::RequestLoop()
{
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0x00);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0x00);

    while(is_connection_open)
    {
        HandleRequest();
    }
}

using thread_fun = void *(*)(void * );

// обрабатываем запросы GDB, пока не получим сигнал прекращения соединения
void GDBServer::ThreadsHandler()
{
    // создаем поток, в котором обрабатываются запросы
    // главный поток в это время проверяет сокет на наличие ctrl-C
    pthread_t tid;
    pthread_create(&tid, nullptr, (thread_fun)&GDBServer::RequestLoop, static_cast<void *>(this));

    while(is_connection_open)
    {
        // когда выполнятся обработка команда GDB, сервер проверяет, ввел ли пользователь ctrl-C
        pthread_mutex_lock(&client_sock_m);
        if(TryGetChar(1, 100) == 0x03)
        {
            std::cout << "пользователь ввел команду ctrl-C\n";
            pthread_mutex_unlock(&client_sock_m);
            SendStopReply(2);

            // нужно уничтожить поток-обработчик запросов
            if(pthread_kill(tid, 0) != ESRCH)
                pthread_cancel(tid);

            // запускаем обработку команд GDB заново
            pthread_join(tid, nullptr);
            pthread_create(&tid, nullptr, (thread_fun)&GDBServer::RequestLoop, static_cast<void *>(this));
            continue;
        }
        pthread_mutex_unlock(&client_sock_m);
    }
    pthread_join(tid, nullptr);
}

// обработчик единичного запроса
void GDBServer::HandleRequest()
{
    GetPkt();

    pthread_mutex_unlock(&client_sock_m);

    std::cout << "\nкоманда от GDB:\t" << pack_in.data << '\n';

    switch (pack_in.data[0])
    {
    case 'H':
        // запрос на установку потока для работы программы
        //  на данный момент говорим GDB выбрать любой поток
        EmptyResp();
        break;

    case 'v':
        // получен v пакет
        vPacket();
        break;
         
    case 'q':
        // получен q (general query) пакет
        QueryPacket();
        break;

    case '!':
        PutPkt("OK");
        break;

    case '?':
        // запрос на причину остановки эмулятора. Присылается при установке соединения с GDB
        SendStopReply(BP_ENCOUNTERED);
        break;
    case 'D':
        // пользователь ввел команду detach
        Detach();
        break;
    case 'k':
        // сервер получил kill request
        KillProcess();
        break;
        // продолжение исполнения программы. Команды С и с обрабатываются одинаково
    case 'c':
        Continue();
        break;
    case 'C':
        Continue();
        break;
        // выполнить шаг исполнения программы. Команды S и s обрабатываются одинаково
    case 's':
        StepInstr();
        break;
    case 'S':
        StepInstr();
        break;
    case 'g':
        // запрос на чтение всех регистров
        ReadAllReg();
        break;
    case 'G':
        // запрос на запись всех реистров
        WriteAllReg();
        break;
    case 'p':
        // запрос на чтение одиночого регистра
        ReadReg();
        break;
    case 'P':
        // запрос на запись одиночого регистра
        WriteReg();
        break;
    case 'm':
        // запрос на чтение памяти
        ReadMem();
        break;
    case 'M':
        // запрос на запись в память
        WriteMem();
        break;
    case 'X':
        // запрос на запись в память двоичных данных
        WriteMemBin();
        break;
    case 'Z':
        // запрос на запись точки останова
        InsertBp();
        break;
    case 'z':
        // запрос на удаление точки останова
        RemoveBp();
        break;
    default:
        // неизвестные запросы игнорируются
        std::cout << "Warning: неподдерживаемый запрос: " << pack_in.data << '\n';
        EmptyResp();
        break;
    }
}

#pragma region ExitCommands
// обработка команды 'k'
void GDBServer::KillProcess()
{
    is_connection_open = false;
    std::cout << "получена команда kill\n";
}

// обработка команды 'D'
void GDBServer::Detach()
{
    is_connection_open = false;
    PutPkt("OK");
    std::cout << "соединение прервано\n";
}
#pragma endregion ExitCommands

#pragma region vPackets
// обработка 'v' пакетов
void GDBServer::vPacket()
{
    if(strstr(pack_in.data, "vMustReplyEmpty"))
        vMustReplyEmpty();
    else if(strstr(pack_in.data, "vCont?"))
        vContQuery();
    else if (strstr(pack_in.data, "vCont;"))
        vCont();
    else
    {
        std::cout << "получен неизвестный 'v' запрос\n";
        EmptyResp();
    }
}

// обработка команды 'vCont; action'
// action может быть c, C, s, S, t, r
void GDBServer::vCont()
{
    char action = 0;
    sscanf(pack_in.data, "vCont;%c", &action);
    switch (action)
    {
    case 'c':
        Continue();
        break;
    case 's':
        StepInstr();
        break;
    default:
        PutPkt("E22");  // остальные виды action не поддерживаются
        break;
    }
}

// говорит GDB, что ответ на неизвестные запросы - пустая строка
void GDBServer::vMustReplyEmpty()
{
    EmptyResp();
}

// обработчик команды 'vCont?'
void GDBServer::vContQuery()
{
    if(em_state == EmulatorState::INTERRUPTED)
        PutPkt("vCont;c;C;s;S"); // говорим, что доступен шаг или продолжение

    if(em_state == EmulatorState::FINISHED || em_state == EmulatorState::RUNNING 
    || em_state == EmulatorState::FAILED)
        PutPkt("vCont;");                       // говорим, что ничего не доступно

}
#pragma endregion vPackets

#pragma region GeneralQueryPackets
// обработчик q пакетов
void GDBServer::QueryPacket()
{
    if (strstr(pack_in.data, "qSupported"))
        qSupported();
    else if (strstr(pack_in.data, "qAttached"))
        qAttached();
    else if (strstr(pack_in.data, "qOffsets"))
        qOffsets();
    else if (strstr(pack_in.data, "qSymbol::"))
        qSymbol();
    else
    {
        std::cout << "получен неизвестный q пакет\n";
        EmptyResp();
    }
}

// этой командой GDB говорит о поддерживаемых возможностях, в ответ ожидает список поддерживаемых сервером
void GDBServer::qSupported()
{
        PutPkt("PacketSize=4096;");                 // пока сообщаем только максимальный размер пакета
}

void GDBServer::qAttached()
{
    PutPkt("0");
}

// поскольку эмулятор (пока) не работает с относительными адресами, возвращаем 0
void GDBServer::qOffsets()
{
    PutPkt("Text=0;Data=0;Bss=0");
}

// сообщаем GDB, что не нуждаемся в поиске символов
void GDBServer::qSymbol()
{
    PutPkt("OK");
}
#pragma endregion GeneralQueryPackets

#pragma region registers
// возвращает GDB строку со значениями всех регистров
void GDBServer::ReadAllReg()
{
    uemu_dsp(8, &r);
    std::string s;
    for (int i = 0; i < NUM_OF_CP0_REGS; ++i)
    {   
        s += ValToHex(r.cpu[i].v, 8);
    }
    PutPkt(s);
    std::cout << "состояние регистров:\t" << s << '\n';
}

// возвращает GDB значение одного регистра
void GDBServer::ReadReg()
{
    unsigned reg_num = 0;
    sscanf(pack_in.data, "p%x", &reg_num);

    uemu_dsp(8, &r);
    std::string s;

    if (reg_num < NUM_OF_CP0_REGS)
        s = ValToHex(r.cpu[reg_num].v, 8);

    if(reg_num >= NUM_OF_CP0_REGS)
        s = ValToHex(r.fpu[reg_num - NUM_OF_CP0_REGS].v.i, 8);

    PutPkt(s);
    if(reg_num >= NUM_OF_CP0_REGS + NUM_OF_CP1_REGS)
        std::cout << "GDB запросил некорректный регистр, номер регистра: " << reg_num << '\n';
    else 
        std::cout << "запрошен регистр: " << RegNumToStr(reg_num) << " = " << s << '\n';
}

// запысывает преданные значения в регистры
void GDBServer::WriteAllReg()
{
    std::string reg_name;
    for (int i = 0; i < NUM_OF_CP0_REGS; ++i)
    {
        reg_name = RegNumToStr(i);
        uemu_dsp(10, reg_name.c_str(), HexToVal(&(pack_in.data[16 * i + 1]), 8));
    }

    PutPkt("OK");
}

// записывает переданное значение в указанный регистр
void GDBServer::WriteReg()
{
    unsigned reg_num = 0;    // номер регистра
    char reg_val[16];   // значение, которое нужно записать в регистр

    sscanf(pack_in.data, "P%x=%s", &reg_num, reg_val);

    std::string reg_name = RegNumToStr(reg_num);
    uemu_dsp(10, reg_name.c_str(), HexToVal(reg_val, 8));

    PutPkt("OK");
    std::cout << "записан регистр :" << reg_name << " = " << reg_val << '\n';
}
#pragma endregion registers

#pragma region memory
// передаем GDB блок памяти. Обрабатывает команду 'm addr, len'
void GDBServer::ReadMem()
{
    uint64_t addr;  // откуда читать информацию
    uint32_t len;        // сколько байтов прочесть
    sscanf(pack_in.data, "m%lx,%x:", &addr, &len);

    int err = 0;
    d.l = 4096;

    err = uemu_dsp(11, &d, addr, (uint64_t)(addr + len - 1));

    // если возникла ошибка, передаем GDB пакет с ошибкой в соответствии с документацией
    if(err != 0)
    {   
        std::string err_mess = "E" + std::to_string(err);
        PutPkt(err_mess);
        std::cout << "ошибка чтения памяти. Код ошибки: " << err << '\n';
        return;
    }

    // читаем блок памяти
    std::string s;
    for (size_t i = 0; i < len; ++i)
    {
        uint8_t ch = d.d[i];
        s += HexToChar(ch >> 4);
        s += HexToChar(ch & 0xf);
    }

    PutPkt(s);
    std::cout << "прочитан блок памяти по адресу: " << addr << "\tдлина блока: " << len << "\tзначение: " << s << '\n';
}
// обработка команды 'M addr, len'
void GDBServer::WriteMem()
{
    uint64_t addr;  // откуда читать информацию
    uint32_t len;        // сколько байтов прочесть
    sscanf(pack_in.data, "M%lx,%x:", &addr, &len);

    char *val_p = (char *)(memchr(pack_in.data, ':', pack_in.len )) + 1;

    int err = 0;
    for (size_t i = 0; i < len; ++i)
    {
        uint8_t b1 = CharToHex(val_p[i * 2]);
        uint8_t b2 = CharToHex(val_p[i * 2 + 1]);
        uint8_t val = ((b1 << 4) | b2);
        d.d[i] = val;
    }
    d.l = len;
    err = uemu_dsp(13, &d, addr);
    if(err != 0)
        std::cout << "error: " << err << '\n';

    PutPkt("OK");
    std::cout << "записан блок памяти по адресу: " << addr << "\tразмер блока: " << len << '\n';
}

//  используется для обработки команды 'X addr, len:XX...' - запись двоичных данных в память
void GDBServer::WriteMemBin()
{
    uint64_t addr;  // откуда читать информацию
    uint32_t len;        // сколько байтов прочесть
    sscanf(pack_in.data, "X%lx,%x:", &addr, &len);

    char *bin_data = (char *)(memchr(pack_in.data, ':', pack_in.len )) + 1;
    size_t off = bin_data - pack_in.data;
    Unescape(bin_data, pack_in.len - off);

    int err = 0;
    // записываем новые значения в блок памяти
    for (size_t i = 0; i < len; ++i)
    {
        d.d[i] = bin_data[i];
    }

    d.l = len;
    err = uemu_dsp(13, &d, addr);   // передаем блок памяти эмулятору на запись

    if(err != 0)
        std::cout << "ошибка записи в память, код ошибки: " << err << '\n';

    PutPkt("OK");
    std::cout << "записаны двоичные данные по адресу: " << addr 
    << "\tдлина блока: " << len << "\tданные: " << bin_data << '\n';
}
#pragma endregion memory

#pragma region ExecutionCommands
void GDBServer::Continue()
{
    int err = uemu_dsp(15, &ex, 0);       // говорим эмулятору запустить процесс исполнения

    SendStopReply(err);
    UpdateEmState(err);
    std::cout << "выполнена команда continue, ответ эмулятора: " << err << '\n';
}

void GDBServer::StepInstr()
{
    int err = uemu_dsp(15, &ex, 1);

    SendStopReply(err);
    UpdateEmState(err);
    std::cout << "выполнен шаг, ответ эмулятора: " << err << '\n';
}
#pragma endregion ExecutionCommands

#pragma region breakpoints
// обрабатывает запрос 'Z type, addr, kind'
// эмулятор поддерживает только software breakpoint (type = 0)
void GDBServer::InsertBp()
{
    uint64_t addr;          // адрес точки останова
    int type;               // тип точки останова

    sscanf(pack_in.data, "Z%d,%lx,", &type, &addr);

    if (type != 0) // если не software breakpoint, передаем пустой пакет
    {
        EmptyResp();
        std::cout << "неподдерживаемый тип точки останова\n";
        return;
    }

    bp.a = addr;
    int err = uemu_dsp(16, &bp, 1);     // передаем запрос на установку точки останова эмулятору

    if(err != 0 && err != 22)
    {
        std::cout << "ошибка установки точки останова. Код ошибки: " << err << '\n';
        std::string err_mess = "E" + std::to_string(err);
        PutPkt(err_mess);
    }
    else
        PutPkt("OK");

    std::cout << "установлена точка останова по адресу: " << addr << '\n';
}

// обрабытавает запрос 'z type, addr, kind'
void GDBServer::RemoveBp()
{
    uint64_t addr;          // адрес точки останова
    int type;               // тип точки останова

    sscanf(pack_in.data, "z%d,%lx,", &type, &addr);

    if (type != 0) // если не software breakpoint, передаем пустой пакет
    {
        EmptyResp();
        std::cout << "неподдерживаемый тип точки останова\n";
        return;
    }

    bp.a = addr;
	int err = uemu_dsp(16, &bp, 0);

    if(err != 0 && err != 26)
    {
        std::cout << "ошибка удаления точки останова. Код ошибки: " << err << '\n';
        std::string err_mess = "E" + std::to_string(err);
        PutPkt(err_mess);
    }
    else
        PutPkt("OK");
    
    std::cout << "удалена точка останова по адресу: " << addr << '\n';
}
#pragma endregion breakpoints

void GDBServer::UpdateEmState(int err)
{
    switch (err)
    {   
    case 0: // эмулятор сообщил об успешном исполнении
        em_state = EmulatorState::INTERRUPTED;
        break;
    case 107: // эмулятор вернул код завершения исполнения программы
        em_state = EmulatorState::FINISHED;
        break;
    case 110:                   // эмулятор достиг точки останова
        em_state = EmulatorState::INTERRUPTED;
        break;
    default:                    // эмулятор вернул код ошибки
        em_state = EmulatorState::FAILED;
        break;
    }
}

// передает GDB пустую строку
void GDBServer::EmptyResp()
{
    PutPkt("");
}