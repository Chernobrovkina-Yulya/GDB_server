#include "server.hpp"

void GDBServer::StartServer(int port, std::string &elf_name, load_elf_str &ed)
{
    static char elf_image[MAX_ELF_IMAGE_SIZE];
    // загрузка исполняемого файла
    FILE *elf;
    long elf_size;
    // открыть двоичный файл для чтения
    elf = fopen(elf_name.c_str(), "rb");
    if(elf == NULL)
    {
        perror("ошибка открытия файла");
        exit(EXIT_FAILURE);
    }
    // чтение файла
    elf_size = fread(&elf_image, 1, MAX_ELF_IMAGE_SIZE, elf);
    if(elf_size <= 0)
    {
        perror("ошибка чтения файла");
        fclose(elf);
        exit(EXIT_FAILURE);
    }
    fclose(elf);

    // инициализируем эмулятор и загружаем в него файл
    uemu_init();
    uemu_dsp(7, &elf_image, elf_size, &ed);

    // создание сокета сервера
    server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    if (server_sock_fd == -1)
    {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr = {0};

    struct in_addr ip_to_num;
    inet_pton(AF_INET, "127.0.0.1", &ip_to_num);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = ip_to_num;
    server_addr.sin_port = htons((uint16_t)port);

    if (bind(server_sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock_fd, 1) == -1)
    {
        perror("listen() failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    client_sock_fd = accept(server_sock_fd, (struct sockaddr *)&client_addr, &client_addr_size);
    if (client_sock_fd == -1)
    {
        perror("accept() failed");
        exit(EXIT_FAILURE);
    }
    is_connection_open = true;
    em_state = EmulatorState::INTERRUPTED;
    pthread_mutex_lock(&client_sock_m);
}

void GDBServer::StopServer()
{
    close(client_sock_fd);
    close(server_sock_fd);
}