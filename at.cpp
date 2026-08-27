#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h> // Подключаем заголовок для работы с терминалами
#include <errno.h>
#include <cstring>
char buffer[1024];
int buf_len = 0;// Текущая длина данных в осн. буфере

//#include <string.h>

typedef struct {
    const char* request;
    const char* response;
} AtCommand;

static const AtCommand cmd_db[] = {
    {"AT",      "OK"},
    {"ATE0",    "OK"},
    {"ATE1",    "OK"},
    {"ATI",     "Manufacturer: MyServer\rModel: VirtualModem\rRevision: 1.0\rOK"},
    {"AT+CPIN", "+CPIN: READY\rOK"},
    {"AT+COPS", "+COPS: 0,0,\"VirtualNet\",7\rOK"},
    {"DEFAULT", "ERROR"}
};

#define CMD_COUNT (sizeof(cmd_db) / sizeof(cmd_db[0]) - 1)

AtCommand findCmd(const char* cmd) {
    if (cmd == NULL || cmd[0] == '\0') {
        return cmd_db[CMD_COUNT];
    }

    for (size_t i = 0; i < CMD_COUNT; i++) {
        if (strcmp(cmd, cmd_db[i].request) == 0) {
            return cmd_db[i];
        }
    }

    return cmd_db[CMD_COUNT];
}
void sendAT(int fd, AtCommand at) {
    if (at.response != nullptr) {
        
        printf("[RX] Клиент прислал : %s\n", at.request);
        printf("[TX] Сервер отвечает : %s\r\n", at.response);
        
        // Отправка реального ответа в порт
        write(fd, at.response, strlen(at.response));
        write(fd, "\r\n", 2); // Стандартное завершение AT-ответа
    }
}


int main (int argc, char *argv[]) {
    const char * device= "/dev/ttyUSB0";
    if (argc >= 2) {
        printf("Использование: %s <путь_к_устройству>\n", argv[1]);
        device= argv[1];
    }
    
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd < 0) {
        perror("Ошибка открытия порта");
        return 1;
    }

    printf("Порт успешно открыт. Можно работать в фоне без зависаний.\n");
    
struct termios tty;

tcgetattr(fd, &tty); // Получить текущие настройки

cfsetispeed(&tty, B115200); // Скорость на вход
cfsetospeed(&tty, B115200); // Скорость на выход

tty.c_cflag |= (CLOCAL | CREAD); // Включить прием, локальное подключение
tty.c_cflag &= ~PARENB;          // Без четности
tty.c_cflag &= ~CSTOPB;          // 1 стоп-бит
tty.c_cflag &= ~CSIZE;           // Очистить маску размера
tty.c_cflag |= CS8;              // 8 бит данных
tty.c_cflag |= CRTSCTS;          // Включить аппаратное управление потоком RTS/CTS

tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IUCLC | IXANY);
// Пояснение отключенных флагов:
    // IGNBRK, BRKINT, PARMRK: Обработка ошибок четности и разрыва линии
    // ISTRIP: Отсечение 8-го бита (нужно для 8-битных данных модема)
    // INLCR, IGNCR, ICRNL: Преобразование \n и \r
    // IXON, IXOFF, IXANY: Программное управление потоком (Ctrl-S/Ctrl-Q)
// Перевод в необработанный (raw) режим для передачи AT-команд
tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
tty.c_oflag &= ~OPOST;

if (tcsetattr(fd, TCSANOW, &tty) != 0)
{
        perror("Ошибка применения настроек");
        close(fd);
        return 1;
}

while (1){
    //проверяем  сколько в буфере свободного места
    int freeZone=sizeof(buffer)-buf_len-1;
    ssize_t n = read(fd, buffer+buf_len, freeZone);
    if(n>0){
        char *pos0=buffer;
        char *posEND=NULL;
        buf_len+=n;          //длинна осн буфера
        
        

        //Проверка буфера на команды!
        // ищем /r
        for(int i=0;i<buf_len;i++){
            if(buffer[i]=='\r'){
                buffer[i]='\0';
                posEND=&buffer[i];
                AtCommand at=findCmd(pos0);
                sendAT(fd,at);
                pos0=posEND+1;
            } 
        }
        //смещение на обработанное количество символов
        int offset = buf_len - (pos0 - buffer);
            if (offset > 0 && pos0 > buffer) {
                memmove(buffer, pos0, offset);
            }
            buf_len = offset; // Обновляем длину 
    }
    else if(n<0)
    {
            // O_NONBLOCK возвращает EAGAIN, если данных прямо сейчас нет. Это нормально.
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
                perror("Ошибка чтения из порта");
                break; // Выход из цикла при реальной ошибке
        }

    }
    
    
    
    usleep(10000); // 10 миллисекунд
    
    
    
}

    

    close(fd);
    return 0;
}