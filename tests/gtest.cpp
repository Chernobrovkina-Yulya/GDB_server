#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/server.hpp"

#define PORT 8000

#pragma region Client
// фиктивный клиент, который отправляет серверу нужные запросы и получает ответ
class Client
{
    int client_sock;
    char data[MAX_BUF_SIZE];

public:
    void StartClient();
    ~Client();
    void SendMess(const std::string&);
    std::string GetResp();
};

void Client::StartClient()
{
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {0};

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    while(0 != connect(client_sock, (struct sockaddr*) & server_addr, sizeof(server_addr)));
}

Client::~Client()
{
    close(client_sock);
}

void Client::SendMess(const std::string &mess)
{
    while(-1 == send(client_sock, mess.c_str(), mess.size(), 0));
}

std::string Client::GetResp()
{
    int len = 0;
    int res =0;
    char *buf = data;
    while (-1 == res  || 0 == res)
    {
        res = read(client_sock, buf, MAX_BUF_SIZE);
        if (1 == res)
        {
            ++buf;
            ++len;
            res = 0;
        }
    }
    len += res;
    std::string str(data, len);
    memset(data, '\0', strlen(data));
    SendMess("+");
    return str;
}
#pragma endregion Client

// функция, которая передается в отдельный поток, исполняет работу сервера
void ServerWork(GDBServer &server)
{
    load_elf_str ed;
    // исполняемый файл для тестов, лежит в той же директории, что и тесты
    std::string elf_name("../tests/example_for_test");    
    server.StartServer(PORT, elf_name, ed);
    server.ThreadsHandler();
    server.StopServer();
}

using thread_fun = void *(*)(void * );

class ServerFixture: public ::testing::Test
{
protected:
    void SetUp()
    {
        pthread_create(&tid, nullptr, (thread_fun)ServerWork, static_cast<void *>(&server));
        client.StartClient();
    }
    void TearDown()
    {
        client.SendMess("$k#6b");
        pthread_join(tid, nullptr);
    }
    Client client;
    GDBServer server;
    pthread_t tid;
};

#pragma region UtilityTest
// тесты для проверки вспомогательных функций
TEST(UtilityTests, HexToCharTest)
{
    EXPECT_EQ(HexToChar(0xa), 'a');
    EXPECT_EQ(HexToChar(0x8), '8');
    EXPECT_EQ(HexToChar(0xf), 'f');
    EXPECT_EQ(HexToChar(0x1), '1');
    EXPECT_EQ(HexToChar(18), '\0');
    EXPECT_EQ(HexToChar(250), '\0');
}

TEST(UtilityTest, CharToHexTest)
{
    EXPECT_EQ(CharToHex('a'), 0xa);
    EXPECT_EQ(CharToHex('8'), 0x8);
    EXPECT_EQ(CharToHex('f'), 0xf);
    EXPECT_EQ(CharToHex('1'), 0x1);
    EXPECT_EQ(CharToHex('B'), 0xB);
    EXPECT_THROW(CharToHex('p'), std::invalid_argument);
    EXPECT_THROW(CharToHex('W'), std::invalid_argument);
}

