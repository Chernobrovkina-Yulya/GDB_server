# gdb server
Этот проект содержит реализацию gdb сервера для эмулятора архитектуры MIPS64. Поддерживает [GDB Remote Serial Protocol](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Remote-Protocol.html).

## Как пользоваться

Предварительно необходимо установить gdb-multiarch, так как gdb по умолчанию не поддерживает MIPS64.

Соберите проект с помощью этих команд:

    mkdir build
    cd build
    cmake ..
    make
Появится файл `server`\
Запустите файл `server` и передайте в качестве аргументов номер порта для TCP соединения, а также полный путь до файла, который вы хотите отлаживать. Сервер выведет некоторые сведения о файле и будет ждать соединения с gdb.

    ./server -p 1234 -f ~/full_path/my_file

Запустите gdb-multiarch и передайте ему путь до отлаживаемого файла. Установите архитектуру на mips:isa64r3 и подключитесь к серверу

    gdb-multiarch ~/full-path-to-your-file
    (gdb) set arch mips:isa64r3
    (gdb) target remote server_ip_adress

После этого можно использовать gdb привычным образом.


## Про сборку исполняемого файла

Рекомендуется собирать исполняемый файл с помощью clang следующим образом:

    clang -target mips64-unknown-gnu -D__R3000__ -nostdlib -mcpu=mips -march=mips3 -mabi=n32 -mno-abicalls -mno-micromips -fuse-ld=lld -meabi gnu -Xlinker -emain -Xlinker -Ttext=0x80010000 -g <Имя файла>
