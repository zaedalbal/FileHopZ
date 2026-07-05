# FileHopZ

[Русский](README.md) | [English](README.en.md)

FileHopZ - это легковесный инструмент для прямой передачи файлов по локальной
сети. Проект ориентирован на простой сценарий: на одной машине запускается
receiver, на другой - sender, после чего файлы, директории и symlink передаются
напрямую без промежуточного сервера.

Проект использует собственный протокол передачи поверх UDP. FileHopZ сам
следит за доставкой данных: подтверждает полученные части, повторяет потерянные
пакеты и восстанавливает правильный порядок файлов на принимающей стороне.

## Передача и безопасность

Передаваемые данные шифруются после установления соединения. Для обмена ключами
используется X25519, а для шифрования и проверки целостности данных -
ChaCha20-Poly1305.

Важно: FileHopZ рассчитан на доверенную локальную сеть. Он шифрует содержимое
передачи, но сейчас не проверяет личность второй стороны. Это значит, что проект
не стоит использовать как замену SSH/SFTP или другим инструментам с полноценной
аутентификацией, особенно в недоверенных сетях.

## Требования

* Компилятор с поддержкой **C++23**
* **CMake 3.25+**
* **Git**
* **Boost 1.82+**
* **OpenSSL 3.0+**
* **spdlog 1.10+**
* **CLI11 2.3+**

### Установка зависимостей

**Для Ubuntu/Debian:**

```bash
sudo apt install git cmake g++ libboost-all-dev libssl-dev libspdlog-dev libcli11-dev
```

**Для Fedora:**

```bash
sudo dnf install git cmake gcc-c++ boost-devel openssl-devel spdlog-devel cli11-devel
```

**Для Arch Linux / Manjaro:**

```bash
sudo pacman -S git cmake base-devel boost openssl spdlog cli11
```

(если ваша версия CMake < 3.25, вы можете попробовать поменять CMakeLists.txt)

## Сборка

```bash
mkdir build
cd build

cmake -DCMAKE_BUILD_TYPE=Release ..

cmake --build .
```

После успешной сборки в каталоге `build` будет создан исполняемый файл

## Установка

```bash
./install.sh
```

Скрипт собирает Release-версию и устанавливает `filehopz` в
`/usr/local/bin/filehopz`. Если для записи в `/usr/local/bin` нужны права
администратора, скрипт использует `sudo`.

Для удаления установленного бинарника:

```bash
./uninstall.sh
```

## Использование

```bash
Sender:   filehopz send <address> <port> <file/directory>
Receiver: filehopz recv [--auto-accept] <port> <output_directory>
```

Подробная справка доступна через `filehopz --help`.

После запуска receiver покажет локальные IPv4 адреса с портом в формате
`address:port`, которые можно передать в sender. Перед приёмом receiver выведет
размер передаваемых данных и попросит подтверждение: `y` — принять, `n` —
отказаться (для автоматического принятия можно выставить флаг --auto-accept).

Пример локального запуска:

```bash
filehopz recv --auto-accept 9000 ./received
filehopz send 192.168.0.101 9000 ./my-directory
```

## Ограничения

FileHopZ сейчас рассчитан на прямую передачу внутри доверенной локальной сети.
Это не P2P-система с NAT traversal, discovery или relay-сервером.

Проект не позиционируется как production-ready security tool: в нем есть
шифрование транспорта, но нет аутентификации peer'а, PKI, pinning ключей или
защиты от активной подмены handshake.
