#include "server.hpp"

// метод пытается получить символ из сокета, по истечении тайм-аута завершается
int GDBServer::TryGetChar(int filedes, unsigned int useconds)
{
    fd_set set;
    int ch = 0;
    struct timeval timeout;
    FD_ZERO(&set);
    FD_SET(client_sock_fd, &set);

    timeout.tv_sec = 0;
    timeout.tv_usec = useconds;
    int res = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
    if(res > 0)
    {
        read(client_sock_fd, &ch, sizeof(char));
    }
    return ch;
}

// вычисляет контрольную сумму для полученного пакета
uint8_t Packet::CheckSum()
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; ++i)
        checksum += static_cast<uint8_t>(data[i]);

    return checksum;
}

// записывает переданную строку в пакет
void GDBServer::PackStr(std::string &str)
{
    strcpy(pack_out.data, str.c_str());
    pack_out.data[str.size()] = '\0';
    pack_out.len = str.size();
}

// получает единичный символ от GDB
// возвращает полученнный символ или -1 в случае неудачи
int GDBServer::GetChar()
{
    char ch;
    int numread = read(client_sock_fd, &ch, sizeof(char));
    if(numread == -1)
        return -1;
    return ch & 0xff;
}

// передает единичный символ GDB
void GDBServer::PutChar(char ch)
{
    // в случае неудачи попытка отправить символ возобновится
    while(1)
    {
        int numwritten = write(client_sock_fd, &ch, sizeof(char));
        if(numwritten == 0 || numwritten == -1)
            continue;
        else
            return;
    }
}

// получает следующий пакет от GDB
// сохраняет полученный пакет в поле pack
// возвращает true в случае успеха и false иначе
bool GDBServer::GetPkt()
{
    while (1)
    {
        char buf[MAX_BUF_SIZE];

        {
            if(1 > read(client_sock_fd, buf, MAX_BUF_SIZE))
                return false;
        }

        int count = 0;
        char ch = buf[0];

        int i = 1;
        while(ch != '$')        // ожидает символа начала пакета '$'
        {
            ch = buf[i];
            ++i;
        }

        while(count < MAX_BUF_SIZE - 1)
        {
            ch = buf[i];
            ++i;

            if(ch == '$')
            {
                count = 0;
                continue;
            }
            if(ch == '#')       // достигли конца пакета
                break;

            pack_in.data[count] = ch;
            ++count;
        }
        pack_in.data[count] = '\0';
        pack_in.len = count;

        if(ch == '#')
        {
            // проверяем контрольную сумму
            uint8_t checksum = pack_in.CheckSum();
            uint8_t pkt_checksum;
            ch = buf[i];
            ++i;
            pkt_checksum = CharToHex(ch) << 4;

            ch = buf[i];
            pkt_checksum += CharToHex(ch);

            if(checksum != pkt_checksum)
            { 
                std::cout <<"Неверная контрольная сумма в пакете\n";
                PutChar('-'); // возвращаем отрицательное подтверждение
            }
            else
            {
                PutChar('+');                           // возвращаем положительное подтверждение
                return true;
            }
        }
        return true;
    }
    return true;
}

// отправляет пакет GDB
void GDBServer::PutPkt(std::string str)
{
    pthread_mutex_lock(&client_sock_m);
    PackStr(str);
    uint8_t checksum = pack_out.CheckSum();
    int len = pack_out.len;
    int ch;
    do
    {
        PutChar('$');                                   // начало пакета

        pack_out.data[len] = '#';                           // конец пакета
        pack_out.data[len + 1] = HexToChar(checksum >> 4);  // указываем контрольную сумму
        pack_out.data[len + 2] = HexToChar(checksum % 16);
        {
        if (-1 == write(client_sock_fd, pack_out.data, len + 3))
        {
            perror("write error");
            exit(EXIT_FAILURE);
        }
        }
        ch = GetChar();
    } while (ch != '+');
    pack_out.len = 0;
}

// функция посылает GDB stop reply пакет, т. е. результат выполнения программы
void GDBServer::SendStopReply(int sig)
{
    const char* mess;
    switch(sig)
    {
        case 0: // эмулятор сообщил об успешном исполнении, отправляем SIGTRAP
            mess = "S05";
            break;
        case 2: // пользователь ввел ctrl-C, отправляем SIGINT
            mess = "S02";
            break;
        case 110: // достигнута точка останова, отправяем SIGTRAP
            mess = "S05";
            break;
        case 107:   // завершение работы программы, отправляем SIGSTOP
            mess = "W19";
            break;
        default:    // эмулятор сообщил об ошибке в исполнении
            mess = "X";
            break;
    }

    PutPkt(std::string(mess));
}