TEST(UtilityTest, ValToHexTest)
{
    EXPECT_EQ(ValToHex(10, 8),       "000000000000000a");
    EXPECT_EQ(ValToHex(111, 8),      "000000000000006f");
    EXPECT_EQ(ValToHex(5284, 8),     "00000000000014a4");
    EXPECT_EQ(ValToHex(0, 8),        "0000000000000000");
    EXPECT_EQ(ValToHex(487, 8),      "00000000000001e7");
    EXPECT_EQ(ValToHex(222'222, 8),  "000000000003640e");
}

TEST(UtilityTest, HexToValTest)
{
    EXPECT_EQ(HexToVal("000000000000000a", 8), 10);
    EXPECT_EQ(HexToVal("000000000000006f", 8), 111);
    EXPECT_EQ(HexToVal("00000000000014a4", 8), 5284);
    EXPECT_EQ(HexToVal("0000000000000000", 8), 0);
    EXPECT_EQ(HexToVal("00000000000001e7", 8), 487);
    EXPECT_EQ(HexToVal("000000000003640e", 8), 222'222);
}

TEST(UtilityTest, RegNumToStrTest)
{
    EXPECT_STREQ(RegNumToStr(0).c_str(), "\x2$0");
    EXPECT_STREQ(RegNumToStr(10).c_str(), "\x2t2");
    EXPECT_STREQ(RegNumToStr(31).c_str(), "\x2ra");
    EXPECT_STREQ(RegNumToStr(34).c_str(), "\x2ip");
    EXPECT_THROW(RegNumToStr(100), std::invalid_argument);
}

TEST(UtilityTest, UnescapeTest)
{
    char data1[] = "}]";

    EXPECT_EQ(Unescape(data1, strlen(data1)), 1);
    EXPECT_STREQ(data1, "}");

    char data2[] = "}\x03";

    EXPECT_EQ(Unescape(data2, strlen(data2)), 1);
    EXPECT_STREQ(data2, "#");

    char data3[] = "}\x04";

    EXPECT_EQ(Unescape(data3, strlen(data3)), 1);
    EXPECT_STREQ(data3, "$");

    char data4[] = "11111    }\x04  11";

    EXPECT_EQ(Unescape(data4, strlen(data4)), 14);
    EXPECT_STREQ(data4, "11111    $  11");

    char data5[] = "";

    EXPECT_EQ(Unescape(data5, strlen(data5)), 0);
    EXPECT_STREQ(data5, "");
}

#pragma endregion UtilityTest

#pragma region PacketTest
TEST(PacketTest, CheckSumTest)
{
    Packet p1{"12456", 5};
    EXPECT_EQ(p1.CheckSum(), 2);

    Packet p2{"abthcd", 6};
    EXPECT_EQ(p2.CheckSum(), 102);

    Packet p3{"", 0};
    EXPECT_EQ(p3.CheckSum(), 0);

    Packet p4{"nlmksappdkkkf", 13};
    EXPECT_EQ(p4.CheckSum(), 113);
}
#pragma endregion PacketTest

#pragma region GeneralQueryTests
// тесты проверяют, что на общие запросы сервер отвечает как нужно
TEST_F(ServerFixture, HaltQueryTest)
{
    client.SendMess("$?#3f");

    EXPECT_STREQ(client.GetResp().c_str(), "+$S05#b8");
}

TEST_F(ServerFixture, qSymbolTest)
{
    client.SendMess("$qSymbol::#5b");

    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");
}

TEST_F(ServerFixture, qOffsetsTest)
{
    client.SendMess("$qOffsets#4b");

    EXPECT_STREQ(client.GetResp().c_str(), "+$Text=0;Data=0;Bss=0#04");
}

TEST_F(ServerFixture, qAttachedTest)
{
    client.SendMess("$qAttached#8f");

    EXPECT_STREQ(client.GetResp().c_str(), "+$0#30");
}

TEST_F(ServerFixture, qSupportedTest)
{
    client.SendMess("$qSupported#37");

    EXPECT_STREQ(client.GetResp().c_str(), "+$PacketSize=4096;#3e");
}
#pragma endregion GeneralQueryTests

#pragma region vPacketTests
TEST_F(ServerFixture, vMustReplyEmptyTest)
{
    client.SendMess("$vMustReplyEmpty#3a");

    EXPECT_STREQ(client.GetResp().c_str(), "+$#00");
}

TEST_F(ServerFixture, vContQueryTest)
{
    client.SendMess("$vCont?#49");

    EXPECT_STREQ(client.GetResp().c_str(), "+$vCont;c;C;s;S#62");
}

#pragma endregion vPacketTests

#pragma region RegistersTests
// тест для запроса на чтение и записи всех регистров
TEST_F(ServerFixture, AllRegTest)
{
    // заполняем вес регистры нулевым значением
    client.SendMess("$G00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000#47");

    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");

    // убеждаемся, что сервер все записал
    client.SendMess("$g#67");

    EXPECT_STREQ(client.GetResp().c_str(), "+$00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000#00");

    // поместили значение 15b3 в регистр zero
    client.SendMess("$P0=00000000000015b3#f8"); 
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");

    // проверили, что сервер записал регистр
    client.SendMess("$g#67");
    EXPECT_STREQ(client.GetResp().c_str(), "+$00000000000015b30000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000#3b");
}

// тест для запросов на чтение и запись единичного регситра
TEST_F(ServerFixture, SingleRegTest)
{
    // записываем в регистр v0 (номер 2) значение 22b
    client.SendMess("$P2=000000000000022b#f5");
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");

    client.SendMess("$p2#a2");
    EXPECT_STREQ(client.GetResp().c_str(), "+$000000000000022b#36");

    // теперь кладем в тот же регистр 0
    client.SendMess("$P2=0000000000000000#bf");
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");

    client.SendMess("$p2#a2");
    EXPECT_STREQ(client.GetResp().c_str(), "+$0000000000000000#00");
}

#pragma endregion RegistersTests

#pragma region MemoryTests
TEST_F(ServerFixture, MemoryTest1)
{
    // записываем в 4-х байтный блок по нулевому адресу нули
    client.SendMess("$M0,4:00000000#97");
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");
    // проверка, что сервер записал
    client.SendMess("$m0,4#fd");
    EXPECT_STREQ(client.GetResp().c_str(), "+$00000000#80");
    // записываем в блок новое значение
    client.SendMess("$M0,4:11111111#9f");
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");
    client.SendMess("$m0,4#fd");
    EXPECT_STREQ(client.GetResp().c_str(), "+$11111111#88");
}

TEST_F(ServerFixture, MemoryTest2)
{
    client.SendMess("$M80010010,8:0000000000000000#75");
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");
    // проверка, что сервер записал
    client.SendMess("$m80010010,8#5b");
    EXPECT_STREQ(client.GetResp().c_str(), "+$0000000000000000#00");
    // записываем в блок новое значение
    client.SendMess("$M80010010,8:8888888888888888#f5");
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");
    client.SendMess("$m80010010,8#5b");
    EXPECT_STREQ(client.GetResp().c_str(), "+$8888888888888888#80");
}

#pragma endregion MemoryTests

#pragma region BreakpointsTests
// тест проверяет, что на неподдерживаемые типы точек останова сервер отвечает пустой строкой
TEST_F(ServerFixture, UnsupportedBreakpointsTest)
{
    client.SendMess("$Z1,80010000,4#a0");
    EXPECT_STREQ(client.GetResp().c_str(), "+$#00");
    client.SendMess("$Z2,80010000,4#a1");
    EXPECT_STREQ(client.GetResp().c_str(), "+$#00");
    client.SendMess("$Z3,80010000,4#a2");
    EXPECT_STREQ(client.GetResp().c_str(), "+$#00");
    client.SendMess("$Z4,80010000,4#a3");
    EXPECT_STREQ(client.GetResp().c_str(), "+$#00");

    client.SendMess("$z1,80010000,4#c0");
    EXPECT_STREQ(client.GetResp().c_str(), "+$#00");
    client.SendMess("$z2,80010000,4#c1");
    EXPECT_STREQ(client.GetResp().c_str(), "+$#00");
    client.SendMess("$z3,80010000,4#c2");
    EXPECT_STREQ(client.GetResp().c_str(), "+$#00");
    client.SendMess("$z4,80010000,4#c3");
    EXPECT_STREQ(client.GetResp().c_str(), "+$#00");
}

TEST_F(ServerFixture, BpTest)
{
    // устанавливаем две точки останова
    client.SendMess("$Z0,80010000,4#9f");
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");

    client.SendMess("$Z0,80010014,4#a4");
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");

    // сервер должен сообщить о том, что была встречена точка останова
    client.SendMess("$c#63");
    EXPECT_STREQ(client.GetResp().c_str(), "+$S05#b8");

    // удаляем установленные точки останова
    client.SendMess("$z0,80010000,4#bf");
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");

    client.SendMess("$z0,80010014,4#c4");
    EXPECT_STREQ(client.GetResp().c_str(), "+$OK#9a");
}
#pragma endregion BreakpointsTests

int main(int argc, char **argv)
{
    ::testing::InitGoogleMock(&argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}