#include "server.hpp"

int main(int argc, char* argv[])
{
    int opt;
    const char *options = "p:f:";
    int port = 0;
    std::string file_name;
    while ((opt = getopt(argc, argv, options)) != -1)
    {
        switch(opt)
        {
            case 'p':
                port = atoi(optarg);
                break;
            case 'f':
                file_name = optarg;
                break;
            }
    }
    if (argc < 2)
    { 
        std::cout << "нужно ввести номер порта для подключения\n";
        std::cout << "опции вызова: -p номер_порта -f полное_имя_файла\n";
        exit(EXIT_FAILURE);
    }
    if (file_name.size() == 0)
    {
        file_name = "../tests/example_for_test";
    }
    load_elf_str ed;
    std::cout << "GDB сервер начал работу\n";
    GDBServer server;
    server.StartServer(port, file_name, ed);
    std::cout << "установлено соединение с GDB\n";
    std::cout << "информация об исполняемом файле\n";
    print_elf_data(ed);
    server.ThreadsHandler();
    server.StopServer();
    std::cout << "соединение прекращено\n";
    return 0;
}