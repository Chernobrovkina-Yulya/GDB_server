#include <array>
#include <stdexcept>

#include "server.hpp"

// возвращает шестнадцатеричный символ соответствующий переданному значению
// d - шестнадцптеричное число. не шестнадцатеричное число вернет '\0'
char HexToChar(uint8_t d)
{
    const char map[] = "0123456789abcdef"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                       "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
    return map[d];
}

// метод дает значение шестнадцатеричного символа
// в случае некорректного символа бросит исключение
uint8_t CharToHex(int ch)
{
    if(!((ch >= 'a') && (ch <= 'f') || (ch >= '0') && (ch <= '9') || (ch >= 'A') && (ch <= 'F')))
        throw std::invalid_argument("получен некорректный символ\n");

    return ((ch >= 'a') && (ch <= 'f')) ? ch - 'a' + 10:
           ((ch >= '0') && (ch <= '9')) ? ch - '0': ch - 'A' + 10;
}

//  метод преобразует значение регистра в строку из шестнадцатеричных цифр
//  rsp протокол требует представлять каждый байт регистра двумя шестн. цифрами
//  считаем, что регистры записаны в BE (big endian)

std::string ValToHex(uint64_t val, size_t numbytes)
{
    std::string s(2 * numbytes, '0');
    for (int n = numbytes -1 ; n >= 0; --n)
    {
        unsigned char byte = val & 0xff;
        s[2 * n] = HexToChar((byte >> 4) & 0xf);
        s[2 * n + 1] = HexToChar(byte & 0xf);
        val /= 256;
    }
    return s;
}

//  метод по строке шестн цифр, которые передает GDB, возвращает значение

uint64_t HexToVal(const char * s, size_t numbytes)
{
    uint64_t res = 0;
    for (size_t n = 0; n < numbytes; ++n)
    {
        res = (res << 4) | CharToHex(s[n * 2]);
        res = (res << 4) |CharToHex(s[n * 2 + 1]);
    }
    return res;
}

// метод, сопоставляющий позиции регистра его имя

std::string RegNumToStr(int num)
{
    if(num >= NUM_OF_CP0_REGS + NUM_OF_CP1_REGS)
        throw std::invalid_argument("некорректный номер регистра");
        
    std::array<std::string, NUM_OF_CP0_REGS + NUM_OF_CP1_REGS> regs =
        {"$0", "at", "v0", "v1", "a0",
         "a1", "a2", "a2", "t0", "t1",
         "t2", "t3", "t4", "t5", "t6",
         "t7", "s0", "s1", "s2", "s3",
         "s4", "s5", "s6", "s7", "t8",
         "t9", "k0", "k1", "gp", "sp",
         "fp", "ra", "hi", "lo", "ip",
         "f0", "f1", "f2", "f3", "f4",
         "f5", "f6", "f7", "f8", "f9",
         "f10", "f11", "f12", "f13", "f14",
         "f15", "f16", "f17", "f18", "f19",
         "f20", "f21", "f22", "f23", "f24",
         "f25", "f26", "f27", "f28", "f29",
         "f30", "f31"};
    std::string reg_name = regs.at(num);
    if(num < 45)
        reg_name.insert(0, 1, 2);         //  в начало строки требуется добавить длину строки
    else    
        reg_name.insert(0, 1, 3);

    return reg_name;
}

int Unescape(char * data, size_t len)
{
    size_t from_offset = 0;
    size_t to_offset   = 0;

    while(from_offset < len)
    {
        if(data[from_offset] == '}')
        {
            ++from_offset;
            data[to_offset] = data[from_offset] ^ 0x20;
        }
        else
            data[to_offset] = data[from_offset];

        ++from_offset;
        ++to_offset;
    }
    if(to_offset < len)
        data[to_offset] = '\0';
    return to_offset;
}

void print_elf_data(load_elf_str const &ed) 
{
	std::cout << "\tРазмер файла                : " <<	ed.size << '\n';
	std::cout << "\tEndian (0-LE, 1-BE)         : " << 	ed.endian << '\n';
	std::cout << "\tАдрес точки входа _start    : " << ed.start << '\n';
	std::cout << "\tАдрес функции main          : 0x" << ed.main << '\n';
	std::cout << "\tВызов API (0-GOT, 1-PLT)    : " <<	ed.gotplt << '\n';
	std::cout << "\tКоличество записей в dynsym : " << 	ed.dynsyms << '\n';
	std::cout << "\tКоличество записей в symtab : " << 	ed.symbols << '\n';
	std::cout << "\tАдрес данных dynsym         : " <<	ed.d_addr << '\n';
	std::cout << "\tАдрес данных symtab         : " << 	ed.s_addr << '\n';
}